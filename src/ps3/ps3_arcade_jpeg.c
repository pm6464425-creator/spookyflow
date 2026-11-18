/* ARCADE ONLY: turn Libretro flyer PNG into small baseline JPEG for PS3.
 * Runs in the existing COVER download worker and never changes game ROMs.
 * Original aspect ratio is contained (never stretched/cropped).
 */
#include <pngdec/pngdec.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ps3_arcade_jpeg.h"
#include "ps3_jpeg_soft.h"
#include "ps3_platform.h"

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define ARC_W 320
#define ARC_H 448
#define ARC_JPEG_MAX 184320u  /* <=180 KiB per downloaded cover */
typedef struct { uint8_t *buf;uint32_t cap,n;int overflow; } OutBuf;
static void write_bytes(void *ctx,void *data,int size)
{
    OutBuf *w=(OutBuf*)ctx;
    if(size<0)return;
    if(w->overflow || (unsigned)size>w->cap-w->n){w->overflow=1;return;}
    memcpy(w->buf+w->n,data,(unsigned)size);
    w->n+=(unsigned)size;
}
static int read_pixel(const uint8_t *data,int pitch,int w,int h,int argb,
                      int x,int y,uint8_t *rgb)
{
    const uint8_t *p;
    if(x<0)x=0;if(x>=w)x=w-1;
    if(y<0)y=0;if(y>=h)y=h-1;
    p=data+(size_t)y*pitch+(size_t)x*4u;
    if(argb){
        /* pngdec's ARGB buffer is laid out A,R,G,B. */
        rgb[0]=p[1];rgb[1]=p[2];rgb[2]=p[3];
        return p[0];
    }
    rgb[0]=p[0];rgb[1]=p[1];rgb[2]=p[2];
    return p[3];
}
int ps3_arcade_jpeg_compact(void *ctx,const uint8_t *input,uint32_t n,
                            uint8_t **out,uint32_t *out_len)
{
    pngData png;
    uint8_t *rgba=NULL,*canvas=NULL,*jpeg=NULL;
    int w=0,h=0,pitch=0,argb=0;
    int new_w,new_h,x0,y0,x,y,quality,rc=-1;
    double scale;
    OutBuf ob;
    (void)ctx;
    if(!out || !out_len || !input || n<24 || n>3u*1024u*1024u)return -1;
    *out=NULL;*out_len=0;
    memset(&png,0,sizeof png);
    if(n>=8 && memcmp(input,"\x89PNG\r\n\x1a\n",8)==0){
        int ok;
        ps3_decode_lock();
        ok=pngLoadFromBuffer((uint8_t*)input,n,&png);
        ps3_decode_unlock();
        if(ok || !png.bmp_out){free(png.bmp_out);return -2;}
        w=(int)png.width;h=(int)png.height;
        rgba=(uint8_t *)png.bmp_out;
        pitch=(int)png.pitch;
        argb=1;
    }else if(n>=3 && input[0]==0xff && input[1]==0xd8 && input[2]==0xff){
        if(ps3_jpeg_soft_decode(input,n,2048,&w,&h,&rgba)!=0 || !rgba)return -2;
        pitch=w*4;
    }else return -2;
    if(w<16 || h<16 || w>2048 || h>2048 || pitch<w*4)goto cleanup;
    canvas=(uint8_t *)malloc((size_t)ARC_W*ARC_H*3u);
    jpeg=(uint8_t *)malloc(ARC_JPEG_MAX);
    if(!canvas || !jpeg)goto cleanup;
    for(y=0;y<ARC_H;y++)for(x=0;x<ARC_W;x++){
        uint8_t *dst=canvas+((size_t)y*ARC_W+x)*3u;
        dst[0]=25;dst[1]=24;dst[2]=30;
    }
    scale=fmin((double)ARC_W/(double)w,(double)ARC_H/(double)h);
    new_w=(int)((double)w*scale+0.5);
    new_h=(int)((double)h*scale+0.5);
    if(new_w<1)new_w=1;
    if(new_h<1)new_h=1;
    if(new_w>ARC_W)new_w=ARC_W;
    if(new_h>ARC_H)new_h=ARC_H;
    x0=(ARC_W-new_w)/2;y0=(ARC_H-new_h)/2;
    for(y=0;y<new_h;y++){
        double sy=((double)y+0.5)*(double)h/new_h-0.5;
        int yb=(int)floor(sy);
        double fy=sy-yb;
        for(x=0;x<new_w;x++){
            double sx=((double)x+0.5)*(double)w/new_w-0.5;
            int xb=(int)floor(sx);
            double fx=sx-xb;
            int i,j,k;
            uint8_t px[4][3],a[4],*dst=canvas+((size_t)(y+y0)*ARC_W+x+x0)*3u;
            a[0]=read_pixel(rgba,pitch,w,h,argb,xb,yb,px[0]);
            a[1]=read_pixel(rgba,pitch,w,h,argb,xb+1,yb,px[1]);
            a[2]=read_pixel(rgba,pitch,w,h,argb,xb,yb+1,px[2]);
            a[3]=read_pixel(rgba,pitch,w,h,argb,xb+1,yb+1,px[3]);
            for(k=0;k<3;k++){
                double v00=px[0][k]*a[0]/255.0+25.0*(1.0-a[0]/255.0);
                double v10=px[1][k]*a[1]/255.0+25.0*(1.0-a[1]/255.0);
                double v01=px[2][k]*a[2]/255.0+25.0*(1.0-a[2]/255.0);
                double v11=px[3][k]*a[3]/255.0+25.0*(1.0-a[3]/255.0);
                double v=(1-fy)*((1-fx)*v00+fx*v10)+fy*((1-fx)*v01+fx*v11);
                dst[k]=(uint8_t)(v<0?0:v>255?255:v+0.5);
            }
        }
    }
    for(quality=78;quality>=50;quality-=14){
        ob.buf=jpeg;ob.cap=ARC_JPEG_MAX;ob.n=0;ob.overflow=0;
        if(stbi_write_jpg_to_func(write_bytes,&ob,ARC_W,ARC_H,3,canvas,quality) &&
           !ob.overflow && ob.n>256 && ob.n<=ARC_JPEG_MAX){
            *out=jpeg;*out_len=ob.n;jpeg=NULL;rc=0;break;
        }
    }
cleanup:
    free(rgba); free(canvas); free(jpeg);
    return rc;
}
