# SpookyFlow V15.14 — PS2 HEN (EXPERIMENTAL)

Nueva rama independiente de V15.12. El PKG base V15.12 en español no se modifica.

## Objetivo y limitación real

En **PS3HEN** no se puede iniciar una ISO PS2 de forma idéntica al disco virtual Cobra de **CFW**.
Según el código de webMAN MOD y la documentación de ConsoleMods, en HEN se necesita **PS2 Classics**.
Este backend no utiliza webMAN MOD: convierte una ISO desde SpookyFlow y coloca un
`ISO.BIN.ENC` compatible con la ruta del lanzador **PS2 Classics Placeholder (PS2U10000)**.

**Después de preparar el juego, vuelve al XMB e inicia PS2 Classics Placeholder.**
La preparación **no equivale a montar un disco PS2 virtual**. No afirmamos que aparezca el
mismo icono de disco de PS2 que en CFW.

## Qué sí hace

- Lee ISO desde el HDD `/dev_hdd0/PS2ISO/*.iso` o USB FAT32 `/dev_usb00N/PS2ISO/*.iso` exclusivamente en modo de lectura.
- Reconoce CD/DVD ISO9660; agrega el sector LIMG **solo al flujo cifrado**, nunca a la ISO fuente.
- Cifra con AES-CBC/PS2 Classics, NPD/CMAC y SHA1 de sectores, siguiendo el
  código público de Apollo Save Tool (GPLv3).
- Conversión en hilo secundario, porcentaje visible en la interfaz original.
- Escribe primero el archivo temporal bajo la carpeta del Placeholder,
  valida cabecera y longitud, y sustituye el anterior solamente tras éxito.
- Conserva un archivo cifrado previo **que no haya sido generado por SpookyFlow**
  como `ISO.BIN.ENC.sf-original`, sin sobrescribir un respaldo existente.
- Registra errores en `/dev_hdd0/tmp/spookyflow/ps2_hen_last.txt`.
- Reutiliza una conversión validada cuando coinciden ruta, tamaño y SHA1 de los sectores inicial y final; invalida la caché si cambian.
- La ruta CFW Cobra de PS2 sigue funcionando como V15.13; PS1, PS3, BDISO, controles,
  carátulas y gráficos permanecen intactos.

## Requisitos

- PS3 con HEN activado.
- **PS2 Classics Placeholder** instalado y activado en el XMB, con su licencia
  correspondiente. No se incluye ni instala automáticamente.
- Suficiente espacio interno: la imagen cifrada es aproximadamente del tamaño de
  la ISO original, y por seguridad se conserva el juego cifrado anterior.
- Para esta primera versión, únicamente ISO **sin dividir en un solo archivo**
  desde HDD interno o USB FAT32. Los archivos `.iso.0` y unidades NTFS no están habilitados.

## Límites de la validación

Esta es una compilación **experimental no probada físicamente en PS3**.
Las comprobaciones de código, la compilación cruzada, las pruebas de cifrado estático
y la verificación estructural del PKG no sustituyen una prueba de inicio real.
Algunos juegos requieren archivos CONFIG específicos; el nuevo backend HEN no aplica
todavía automáticamente un CONFIG cifrado. No se promete compatibilidad universal.

## Código y licencias

- https://github.com/bucanero/apollo-ps3 — `source/ps2classic.c`, `include/ps2_data.h`
- https://github.com/aldostools/webMAN-MOD — `include/mount/mount_misc.h`,
  `include/mount/ps2_classic.h`.
- https://consolemods.org/wiki/PS3:PS2_Games,_Multiplayer,_and_Fixes

Adaptaciones y atribuciones: GPL-3.0-or-later. La redistribución del código
fuente correspondiente y sus avisos de licencia debe mantenerse.

## CONFIG especial de Rumble Racing (SLUS_201.74)

Incorpora en el PKG el archivo cifrado de PS2CONFIG de aldostools
`USRDIR/ps2configs/SLUS_201.74.ENC`, identificado para Rumble Racing.
Si está instalado PS2CONFIG, se prioriza el archivo ENC con el ID exacto; si
no, se utiliza el respaldo incluido de este juego. Una configuración sin el
ID correcto nunca se aplica indiscriminadamente a otro juego.

Los archivos originales `ISO.BIN.ENC` y `CONFIG` existentes en PS2 Classics
Placeholder se protegen con respaldo `.sf-original` antes de ser sustituidos;
los archivos de SpookyFlow se reconocen mediante marcadores `.sf-source`.
No se elimina ni se sobrescribe ningún respaldo ajeno existente.

## V15.15 — Reparación del aviso del lanzador

La V15.14 bloqueaba el proceso si no encontraba `USRDIR/EBOOT.BIN` como archivo regular. Algunos lanzadores instalados y funcionales no exponen ese archivo de esa manera. La V15.15 identifica la instalación de PS2U10000 comprobando la carpeta `USRDIR` junto con `PARAM.SFO` o un ejecutable reconocido. No interpreta una ISO dañada como un lanzador ausente. Registra las comprobaciones concretas en `/dev_hdd0/tmp/spookyflow/ps2_launcher_probe.txt` y, cuando no está disponible el lanzador, mantiene la negativa segura, sin alterar su contenido. El arranque en PS3 física continúa pendiente de validación.
