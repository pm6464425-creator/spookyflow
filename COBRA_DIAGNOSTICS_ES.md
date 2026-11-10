# Diagnóstico del lanzador Cobra: SpookyFlow PS3 HEN

Versión experimental para **identificar** el punto de bloqueo; no garantiza
que corrija el montaje del juego Gran Turismo 6.

Antes de cada llamada crítica de Cobra (opcodes 0x7000, 0x7020, 0x7022,
0x7024, 0x702C y syscall 35) escribe un estado BEFORE_* y CIERRA el TXT.
Al volver de la llamada registra AFTER_* con el resultado. Se comprueba que
existan PS3_GAME/PARAM.SFO y PS3_GAME/USRDIR/EBOOT.BIN antes de montar carpetas.

Archivos que pueden copiarse por FTP:
- /dev_hdd0/tmp/spookyflow/cobra_last_mount.txt — último estado, etapa y juego.
- /dev_hdd0/tmp/spookyflow/cobra_trace.txt — historial de llamadas (máx. ~96 KiB).
- /dev_hdd0/tmp/spookyflow/cobra_previous_interruption.txt — copia del
  último intento incompleto cuando se vuelve a abrir SpookyFlow.
- /dev_hdd0/tmp/spookyflow/log.txt — log general original.

Si PS3 se congela: tras reiniciar y activar HEN, copia los TXT con FileZilla.
Un estado IN_PROGRESS significa que no se registró el final de esa etapa:
puede deberse a un cuelgue, cierre forzoso o corte de energía. La escritura
se intenta completar antes del syscall, pero un apagado brusco no garantiza
persistencia absoluta del filesystem.

Esta edición **no instala webMAN MOD ni plugins en boot_plugins.txt**.
No es un depurador de kernel y no puede registrar errores que ocurran después
de salir de SpookyFlow, dentro de Gran Turismo 6.
