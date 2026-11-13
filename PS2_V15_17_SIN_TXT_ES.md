# SpookyFlow V15.17 — cifrado PS2 en HEN, sin informes TXT de errores

## Corrige la causa demostrada por la consola

La V15.16 mostró «PS2 Classics: cifrado no disponible». Al examinar los
registros de compilación, comprobamos que el compilador rechazaba
`mbedtls_sha1(... ) != 0` con mbedTLS 2.28: la función histórica devuelve
`void`. La compilación se reintentaba como `SF_TLS=0`, dejando fuera el
cifrador PS2 Classics a pesar de producir un PKG «correcto».

Esta versión utiliza `mbedtls_sha1_ret` y **prohíbe compilar PS2 sin mbedTLS**,
incluida la compilación manual. Si no compila mbedTLS, CI termina en error y no
publica otro instalador que muestre «cifrado no disponible».

## Eliminar archivos de Bloc de notas de errores

Se compila con `SF_DISABLE_DIAG_TXT`. Se anula la creación de:
- log general `log.txt`;
- diagnósticos USB/NTFS y carátulas;
- diagnósticos del montador PS3 Cobra;
- informes y trazas PS2 Cobra;
- `ps2_hen_last.txt` y `ps2_launcher_probe.txt`.

Estas rutinas pasan a ser operaciones sin escritura y éxito no fatal. Se
mantienen los avisos en pantalla. No se borran automáticamente TXT
preexistentes (para evitar eliminar archivos que puedan interesar al usuario).
No se eliminan archivos de datos con extensión `.txt`, como el catálogo de
carátulas PS1 o metadatos de fuentes necesarios para reconocimientos.

## Estado de compatibilidad

- PS2 HEN: ISO sin dividir en HDD o FAT32 USB; cifra a PS2 Classics
  `/dev_hdd0/game/PS2U10000/USRDIR/ISO.BIN.ENC`.
- El lanzador PS2 Classics instalado y activado sigue siendo necesario para
  iniciar el juego; SpookyFlow no depende de webMAN MOD para convertirlo.
- PS1, PS3, BDISO, fondos, portadas e interfaz se conservan.

**Pendiente:** ejecución real con Rumble Racing en PS3 HEN. El PKG solo debe
ofrecerse como versión experimental aunque la compilación y CI sean correctas.
