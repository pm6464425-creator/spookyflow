# SpookyFlow V15.20 — arreglo del error RC=-40 al completar PS2 Classics

## Evidencia de consola
El diagnóstico de Rumble Racing PAL (SLES_501.20) registró:
- ISO: `/dev_hdd0/PS2ISO/Rumble Racing.iso`, 607758336 bytes.
- Entrada: `COOKED_CD_2048_TO_RAW_2352` (CD-ROM de 2048 a 2352 bytes).
- Conversión AES alcanzó `CIFRANDO PROGRESS=100`.
- Inmediatamente después: `STAGE=ERROR RC=-40` y nuevo intento desde 0%.
- Manifiesto instalado anterior: `SF-PS2-CLASSICS2`.

## Causa identificada en el código
En `src/ps3/sf_ps2_hen.c`, `finish_atomic()` devolvía exactamente
`-40` cuando fallaba `rename(ISO.BIN.ENC.sf-source-part,
ISO.BIN.ENC.sf-source)`. El destino existía desde la versión anterior;
una sustitución por rename no debe asumirse disponible en la PS3.
El archivo antiguo se restauraba, pero el nuevo manifiesto no se
confirmaba y por eso cada nueva selección reiniciaba el cifrado.

**No debe interpretarse PROGRESS=100 como éxito hasta pasar la fase
FINALIZANDO, el guardado del manifiesto y la selección del CONFIG.**

## Cambios
1. Conserva la imagen cifrada anterior hasta confirmar imagen nueva Y
   nuevo manifiesto. El registro anterior se mueve temporalmente a
   `ISO.BIN.ENC.sf-source-old` y el nuevo ocupa su destino ya libre.
2. Si falla el guardado, recupera el archivo antiguo y la imagen
   previa. Si falla una recuperación, conserva los respaldos y registra
   `ROLLBACK_FAIL`; nunca elimina intencionalmente el respaldo válido.
3. Rechaza (`-43`) una copia de recuperación previa todavía presente,
   sin sobrescribirla. Registra las fases `FINALIZANDO`,
   `COMMIT_FAIL`, `COMMIT_RESULT`, `ERRNO` y presencia de respaldos.
4. Corrige los dos textos estáticos del encabezado del diagnóstico,
   que en V15.19 todavía decían `V15.18`. El encabezado antiguo
   **no demostraba** que se instalara la versión equivocada.
5. Amplía las pruebas de simulación para comprobar guardado repetido,
   invalidación de caché, preservación del respaldo original y bloqueo
   seguro de recuperación pendiente.
6. Mantiene el diagnóstico único
   `/dev_hdd0/tmp/spookyflow/diagnostico.txt`. No crea nuevos TXT.
   No cambia carátulas, interfaz española, PS1, PS3 ni BDISO.

## Prueba pendiente en hardware
La compilación y las pruebas en GitHub son diferentes de la prueba de
arranque en una PS3 física con HEN. Para comprobar la corrección:
1. Instalar V15.20 sobre la versión experimental anterior.
2. Seleccionar Rumble Racing una sola vez; confirmar que tras 100% aparece
   `COMMIT_RESULT=OK` y `CONFIG_PRESENT=1`.
3. Ejecutar el PS2 Classics Launcher existente desde XMB.
4. Si falla el arranque, capturar el único `diagnostico.txt`.
No borrar manualmente `ISO.BIN.ENC`, `CONFIG` ni los respaldos.
