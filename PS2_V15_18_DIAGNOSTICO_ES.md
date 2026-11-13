# SpookyFlow V15.18 — Un diagnóstico único y prueba de arranque PS2 HEN

## Resultado que motivó la revisión

La ISO de PS2 Rumble Racing llegó al 100% en SpookyFlow V15.17. PS2 Classics
Launcher salió al XMB al intentar ejecutarla. Eso puede deberse al formato
del archivo cifrado, CONFIG, LIMG/CD o a una incompatibilidad específica del
emulador. El error de **otro proceso** (PS2 Classics Launcher) no se puede
capturar directamente desde SpookyFlow tras salir de la aplicación.

Esta versión NO aplica cambios ciegos al tipo de disco ni altera una ISO
original. Incorpora una inspección de bajo costo del archivo cifrado instalado,
antes y después de la conversión.

## Único informe, siempre reemplazado

`/dev_hdd0/tmp/spookyflow/diagnostico.txt`

Al iniciar SpookyFlow se abre con `wb`, por lo que sustituye el informe
anterior. Durante la ejecución, agrega las etapas importantes al mismo archivo
y descarga físicamente las escrituras (flush). No abre un informe por juego ni
por cada error. Limita a 256 KiB y omite los mensajes repetitivos de renderizado.

El informe registra:
- estado de PS2 Classics Launcher `PS2U10000`, ISO seleccionada, ID, tamaño;
- cabecera del `ISO.BIN.ENC`, bytes reales/esperados y Content ID;
- descifrado de metadatos y validación SHA-1/índice del primer y último bloque;
- comprobación de `LIMG`, tipo CD/DVD, sectores y tamaño del sector;
- presencia/validez del CONFIG cifrado e identificación del origen de CONFIG;
- metadatos de la conversión, copias de seguridad y conversiones incompletas.

## Limpieza automática controlada

Solo se eliminan **12 archivos de diagnóstico de versiones anteriores**
con nombres exactos, en `/dev_hdd0/tmp/spookyflow`: `log.txt`,
`diagnostico_portadas.txt`, `diagnostico_usb_ntfs.txt`,
`cobra_startup_probe.txt`, `cobra_selection_probe.txt`,
`cobra_last_mount.txt`, `cobra_trace.txt`,
`cobra_previous_interruption.txt`, `ps2_hen_last.txt`,
`ps2_launcher_probe.txt`, `ps2_last_mount.txt`,
`ps2_mount_trace.txt`. Si un nombre pertenece a una carpeta, no se elimina.

**NO se elimina** `settings.ini`, `sf_blank.iso`, `covers`,
`downloaded`, `music`, `sounds`, `tabs`, ni el juego PS2,
su CONFIG o sus copias de seguridad. No se borran TXT de datos de carátulas.

## Prueba concreta

1. Instalar este PKG experimental y abrir SpookyFlow una vez. Se limpia
   la carpeta de los 12 nombres antiguos y se crea `diagnostico.txt`.
2. Abrir `/dev_hdd0/tmp/spookyflow/diagnostico.txt` para revisar el estado
   de la conversión V15.17 que quedó instalada en PS2 Classics.
3. Seleccionar Rumble Racing en SpookyFlow. Si la conversión cacheada es
   válida, se reutiliza; si no, se convierte. No se modifican ISO originales.
4. Tras salir al XMB, probar PS2 Classics Launcher. Si vuelve al XMB,
   abrir de nuevo SpookyFlow para regenerar el informe con los datos
   de la imagen que quedó instalada, y copiar el TXT para analizarlo.

Este informe permite descartar defectos de formato y CONFIG en SpookyFlow,
pero no demuestra por sí solo la causa interna de un cierre de PS2 Classics
fuera de SpookyFlow. La prueba final debe hacerse en una PS3 con HEN.
