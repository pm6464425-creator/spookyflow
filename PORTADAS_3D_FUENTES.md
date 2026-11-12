# SpookyFlow V15 — Ajuste BDISO 2:3 y reconocimiento estricto

## Corrección 1: estuche y carátula sin franjas

El código anterior dibujaba los carteles de cine en una ventana de proporción aproximada **260:300 (0,867)**, demasiado ancha para un cartel cinematográfico. Si la diferencia superaba el 20 %, añadía franjas oscuras.

**V15 cambia únicamente la geometría de BDISO:** el área visible es ahora **2:3 (0,667)**. El estuche exterior se estrecha proporcionalmente. Se sigue reutilizando la misma textura de estuche Blu-ray sin tocar los estuches de PS1, PS2 o PS3.

La imagen se adapta mediante **recorte centrado proporcional**: llena todo el espacio sin bordes añadidos y sin deformar ni estirar actores ni letras. Si un cartel tiene una proporción diferente, se recorta un poco de los lados o de arriba/abajo. Si la fotografía original ya trae franjas negras impresas dentro, el recorte no garantiza eliminarlas.

Conserva las discretas marcas «Blu-ray» y «3D» sobre el estuche; no las dibuja dentro del JPG.

## Corrección 2: identificar la película correcta, no palabras parecidas

**Río, 2011** (Carlos Saldanha, Blue Sky Studios) es una película distinta de **Extraño río, 2025**. V14 podía encontrar la segunda porque la búsqueda aceptaba coincidencias por terminación en títulos cortos y el listado de SensaCine muestra numerosos resultados antes de Río.

**V15:**
- Extrae el año desde el nombre del ISO, sin confundir el sufijo `3D-001` con el año.
- El nombre `Rio` (o `Río`) sin año se identifica como la película de **2011** para esta biblioteca.
- Utiliza la ficha cinematográfica original de SensaCine: `https://www.sensacine.com/peliculas/pelicula-146550/` para Río, y verifica el título y año.
- Para otros títulos usa coincidencia exacta de nombre y comprueba el año que aparece en el resultado de SensaCine. No acepta otra película porque termine en la misma palabra.
- También verifica **título y año** de los resultados de TMDB antes de descargar sus carteles; Cinemeta continúa comprobando el año.
- Si una coincidencia no puede verificarse, prefiere no asignar una carátula equivocada.

## Carpeta nueva V15

Las nuevas portadas cinematográficas se escriben en:

`/dev_usb000/BDISO/COVERS/POSTERS/2011 Rio 3D-001.jpg`

Los carteles anteriores de V14 en `BDISO/COVERS/CINE` no tienen prioridad ni bloquean la búsqueda y los JPG de V11–V13 con caja Blu-ray tampoco. Ninguno de los archivos anteriores se borra.

Una imagen manual personalizada colocada junto al ISO o en `BDISO/COVERS/MANUAL` sigue teniendo prioridad. Conserva el mismo nombre de la película sin `.iso`.

Las portadas de cine normales vienen de SensaCine en español; TMDB en español, Cinemeta y otros proveedores siguen como respaldo. El acceso a internet depende de la conexión de PS3 y de las fuentes externas.

## Prueba

Instala el PKG, abre BDISO, ejecuta **Download Missing Covers** y comprueba especialmente `Río (2011)`. La ruta de diagnóstico sigue disponible:

`/dev_hdd0/tmp/spookyflow/diagnostico_portadas.txt`

Las pruebas de compilación y los tests de C no sustituyen la verificación visual en una PS3 física.
