# Portadas BDISO 3D: cuatro proveedores y comunidades

Esta integración amplía el buscador de posters de la versión «MI PKG V2».
No cambia el lector NTFS, el montaje Cobra, el carrusel, el estuche PS3
ni el logotipo Blu-ray. **La coincidencia exacta de edición comercial
3D no puede garantizarse por el nombre de la película solamente.**

## Descarga automática de pósteres (prioridad)

1. **TMDB** — se usa primero el póster en español, después su póster
   estándar si no hay más alternativas. Necesita la clave que ya
   utilizaba la aplicación: «BDISO/COVERS/TMDB_KEY.TXT».
2. **Fanart.tv** — posters de su API oficial mediante el TMDB ID.
   Clave opcional de **proyecto**:
   «BDISO/COVERS/FANART_KEY.TXT».
   Si dispones de una clave **personal**, colócala en
   «BDISO/COVERS/FANART_PERSONAL_KEY.TXT».
   La API de fanart.tv usa «client_key» para las claves personales.
3. **OMDb** — posters registrados en su API, como respaldo.
   Clave opcional «BDISO/COVERS/OMDB_KEY.TXT».
   *El servicio específico de posters de alta resolución de OMDb
   requiere un plan autorizado; las URLs devueltas por la API pueden
   estar limitadas por el proveedor.*
4. **MoviePosterDB** — URL estable por identificador IMDb;
   «BDISO/COVERS/MOVIEPOSTERDB_KEY.TXT» con token propio
   «mpdb_pk_...». La cuenta gratuita puede limitar la resolución
   a unos 200 píxeles, por eso se usa como último respaldo.

La búsqueda requiere una clave de metadatos de TMDB y/o OMDb. Las
claves no están incluidas en el repositorio/PKG y no se deben
compartir públicamente.

En la aplicación se respetan, por orden: **la portada elegida
manualmente** y la portada descargada ya existente. El proceso no
sobrescribe deliberadamente las portadas guardadas.

## Ediciones Blu-ray 3D y foros

La API de TMDB y los proveedores de posters no documentan de manera
fiable todas las ediciones comerciales 3D. Para títulos cuyo archivo
ISO incluya «3D», la aplicación crea:

  BDISO/COVERS/<nombre exacto de la ISO sin .iso>.sources.txt

Incluye enlaces de búsqueda a estas fuentes:
- Catálogo de lanzamientos: https://www.blu-ray.com/
- Foro de carátulas: https://forum.blu-ray.com/showthread.php?t=100007
- Comunidad de diseño de carátulas: https://www.covercity.net/
- Foro español con apartado BluRay 3D: https://www.swarzycustom.com/

Se usan búsquedas web por título, año y «3D Blu-ray»; **la PS3 no
realiza scraping de foros**, descarga imágenes protegidas ni presume
que una carátula genérica es la edición 3D exacta. Si escoges una imagen
de esas comunidades, verifica permisos y coloca el archivo JPG en:

  BDISO/COVERS/<nombre exacto de la ISO sin .iso>.jpg

Por ejemplo, «Avatar (2009) 3D.iso» usa
«BDISO/COVERS/Avatar (2009) 3D.jpg».

La descarga automática guarda en el **mismo disco USB** los JPG
y títulos (archivos .title), sin usar el HDD interno para arte original.

## Fuentes públicas oficiales (consulta)

- TMDB: https://developer.themoviedb.org/reference/movie-images
- fanart.tv: https://api.fanart.tv/
- OMDb: https://www.omdbapi.com/
- MoviePosterDB: https://movieposterdb.com/embed/docs

**Estado de las pruebas**: el CI verifica compilación e invariantes
de integración. El acierto de portada por proveedor, autenticación
personal, conexión HTTPS desde PS3 y montaje real siguen requiriendo
una prueba en consola y con las claves del propietario.
