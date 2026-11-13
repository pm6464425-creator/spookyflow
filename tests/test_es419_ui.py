#!/usr/bin/env python3
"""Ten independent safety reviews of the es-419 ONLY-text edition.

The baseline SHA-1 values are exact Git blob IDs from the hardware-tested
SpookyFlow V15.11 release (GitHub commit 1f45ca4de2b7bbcbc54f8f2d16d3003ba70bb10c).
No modified font, geometry, stage, mount, cover, sound or controller assets
are permitted. This test intentionally avoids modifying the functional code
and runs before releasing the PKG. Real PS3 rendering is still a separate test.
"""
from __future__ import annotations
from pathlib import Path
import subprocess
import re

BASELINE = {
    "src/core/font.c": "0ad3b1a90e325599a6e8271ae49f443a3bac3d55",
    "src/core/library.c": "a9c29d07d2dc4cca179aec4460ff9620bd29b19a",
    "src/core/font.h": "f689789494bc00ca6faf621d469bbbd36edba257",
    "src/core/font_data.c": "a5135f6e296147299825b02831fe08af7e830bcb",
    "src/core/draw.c": "726a8c85e1300900aecff67960f29c2c1fa0a55a",
    "src/core/draw.h": "51a4b2658b2fd9372eb3cbe52599fc291d7d6eae",
    "src/core/layout.c": "1e49e5840b04e86536062666a2c06cf29b0caca2",
    "src/core/layout.h": "0d6b7a16686c0aee4825e2b1f59660cc87e2fd8b",
    "src/core/theme.h": "b8fd888e7b932806360332c06a217b205612c436",
    "src/core/icons_data.c": "4f1e27f3dc167252deb2b003a11a62ecd0c9b93b",
    "src/core/images_data.c": "067c89931794b569616c348e4ac3df7814b88c0f",
    "src/core/sounds_data.c": "444563ab23dfa1f218a736a746aa5f466ea1de45",
    "src/core/stage_data.c": "615bcda132f3776f260798e277439264a255357c",
    "src/core/music.c": "f876ac3b41e599194abc5e236dbb007be48f5faf",
    "src/core/anim.c": "dd68962bbb1e72ed5cffa41d9339f3e91ee255ec",
    "src/ps3/render_tiny3d.c": "416510cc43e24552c986a89c3a082d9df74e7357",
    "src/core/input.c": "311591ef1bd967377fb7faa26a85dda50555a00c",
    "src/ps3/main.c": "a835b3a4cd4f390afd65a26d105369eea642a875",
    "src/ps3/sf_cobra_mount.c": "8fcbf21818b4dbee2dc0314fcb53fc4c25b30b1b",
    "src/ps3/ps3_webman.c": "333f9d3fe75f3ebbdc0840ddb696385306919eca",
    "src/core/coverart.c": "31d829e7acea22bf0df7340a4b45c3a6f681fc52",
    "src/core/ps1_name_match.c": "b5ea816d0c4d978db13a0a1c89452f56b7f78e9c",
    "src/core/scanner.c": "1cd7dc2e56528d61d1e8bf6d86d0877e6347dd37",
    "src/ps3/ps3_covers.c": "8236c5c91cca655fd5cdc8448db62be1b779a888",
    "src/ps3/sf_bdiso_ntfs.c": "2865587c298aa3d4adba399d409959875606cb88",
    "src/ps3/sf_bdiso_art.c": "eaad9d89a02518b923409d0b4cf584b8f989f56e",
    "sfo.xml": "01b2fd233bc1a6d16f6918a179b3681e86a80222"
}
def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")
def unchanged(paths):
    return all(
      subprocess.check_output(["git","hash-object",p], text=True).strip()==BASELINE[p]
      for p in paths
    )
menu=read("src/core/menu.c")
settings=read("src/core/settings.c")
ui=read("src/core/ui.c")
app=read("src/core/app.c")
font=read("src/core/font_data.c")
library=read("src/core/library.c")
glyphs=font.split("static const FontGlyph font_big_glyphs")[0]
cps=set(int(z,16) for z in re.findall(r"\{\s*0x([0-9a-fA-F]{4})\s*,",glyphs))
menu_targets={
 "Apariencia","Música y sonido","Juegos y biblioteca","Sistema",
 "Inicio automático","Emulador","Modo sin conexión","Config. de PS2",
 "Ocultar juego","Mostrar juegos ocultos","Solo juegos de PS3",
 "Efectos de sonido","Volumen de música","Música aleatoria",
 "Fondo","Color de interfaz","Disco","Mostrar info. del juego",
 "Estuches de juegos","Reflejos","Luz de estuches","Modo de reposo",
 "Descargar carátulas faltantes",'Ignorar \\"The\\" al ordenar',
 "Fijar arriba en XMB","Volver a escanear","Acerca de",
 "Ocultar interfaz","Almacenamiento","Salir al XMB","Apagar PS3"
}
# 1: Identical fonts and glyph atlases: no font replacement.
font_paths=["src/core/font.c","src/core/font.h","src/core/font_data.c"]
# 2: All Spanish glyphs supported by the ORIGINAL small text font.
spanish_chars="áéíóúñüÁÉÍÓÚÑÜ¿¡"
# 3: The coordinates, text sizing algorithms and theme are byte-identical.
layout_paths=["src/core/draw.c","src/core/draw.h",
              "src/core/layout.c","src/core/layout.h","src/core/theme.h",
              "src/ps3/render_tiny3d.c"]
# 4: Original animations, music, pictures, icons and background bytes unchanged.
media_paths=["src/core/icons_data.c","src/core/images_data.c",
             "src/core/sounds_data.c","src/core/stage_data.c",
             "src/core/music.c","src/core/anim.c"]
# 5: Controller input + PS3 launch unchanged, including Cobra/webMAN.
control_paths=["src/core/input.c","src/ps3/main.c",
               "src/ps3/sf_cobra_mount.c","src/ps3/ps3_webman.c"]
# 6: All PS1 cover/BDISO scanning, fetching, decoding and mounting intact.
cover_paths=["src/core/library.c","src/core/coverart.c","src/core/ps1_name_match.c",
             "src/core/scanner.c","src/ps3/ps3_covers.c",
             "src/ps3/sf_bdiso_ntfs.c","src/ps3/sf_bdiso_art.c"]
# 7: Existing settings file keys preserved, as well as XMB app identity.
serialized_keys=["autoplay=%d","offline=%d","music_volume=%d",
  "shuffle=%d","sfx_volume=%d","background2=%d","background_image=%s",
  "ui_color=%d","custom_cases=%d","reflections=%d","sleep_minutes=%d",
  "show_game_info=%d","hide_ui=%d","show_hidden=%d","ps3_only=%d",
  "case_light=%d","ignore_the=%d","xmb_top=%d","last_tab=%d",
  "last_key_%s=%s","ps2cfg_copy=%08x"]
# 8: Actual rendered menu text is localized, without changing RowKind IDs.
# 9: About, downloads, controls and on-screen notices in Spanish.
ui_targets=["Jugar","Vista","Ajustes del juego","Ajustes generales",
            "Volver","Elegir","Cambiar","Salir (mantén)",
            "Descargar carátulas faltantes","Buscando carátulas",
            "Sin juegos","Sin películas","Almacenamiento",
            "Toda la colección","REPRODUCIENDO","Funciones",
            "Creado por spooky","Elegir disco","Versión %s"]
app_targets=["Iniciando","Cargando ajustes","Preparando gráficos",
             "Actualizando discos NTFS","Buscando juegos de %s",
             "Ordenando","Casi listo","Memoria insuficiente",
             "La descarga no está disponible","No se encontraron carátulas nuevas",
             "Imagen: %s","No disponible"]
# 10: Formatting and translated-screen UTF-8 safety.
generated_literals=[]
for contents in [menu,settings,ui,app,library]:
    generated_literals += re.findall(r'"((?:\\.|[^"\\])*)"',contents)
unicode_used={ord(c) for literal in generated_literals for c in literal if ord(c)>127}

checks=[
 ("1. Tipografías y atlas idénticos a V15.11", unchanged(font_paths)),
 ("2. Todos los signos del español presentes en la fuente original",
     all(ord(ch) in cps for ch in spanish_chars)),
 ("3. Diseño, escala de texto y renderizado intactos", unchanged(layout_paths)),
 ("4. Fondos, animaciones, sonidos e iconos intactos", unchanged(media_paths)),
 ("5. Controles, Cobra y webMAN intactos", unchanged(control_paths)),
 ("6. PS1, BDISO y descarga de carátulas intactos", unchanged(cover_paths)),
 ("7. Archivos de ajustes y título de XMB compatibles",
      unchanged(["sfo.xml"]) and all(k in settings for k in serialized_keys)),
 ("8. Todas las etiquetas del menú traducidas",
      all(('return "'+k+'";') in menu for k in menu_targets)
      and "Predet." in menu and '"Sí"' in menu and '"No"' in menu),
 ("9. Menús, ayudas, indicadores y avisos traducidos",
      all(k in ui for k in ui_targets) and all(k in app for k in app_targets)
      and '"No games"' not in ui and '"Game Settings"' not in ui
      and '"Appearance"' not in menu),
 ("10. UTF-8 y formatos de contador sin errores",
      unicode_used.issubset(cps)
      and 'carátula%s agregada%s", got, got == 1 ? "" : "s", got == 1 ? "" : "s"' in ui
      and 'carátula%s descargada%s", a->dl_got, a->dl_got == 1 ? "" : "s", a->dl_got == 1 ? "" : "s"' in app
      and 'snprintf(lab,sizeof(lab),"Disco %s",num);' in ui)
]
for title,ok in checks:
    print(("PASS" if ok else "FAIL"),title)
print("ES-419: %d/10 comprobaciones; cambios solo en textos, opciones y pruebas"%(sum(bool(x) for _,x in checks)))
if not all(ok for _,ok in checks):
    raise SystemExit("Language-only safety review failed")
