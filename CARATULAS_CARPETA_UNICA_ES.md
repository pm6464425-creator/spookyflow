# Portadas centralizadas (SpookyFlow PS1 / PS2 / PS3)

La aplicación busca imágenes **exclusivamente** en:

`/dev_hdd0/tmp/spookyflow/downloaded/`

No utiliza PNG/JPG junto a las ISO ni la carpeta de carátulas de
multiMAN o webMAN. Se evita seleccionar por error los iconos horizontales
320×176 que deja la conversión de carpeta a ISO.

## Identificación y orden

1. **Manual, por nombre:** para `Gran Turismo6.ISO`, poner
   `/dev_hdd0/tmp/spookyflow/downloaded/Gran Turismo6.jpg`
   (o `.png`). Sirve también para mods como `EA Sports FC 27.iso`.
2. **Automática, por Title ID:** si la carátula manual no existe, la app
   consulta `/dev_hdd0/tmp/spookyflow/downloaded/BCES01893.jpg`
   (o `.png`) y descarga solo si falta.
3. **Sin coincidencia:** muestra la caja vacía/placeholder, **nunca**
   reutiliza un ICON0 horizontal desde la carpeta PS3ISO.

La carpeta gestionada almacena tanto descargas como imágenes que copies
manualmente por FTP. No se mueven archivos automáticamente desde
`PS3ISO`, porque esos PNG de 320×176 no son las carátulas correctas.

### Primer inicio después de instalar esta versión

La caché procesada pasa de versión 5 a 6. El programa no muestra las
imágenes procesadas de la política anterior. Las descargará de nuevo
cuando exista ID y una fuente de carátula accesible; la primera pasada
puede requerir conexión a Internet. No elimina las ISO ni los PNG
originales de multiMAN.

### Sustitución manual

Copia un JPG o PNG vertical a la carpeta centralizada con el mismo nombre
de la ISO sin extensión y elige **Rescan Library**. La versión anterior
de la imagen procesada quedará invalidada. Se conservan las proporciones
y el procesamiento de las carátulas originales.

**No cambia** el lanzador Cobra, el Title ID de SpookyFlow, sus animaciones
ni los directorios de juegos.
