# SpookyFlow V15.13 — montaje PS2 (EXPERIMENTAL)

Rama de prueba a partir de V15.12; no altera la publicación estable.

## Qué implementa

- Ruta independiente de webMAN MOD para ISO de PS2 en CFW con Cobra activo.
- Usa `cobra_mount_ps2_disc_image` de la libCobra de IRISMAN, que invoca la operación Cobra 0x7028.
- Acepta `/dev_hdd0/PS2ISO/*.iso` y archivos divididos comenzando por `*.iso.0`.
- Configuración PS2 por juego (CONFIG) conservada; la comprobación de compatibilidad se ejecuta antes de escribirla.
- Respeta la elección de `ps2_netemu` al seleccionarla explícitamente; «Predeterminado» no fuerza un cambio.
- Diagnóstico cerrado después de cada punto crítico: `/dev_hdd0/tmp/spookyflow/ps2_last_mount.txt` y `ps2_mount_trace.txt`.
- PS1, PS3, BDISO, portadas, interfaz y archivos originales permanecen sin cambios funcionales, salvo la nueva ruta PS2.

## Compatibilidad y límites IMPORTANTES

**NO validado en una consola física**. Las pruebas CI, la compilación y las auditorías no demuestran que la consola ejecute correctamente un juego.

**HEN**: Las ISO de PS2 sin cifrar no funcionan con esta ruta directa, tal como confirma el código de webMAN MOD. El detector `0x1337` devuelve un error claro ANTES de eyectar o desmontar discos. En HEN se utiliza el formato PS2 Classics (`ISO.BIN.ENC`) y su lanzador, que todavía no está incorporado a este backend. No se inventa una conversión ni se altera el kernel.

**CFW Cobra**: solamente HDD interno; no monta PS2 directamente desde USB/NTFS. No modifica firmware, no instala plugins, no controla ventilador y no toca los montadores PS3/BDISO. Las ISO de PS2 con pistas CD-Audio requieren soporte CUE en una futura iteración.

**Riesgo conocido**: si falla un montaje después de desmontar otro disco, puede ser necesario reiniciar la PS3. El diagnóstico deja constancia de la última operación.

## Referencias técnicas

- IRISMAN: https://github.com/aldostools/IRISMAN — libCobra, source/cobra.c
- webMAN MOD: https://github.com/aldostools/webMAN-MOD — include/mount/mount.h y mount_cobra.h
- Documentación de formatos: https://github.com/aldostools/webMAN-MOD/wiki/Game-Paths-%26-Covers

El código adaptado conserva atribución GPL-3.0-o-posterior; cualquier distribución debe mantener las licencias correspondientes.
