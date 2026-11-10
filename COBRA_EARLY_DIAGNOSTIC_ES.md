# SpookyFlow — diagnóstico temprano de bloqueos

Revisión experimental EARLY_PROBE_20261109_R1. Conserva la interfaz original.

El archivo `cobra_startup_probe.txt` se escribe **antes de Tiny3D** y
se actualiza en etapas: BEFORE_TINY3D_INIT, AFTER_TINY3D_INIT,
AFTER_PAD_INIT, AFTER_SYSUTIL_CALLBACK, BEFORE_APP_INIT,
READY_FOR_GAME_SELECTION. Así no depende de que el detector de montaje
ya haya comenzado.

Cuando se selecciona un juego PS3, antes de consultar sus ajustes y
antes de realizar una llamada Cobra, se escribe otro archivo:
`cobra_selection_probe.txt`. Si no puede escribirse ni en su ruta
principal ni en la alternativa, se cancela el montaje y muestra aviso.

Archivos en FileZilla:
- `/dev_hdd0/tmp/spookyflow/cobra_startup_probe.txt`
- `/dev_hdd0/tmp/spookyflow/cobra_selection_probe.txt`
- `/dev_hdd0/tmp/spookyflow/cobra_last_mount.txt` (solo al montar)
- `/dev_hdd0/tmp/spookyflow/cobra_trace.txt`
- `/dev_hdd0/tmp/spookyflow/log.txt`

Alternativas en caso de que la carpeta `spookyflow` no se pueda escribir:
- `/dev_hdd0/tmp/cobra_startup_probe_fallback.txt`
- `/dev_hdd0/tmp/cobra_selection_probe_fallback.txt`

## Verificación controlada antes de lanzar un juego

1. Instala el nuevo PKG y activa HEN.
2. Abre SpookyFlow **sin seleccionar Gran Turismo 6**.
3. Comprueba que `cobra_startup_probe.txt` existe y dice
   `BUILD=EARLY_PROBE_20261109_R1`,
   `STAGE=READY_FOR_GAME_SELECTION` (o alguna etapa anterior).
4. Si el archivo no existe en ninguna de las dos rutas, NO pruebes
   juegos; puede que no esté instalada esta compilación o fallen
   los accesos a disco.
5. Recupera `log.txt` y los otros archivos antes de intentar
   otro montaje que pudiera bloquear la PS3.

Estos registros solo permiten detectar **hasta dónde llegó el código**.
No corrigen por sí solos el montaje Cobra ni garantizan que no se cuelgue
el sistema. No pueden diagnosticar una congelación anterior a que el
proceso ejecute las primeras instrucciones ni un bloqueo del kernel
si la escritura previa no quedó persistida.
