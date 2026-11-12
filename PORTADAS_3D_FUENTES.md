# SpookyFlow — carátulas normales y marca de agua 3D

## Regla de imagen

- **Primero** se usa una carátula personalizada colocada junto al ISO en `BDISO/`, con el mismo nombre del ISO (se admite `.jpg`, `.png` o `.iso.jpg`).
- De forma automática se busca la **portada normal en español**: The Movie Database si tienes clave; ficha normal en español de Mubis sin clave; MoviePosterDB con clave.
- **Después** se busca una portada normal en inglés: Cinemeta sin clave, TMDB, OMDb o fanart.tv si están disponibles.
- **Nunca** se buscan carátulas comerciales de la *edición Blu-ray 3D*. Es el estuche del programa el que lleva dos marcas de agua pequeñas: «3D» a la izquierda (solo en ISO 3D) y «Blu-ray» a la derecha. No hay bandas ni fondos opacos.

## Portadas anteriores

Para evitar borrar las antiguas carátulas 3D o tus imágenes personalizadas, las portadas normales descargadas por esta versión se escriben automáticamente en:

`/dev_usb000/BDISO/COVERS/NORMAL/2009 Avatar 3D-001.jpg`

La imagen sigue teniendo el mismo nombre base que su ISO, solo cambia su subcarpeta. SpookyFlow lee en este orden:

1. Imagen manual junto al ISO.
2. Nueva portada normal en `COVERS/NORMAL`.
3. Antigua imagen en `COVERS` como respaldo si no hay conexión.

Así la actualización puede reemplazar las carátulas 3D en pantalla **sin borrar archivos**. Si la carátula manual está dentro de `COVERS`, seguirá existiendo como respaldo; para hacer que tenga prioridad sobre la normal automática se puede colocar una copia junto al ISO en `BDISO/`.

La descarga automática se ejecuta en segundo plano cuando termina el escaneo; también está disponible **Download Missing Covers**. El PKG conserva el lector NTFS y los mismos estuches de SpookyFlow. No necesita cambiar los ISO ni descargar texturas desde el menú de PS3.

## Limitaciones

La disponibilidad e idioma reales dependen de las fuentes externas. Los títulos en español ya localizados en la biblioteca no se modifican. Si no encuentra una portada nueva puede continuar mostrando la carátula antigua; nunca se declara éxito si no se guardó y detectó el JPG. El registro `/dev_hdd0/tmp/spookyflow/diagnostico_portadas.txt` permite verificar el estado real en PS3.
