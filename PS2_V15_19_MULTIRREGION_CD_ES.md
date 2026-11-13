# SpookyFlow V15.19 — PS2 multi-región + conversión de CD a PS2 Classics

## Base y diagnóstico de la PS3 real

La V15.18 confirmó mediante `diagnostico.txt`:
- Versión real seleccionada: `SLES_501.20` (Europa / PAL)
- ISO original: 607 758 336 bytes, en formato cocido de 2048 bytes/sector
- `ISO.BIN.ENC`: cifrado AES/SHA1 correcto en bloques 0 y último
- CONFIG de esa región: ausente; únicamente `SLUS_201.74.ENC` se incluía
- Descriptor LIMG anterior: DVD, 2048 bytes/sector pese a que el título
  Rumble Racing se comercializó en CD-ROM
- El lanzamiento externo desde PS2 Classics volvió al XMB, cuyo error
  interno no puede interceptarse desde SpookyFlow.

## Cambios

1. Usa `SYSTEM.CNF`/ID real para reconocer automáticamente regiones:
   NTSC-U (EEUU), PAL (Europa), NTSC-J (Japón) y Asia. No adivina región
   según la imagen de carátula ni sustituye CONFIG de otra edición.
2. CONFIG cifrado por identidad EXACTA: primero
   `/dev_hdd0/game/PS2CONFIG/USRDIR/CONFIG/ENC/<SERIAL>.ENC`; después
   `/dev_hdd0/game/SPKF00001/USRDIR/ps2configs/<SERIAL>.ENC`.
   El PKG conserva `SLUS_201.74.ENC` e incorpora `SLES_501.20.ENC`
   original de la base de datos webMAN MOD (GNU GPL), hash Git de blob
   `bfcb941393185fb4e4682aba6a33e900dc20a182`.
3. Reconoce automáticamente las ISO CD *raw* de 2352 bytes/sector, de
   cualquier región, por su descriptor ISO9660 en offset `0x9318`.
4. Para las ISO *cocidas* de Rumble Racing `SLES_501.20` y
   `SLUS_201.74` (PVD en `0x8000`), convierte en streaming los
   datos 2048 a CD-ROM XA Mode2/Form1 2352 bytes, con cabecera,
   EDC y ECC P/Q reales, basados en la implementación de Neill
   Corlett ECM (GPL). La ISO ORIGINAL permanece intacta.
5. El LIMG resultante indica `CD, 2352`, con el número auténtico de
   sectores físicos. No se cambia un valor 2048→2352 de manera
   ficticia sin alterar físicamente los sectores.
6. La versión del manifiesto cambia de `SF-PS2-CLASSICS2` a
   `SF-PS2-CLASSICS3`: archivos preparados con el descriptor
   anterior no se reutilizan silenciosamente. La sustitución guarda
   copias y evita sobrescribir ISO originales.
7. El `diagnostico.txt` único de V15.18 se conserva y continúa
   sobrescribiéndose en cada inicio. Incluye región, ID y tipo de
   disco preparado.

## Límites de compatibilidad reales

- Todas las regiones **se reconocen**, pero no todos los juegos
  funcionarán en el emulador PS2 Classics de la PS3.
- Para otros títulos CD que ya tengan 2352 bytes/sector, el programa
  conserva ese formato. Si solo se dispone de una ISO cocida de
  2048 bytes/sector de un CD desconocido, no la modifica automáticamente
  como CD: requiere una identificación inequívoca para evitar crear datos
  inválidos.
- La conversión Mode2/Form1 puede recrear un único track de **datos**,
  pero no recupera pistas CD-DA, sectores XA Form2, subcanales ni datos
  que se hayan perdido al transformar la imagen original a ISO 2048.
  Para esos juegos puede necesitarse un dump crudo BIN/CUE auténtico.
- Sigue siendo necesario tener instalado PS2 Classics Launcher
  (`PS2U10000`) en HEN. No se necesita webMAN MOD para preparar los
  juegos. No tocamos el firmware ni instalamos plugins.
- La compilación en GitHub y la reproducción con imágenes de prueba
  no prueban el lanzamiento en PS3 física. Hasta que Rumble Racing
  arranque, esta entrega se considera EXPERIMENTAL.

## Referencias técnicas

- PS3 Developer Wiki, PS2 Emulation, LIMG Segment
- aldostools/webMAN-MOD, PS2CONFIG/CONFIG/ENC/SLES_501.20.ENC
- bucanero/apollo-ps3, PS2 Classics encryption routines
- Neill Corlett, unecm.c GNU GPL, EDC/ECC sector generator
- ECMA-130, CD-ROM Mode2 Form1 sectors
