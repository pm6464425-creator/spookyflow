# SpookyFlow V15.24 — Montador PS1, corrección Netemu y diagnóstico único

## Problema reportado en PS3 HEN
Al seleccionar juegos PS1 desde V15.23 aparecía «Para iniciar necesitas webMAN MOD activo». El código ocultaba el motivo real tras un único -41 y enviaba a webMAN cualquier juego cuya preferencia de emulación fuera Netemu, incluso con un BIN/CUE válido para Cobra.

## Corrección realizada
- El montador nativo intenta Cobra para imágenes PS1 de un solo archivo de sectores raw de 2352 bytes, con CUE de varias pistas del mismo BIN. La preferencia PS1 Netemu ya no fuerza el abandono de la ruta nativa; **Cobra no garantiza que la emulación use Netemu**, solo monta la imagen.
- Los archivos CUE que referencian varios BIN, CHD/PBP, NTFS con índices y otros formatos no comprobados no se montan falsamente: se informa el motivo y se conserva la ruta opcional webMAN. No se altera ni convierte la ISO.
- No se ejecutan operaciones de disco antes de comprobar formato y versión Cobra. El error real de syscall se registra cuando sea observable; la rutina conserva el intento de restaurar la presencia del disco al fallar.

## Un solo TXT y limpieza segura
- Se elimina el antiguo informe del PS2 `/dev_hdd0/tmp/spookyflow/diagnostico.txt` (no se borra ninguna ISO, CONFIG, carátula, copia de seguridad o configuración).
- Se crea **un único** `/dev_hdd0/tmp/spookyflow/diagnostico_ps1.txt`, truncado/reemplazado **al iniciar cada vez SpookyFlow**. No se crean informes numerados. La lista de limpieza solo contiene nombres de informes antiguos conocidos.
- Los mensajes PS2/PS3/BDISO no se escriben al nuevo informe. Los módulos de cifrado y PS2 de V15.22 permanecen funcionalmente intactos.
- Los campos relevantes incluyen `STAGE=SELECTION`, `IMAGE_STAT` y `IMAGE_SIZE`, `CUE_PARSE`, `COBRA_VERSION`, `DISC_TYPE`, `FAKE_EJECT`, `MOUNT_PSX_7027`, `FAKE_INSERT`, `ROLLBACK` y `WEBMAN_TRANSPORT`.

## Prueba en consola
1. Activar HEN y ejecutar SpookyFlow V15.24.
2. Seleccionar exactamente el juego PS1 que antes pedía webMAN.
3. Si el XMB no muestra el disco, recuperar `/dev_hdd0/tmp/spookyflow/diagnostico_ps1.txt` con FTP/multiMAN y enviarlo para identificar el motivo concreto.
4. Si el juego es CHD/PBP o CUE multifichero, el diagnóstico lo notificará: requiere otro método de montaje/conversión, no es un fallo de las opciones de PS3.

Las pruebas automáticas de host verifican la seguridad y la estructura; la confirmación física de arranque en HEN queda pendiente hasta probar el PKG en la consola.
