# SpookyFlow V15.22: corrección ISO de PS2 mayores de 2 GiB

## Problema reproducible del usuario
En V15.20, el archivo /dev_hdd0/PS2ISO/Dragon Ball Z Budokai Tenkaichi 4.iso pesa 4.866.244.608 bytes (4,53 GiB). El cifrado terminó abruptamente al 44% con RC=-34 en dos ocasiones. Ese porcentaje corresponde aproximadamente al tamaño de salida de 2 GiB. El tercer intento fue cancelado (RC=-70).

## Causa probable identificada en el código
El conversor escribía mediante FILE* (fwrite/ftello/fseeko), cuyo off_t puede ser de 32 bits en el entorno PSL1GHT PPU. Además, se usaba fseeko con (off_t) de la posición de final de ISO y de las muestras de diagnóstico. La causa exacta del sistema PS3 no estaba registrada en V15.20, por lo que no se debe afirmar certeza del errno.

## Solución V15.22
Se añadió src/ps3/sf_ps2_largefile.h con archivo genérico de posición uint64_t. En PS3 utiliza sysLv2FsOpen, sysLv2FsRead/Write, sysLv2FsLSeek64 y sysLv2FsFsync; el backend de pruebas usa stdio 64 bits de host. Se aplica a cifrado de datos y metadatos, búsqueda del inicio y fin de ISO, huella de origen, y diagnóstico del último segmento cifrado. No cambia los bytes criptográficos, LIMG, CONFIG ni PS2 Classics Placeholder. Se mantiene el inicio automático opcional de V15.21 por webMAN MOD.

Si vuelve a fallar, el único diagnostico.txt registra PS2_DIAG LARGEFILE_IO_ERROR con OP, SYS_RC, FILE_OFFSET, PROGRESS y RC. No modifica la ISO de origen y conserva los archivos finales previos hasta transacción completa.

## Pruebas
- Prueba de archivo disperso en host (sin consumir gigabytes): escribe/lee y verifica offsets justo antes, en y después de 2 GiB, 4 GiB y el fin de la ISO del usuario de 4.866.244.608 bytes.
- 32 invariantes de integración de seguridad de archivo grande.
- Pruebas existentes de cifrado AES/SHA1 y reconstrucción PS2 CD y validación del PKG compilado.

**Pendiente:** prueba real de cifrado y arranque del juego en PS3 HEN. CI prueba la estructura y uso de API nativa pero no emula el firmware PS3. V15.20 y V15.21 permanecen disponibles para volver atrás si se necesita.
