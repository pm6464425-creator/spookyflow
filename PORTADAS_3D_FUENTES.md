# SpookyFlow V14 — CARTEL DE PELÍCULA, no caja Blu-ray

## Corrección del problema de V13

La fuente «2D-ES-Mubis» publicaba imágenes de carátulas de ediciones Blu-ray. Aunque no fueran 3D, incluían el **estuche fotografiado dentro del JPG**, que SpookyFlow volvía a meter dentro de su propio estuche.

**V14 elimina por completo esa fuente** y ya no muestra las imágenes descargadas de ediciones Blu-ray en V11–V13. Se conservan en el USB; no se borran ni modifican.

## De dónde vienen las portadas nuevas

1. **Primero SensaCine España:** busca el título de la película y recoge exclusivamente el JPG de la entrada etiquetada «Cartel de [título]». Acepta nombres completos o una traducción conocida; rechaza secuelas que no coincidan, imágenes de actores, fotos de rodaje, fotogramas, carteles genéricos y sitios externos no autorizados. Es un **póster cinematográfico plano**, como el Avatar de ejemplo.
2. Si está configurada una clave **TMDB**, los pósteres de cine en español de TMDB son una segunda fuente.
3. Si SensaCine o TMDB no pueden ofrecer ese cartel, el programa utiliza carteles de cine de otros catálogos (MoviePosterDB, fanart.tv, OMDb) o **Cinemeta sin clave** como respaldo. El idioma puede ser inglés.
4. El texto «3D» aparece únicamente como **marca de agua discreta en el extremo izquierdo del estuche**, frente a «Blu-ray» a la derecha, y solo para las películas cuyos ISO indican 3D. No hay bandas ni cajas en el propio JPG.

## Carpeta nueva y migración automática

Los carteles descargados se guardan en:

`/dev_usb000/BDISO/COVERS/CINE/2009 Avatar 3D-001.jpg`

Esa ruta mantiene el mismo nombre del ISO, pero **no reutiliza** la carpeta NORMAL de V13 porque allí había fotos de productos Blu-ray.

Para utilizar tu propia imagen, puedes guardar un JPG/PNG plano junto al ISO, por ejemplo `/dev_usb000/BDISO/2009 Avatar 3D-001.jpg`, o ponerlo en `BDISO/COVERS/MANUAL/` con el mismo nombre. La imagen manual tiene prioridad sobre la descargada.

Los JPG antiguos en `BDISO/COVERS/` y `BDISO/COVERS/NORMAL/` **se conservan en el disco, pero esta versión no los carga** porque pueden tener estuche. No es necesario borrarlos ni reinstalar ISO. SpookyFlow busca nuevamente las portadas de las películas en su primera ejecución o al pulsar «Download Missing Covers».

## Diagnóstico

Si no puede obtener una imagen, no copia de nuevo el viejo JPG con caja para fingir que la carátula está arreglada. Se muestra la tarjeta con el título y se mantiene el informe real de la consola:

`/dev_hdd0/tmp/spookyflow/diagnostico_portadas.txt`

La disponibilidad de SensaCine es externa y no se garantiza si la web cambia o hay problemas de conexión en PS3. El programa conserva las protecciones de rutas, montaje NTFS y lectura de las películas originales.
