# Portadas BDISO automáticas para SpookyFlow

## Comportamiento

1. Si ya existe una carátula manual, se utiliza sin descargar otra. Compatible
   con JPG/PNG junto al ISO o en «BDISO/COVERS», por ejemplo
   «BDISO/COVERS/2009 Avatar 3D-001.jpg».
2. Para una película marcada como 3D, se busca una **carátula frontal de una
   edición Blu-ray 3D española** en Mubis, que publica esas ediciones.
   Como respaldo, se consultan algunas ediciones 3D españolas verificadas
   de Blu-ray.com.
3. Si falta la española, se intentan **carátulas 3D inglesas** de ediciones
   de Blu-ray.com verificadas (título y año). Hay una tabla de ediciones
   conocidas: no es un buscador universal.
4. Si falla la búsqueda 3D, se utiliza una **portada normal en español**:
   TMDB (si hay clave personal) y ediciones españolas normales de Mubis.
5. Finalmente se busca la **portada normal en inglés**, mediante otras
   fuentes configuradas o Cinemeta sin clave.

Las imágenes se descargan como JPG frontal, **sin marco/estuche incluido
artificialmente por la app**. El estuche PS3/Blu-ray de SpookyFlow queda
exactamente como antes: no se agrega franja ni texto «BLU-RAY 3D» sobre
él. Una ficha comercial marcada 3D es una referencia de edición, pero
no hay OCR de la propia foto: algunos diseños pueden no llevar un sello 3D
visible en la imagen. Una portada manual tiene prioridad siempre.

## Almacenamiento

Con «/dev_usb000/BDISO/2009 Avatar 3D-001.iso» se guarda:

  /dev_usb000/BDISO/COVERS/2009 Avatar 3D-001.jpg

La misma imagen se reconoce al volver a abrir SpookyFlow. No se cambia
el nombre de la ISO ni se escriben portadas originales en el HDD interno.
Si ya has copiado una foto manual junto al ISO con el mismo nombre sin
«.iso», SpookyFlow también la lee.

El código anterior creaba ficheros «.sources.txt» con enlaces a foros:
**ya no se crean esos archivos**. No son imágenes.

## Fuentes y claves

- Ediciones 3D españolas: https://www.mubis.es/
- Ediciones 3D españolas/inglesas verificadas: https://www.blu-ray.com/
- Póster normal en español opcional: https://www.themoviedb.org/
- Otros pósteres opcionales: https://fanart.tv/ , https://www.omdbapi.com/
- Último recurso sin clave: https://v3-cinemeta.strem.io/

«TMDB_KEY.TXT», «FANART_KEY.TXT», «OMDB_KEY.TXT» y
«MOVIEPOSTERDB_KEY.TXT» siguen siendo opcionales y nunca están
incluidos en el PKG. Las conexiones a sitios externos pueden fallar
o cambiar. Se respetan las restricciones del proveedor de imágenes;
no se imitan credenciales, se fuerzan bloqueos ni se rehostean portadas.

## Pruebas

Las pruebas incluyen la selección de ediciones originales 3D,
rechazo de imágenes no confiables, conservación de carátulas manuales,
títulos en español, decodificación de JPEG progresivo, NTFS y
compilación PS3. **La reproducción y conexión reales deben
comprobarse en la consola del usuario**.
