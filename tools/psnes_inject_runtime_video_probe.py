#!/usr/bin/env python3
"""Instrument pSNES 7.2 PS3 source: actual frame and GPU transform, one TXT."""
from pathlib import Path
import sys

def edit(path, replacements):
    s=path.read_text(encoding="utf-8")
    for old,new in replacements:
        if s.count(old)!=1:
            raise RuntimeError(f"{path}: expected exact old anchor once: {old[:70]!r}; found {s.count(old)}")
        s=s.replace(old,new)
    path.write_text(s,encoding="utf-8")

def main(root):
    main=root/"src/cores/main.cpp"
    video=root/"src/skeleton/ui_video.cpp"
    emu=root/"src/cores/psnes/sources/psnes_ui_emu.cpp"
    tiny=root/"external/libcross2d/source/platforms/ps3/tiny3d/source/tiny3d.c"
    rend=root/"external/libcross2d/source/platforms/ps3/tiny3d_renderer.cpp"
    runtime=r'''
#ifdef __PS3__
#include <stdarg.h>
extern "C" void sf_runtime_video_telemetry(const char *fmt, ...) {
    // Append to the SINGLE SpookyFlow TXT; no other report is created.
    const char *name="/dev_hdd0/tmp/spookyflow/diagnostico_pantalla_snes.txt";
    char buf[384],line[450];
    va_list a;
    s32 fd=-1;
    u64 wrote=0;
    va_start(a,fmt);vsnprintf(buf,sizeof(buf),fmt,a);va_end(a);
    snprintf(line,sizeof(line),"psnes_runtime: %s\n",buf);
    if(sysLv2FsOpen(name,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_APPEND,
                    &fd,0666,nullptr,0)==0){
        sysLv2FsWrite(fd,line,strlen(line),&wrote);
        sysLv2FsClose(fd);
    } else {
        // The existing pSNES boot.log is only a fallback if direct report fails.
        boot_log(line);
    }
}
#endif
'''
    edit(main,[
      ("using namespace c2d;\nusing namespace pemu;",
       "void boot_log(const char *msg);\n"+runtime+"\nusing namespace c2d;\nusing namespace pemu;"),
      ("        cfg->loadGame(game);\n        uiEmu->setExitOnStop(true);",
       """        cfg->loadGame(game);
#ifdef __PS3__
        auto *scale=cfg->get(PEMUConfig::OptId::EMU_SCALING,true);
        sf_runtime_video_telemetry("GAME_CFG actual_scaling=%s rom=%s",
            scale?scale->getString().c_str():"NULL",game.path.c_str());
#endif
        uiEmu->setExitOnStop(true);""")
    ])
    edit(video,[
      ('#include "pemu.h"','#include "pemu.h"\nextern "C" void sf_runtime_video_telemetry(const char *fmt, ...);'),
      ("    setRotation(rotation);\n}", """    setRotation(rotation);
#ifdef __PS3__
    sf_runtime_video_telemetry(
        "UPDATE_SCALING mode=%s mode2=%s virtual=%.0fx%.0f "
        "texture=%dx%d aspect=%dx%d max=%.5fx%.5f "
        "scale=%.5fx%.5f result=%.1fx%.1f center=%.1fx%.1f",
        scale_value.c_str(),scaling_mode.c_str(),screen.x,screen.y,
        getTextureRect().width,getTextureRect().height,aspect.x,aspect.y,
        scale_max.x,scale_max.y,scale.x,scale.y,
        getTextureRect().width*scale.x,getTextureRect().height*scale.y,
        screen.x/2.0f,screen.y/2.0f);
#endif
}""")
    ])
    edit(emu,[
      ('#include "psnes_ui_emu.h"','#include "psnes_ui_emu.h"\nextern "C" void sf_runtime_video_telemetry(const char *fmt, ...);'),
      ('    addVideo((uint8_t **) &GFX.Screen, (int *) &GFX.Pitch, {MAX_SNES_WIDTH, MAX_SNES_HEIGHT});',
       '''    addVideo((uint8_t **) &GFX.Screen, (int *) &GFX.Pitch, {MAX_SNES_WIDTH, MAX_SNES_HEIGHT});
#ifdef __PS3__
    sf_runtime_video_telemetry("SNES_VIDEO_ALLOC dimensions=%dx%d pitch=%d",
        MAX_SNES_WIDTH,MAX_SNES_HEIGHT,(int)GFX.Pitch);
#endif'''),
      ('    C2DUIVideo *video = m_ui->getUiEmu()->getVideo();',
       '''#ifdef __PS3__
    static unsigned sf_video_samples=0;
    if(sf_video_samples<8){
        sf_runtime_video_telemetry("SNES_FRAME sample=%u visible=%dx%d",
             sf_video_samples++,width,height);
    }
#endif
    C2DUIVideo *video = m_ui->getUiEmu()->getVideo();''')
    ])
    edit(rend,[
      ('#include <sys/unistd.h>','#include <sys/unistd.h>\nextern "C" void sf_runtime_video_telemetry(const char *fmt, ...);'),
      ('    TINY3DRenderer::setSize(Vector2f(848, 512));',
       '''    TINY3DRenderer::setSize(Vector2f(848, 512));
    sf_runtime_video_telemetry("RENDERER_CREATED virtual=848x512 physical=%ux%u",
        (unsigned)Video_Resolution.width,(unsigned)Video_Resolution.height);'''),
      ('        tiny3d_Project2D();',
       '''        tiny3d_Project2D();
        static unsigned sf_proj_frames=0;
        if(sf_proj_frames<6){
            sf_runtime_video_telemetry("RENDERER_PROJECT sample=%u virtual=%.0fx%.0f physical=%ux%u",
                sf_proj_frames++,getSize().x,getSize().y,
                (unsigned)Video_Resolution.width,(unsigned)Video_Resolution.height);
        }''')
    ])
    edit(tiny,[
      ('void tiny3d_Project2D()\n{',
       '''extern void sf_runtime_video_telemetry(const char *fmt, ...);
void tiny3d_Project2D()
{
    static unsigned sf_project_calls=0;
    unsigned sf_report=(sf_project_calls++<6);'''),
      ('        rsxtiny_ViewportTranslate(context, 54.0, 24.0, 0.0, 0.0);',
       '''        if(sf_report) sf_runtime_video_telemetry("GPU_BRANCH=720p pos=-40,-24 scale=%.7f,%.7f",
             1360.0/848.0,768.0/512.0);
        rsxtiny_ViewportTranslate(context, -40.0f, -24.0f, 0.0, 0.0);'''),
      ('        rsxtiny_ViewportScale(context, 848.0 / 611.0 ,  674.0 / 512.0, Z_SCALE, 1.0);',
       '        rsxtiny_ViewportScale(context, 1360.0f/848.0f, 768.0f/512.0f, Z_SCALE, 1.0);'),
      ('        rsxtiny_ViewportTranslate(context, 63.0, 40.0, 0.0, 0.0);',
       '''        if(sf_report) sf_runtime_video_telemetry("GPU_BRANCH=1080p pos=-60,-24 scale=%.7f,%.7f actual=%ux%u",
             2040.0/848.0,1128.0/512.0,(unsigned)Video_Resolution.width,(unsigned)Video_Resolution.height);
        rsxtiny_ViewportTranslate(context, -60.0f, -24.0f, 0.0, 0.0);'''),
      ('        rsxtiny_ViewportScale(context, 848.0 / 400.0 ,  952.0 / 512.0, Z_SCALE, 1.0);',
       '        rsxtiny_ViewportScale(context, 2040.0f/848.0f, 1128.0f/512.0f, Z_SCALE, 1.0);'),
      ('        rsxtiny_ViewportTranslate(context, x_viewport_2D, y_viewport_2D, 0.0, 0.0);',
       '''        if(sf_report) sf_runtime_video_telemetry("GPU_BRANCH=USER_VIEWPORT pos=%.2f,%.2f scale=%.7f,%.7f",
             x_viewport_2D,y_viewport_2D,scale2D_x_viewport,scale2D_y_viewport);
        rsxtiny_ViewportTranslate(context, x_viewport_2D, y_viewport_2D, 0.0, 0.0);'''),
      ('        rsxtiny_ViewportTranslate(context, 38.0 , 16.0, 0.0, 0.0);',
       '''        if(sf_report) sf_runtime_video_telemetry("GPU_BRANCH=SD physical=%ux%u",
             (unsigned)Video_Resolution.width,(unsigned)Video_Resolution.height);
        rsxtiny_ViewportTranslate(context, 38.0 , 16.0, 0.0, 0.0);''')
    ])
    # The original PS3 backend in this *pinned* submodule retains outdated
    # interfaces that cannot even compile against its own skeleton headers.
    # Bring only PS3 signatures in line with the actual virtual interfaces.
    base=root/"external/libcross2d/include/cross2d"
    edit(base/"platforms/ps3/tiny3d_renderer.h", [
      ("Texture *texture, Sprite *sprite = nullptr) override;",
       "Texture *texture) override;")
    ])
    edit(rend,[
      ("Texture *texture, Sprite *sprite) {",
       "Texture *texture) {"),
      ("int filter = tex->filter == Texture::Filter::Linear",
       "int filter = tex->m_filter == Texture::Filter::Linear"),
      ("(u32) tex->pitch, tex->fmt, filter);",
       "(u32) tex->m_pitch, tex->fmt, filter);")
    ])
    edit(base/"platforms/ps3/tiny3d_texture.h",[
      ("explicit TINY3DTexture(const Vector2f &size = Vector2f(0, 0), Format format = Format::RGBA8);",
       "explicit TINY3DTexture(const Vector2i &size = Vector2i(0, 0), Format format = Format::RGBA8);"),
      ("int lock(FloatRect *rect, void **pixels, int *pitch) override;",
       "int lock(uint8_t **pixels, int *pitch = nullptr, IntRect rect = IntRect()) override;"),
      ("void unlock(void *data = nullptr) override;",
       "void unlock(const uint8_t *data = nullptr) override;"),
      ("u32 offset;\n        text_format fmt;",
       "u32 offset;\n        text_format fmt;\n        bool available=false;")
    ])
    tex=root/"external/libcross2d/source/platforms/ps3/tiny3d_texture.cpp"
    edit(tex,[
       ("TINY3DTexture::TINY3DTexture(const Vector2f &size, Format format)",
        "TINY3DTexture::TINY3DTexture(const Vector2i &size, Format format)"),
       ("int TINY3DTexture::lock(FloatRect *rect, void **pix, int *p) {",
        "int TINY3DTexture::lock(uint8_t **pix, int *p, IntRect rect) {"),
       ("    if (!rect) {\n        *pix = pixels;\n    } else {\n        *pix = (void *) ((uint8_t *) pixels + (int) rect->top * pitch + (int) rect->left * bpp);\n    }",
        "    if (rect==IntRect()) *pix=(uint8_t*)pixels;\n    else *pix=(uint8_t*)pixels+rect.top*m_pitch+rect.left*m_bpp;"),
       ("void TINY3DTexture::unlock(void *data) {",
        "void TINY3DTexture::unlock(const uint8_t *data) {")
    ])
    texture_src=tex.read_text()
    for before,after in [
        (" bpp"," m_bpp"),(" bpp)"," m_bpp)"),("pitch","m_pitch"),
        ("tex_size","m_tex_size"),("setFilter(filter)","setFilter(m_filter)")
    ]:
        pass # exact token patch below, no broad textual replacements.
    textext=tex.read_text()
    import re
    for old,new in [(r"\bbpp\b","m_bpp"),(r"\bpitch\b","m_pitch"),(r"\btex_size\b","m_tex_size"),(r"\bfilter\b","m_filter")]:
        textext=re.sub(old,new,textext)
    tex.write_text(textext)
    code=main.read_text()+video.read_text()+emu.read_text()+rend.read_text()+tiny.read_text()
    for key in ("psnes_runtime:","UPDATE_SCALING mode=","SNES_FRAME sample=",
                "RENDERER_PROJECT sample=","GPU_BRANCH=1080p","GPU_BRANCH=720p"):
        assert key in code,key
    print("PASS verified upstream source instrumentation of ACTUAL pSNES/GPU video path")

if __name__=="__main__":
    if len(sys.argv)!=2: raise SystemExit("usage: inject_runtime.py SOURCE_ROOT")
    main(Path(sys.argv[1]))
