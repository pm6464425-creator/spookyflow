# SpookyFlow V15.21 — Inicio automático desde XMB

Conserva el cifrado de V15.20 y la ruta ISO.BIN.ENC en PS2 Classics Placeholder. Al seleccionar un juego PS2 y comprobar que está preparado, SpookyFlow envía a webMAN MOD:

`/wait.ps3?xmb;/wait.ps3?3;/play.ps3?PS2U10000`

webMAN esperará a que SpookyFlow haya salido al XMB, esperará otros 3 segundos y solicitará ejecutar PS2 Classics Launcher (PS2U10000). No crea disco virtual PS2 en HEN ni monta ISO cruda.

Si webMAN no responde, SpookyFlow vuelve al XMB y el usuario puede abrir PS2 Classics manualmente. La orden aceptada no garantiza ejecución real: requiere pruebas con consola PS3 HEN, webMAN MOD activo y launcher instalado y activado.

El único diagnostico.txt registra PS2_DIAG AUTOPLAY=QUEUED o PS2_DIAG AUTOPLAY=MANUAL_FALLBACK. La V15.20 no se modifica.
