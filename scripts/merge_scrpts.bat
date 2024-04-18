

rem borramos el raster destino y los metadatos de qgis para evitar confusión al viasualizar max min
del d:\datos\input\rutaMulhacen.tif.aux.xml
del d:\datos\input\rutaMulhacen.tif
rem copiamos el raster sobre el que vamos a trabajar (extension resolucion y crs) pero con valor 0
c:\OSGeo4W\bin\gdal_create.exe -burn 0 -if d:\datos\input\bak\SierraNevada.tif d:\datos\input\rutaMulhacen.tif
rem quemamos el shape con valor 1
C:\OSGeo4W\bin\gdal_rasterize.exe -burn 1  -l rutaMulhacen d:/datos/input/rutaMulhacen.shp d:/datos/input/rutaMulhacen.tif
rem hacemos el merge en dos pasos (primero, creamos un rater virtual de dos bandas con dos tif)
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\rutaMulhacen.vrt -separate d:\datos\input\bak\SierraNevada.tif d:\datos\input\rutaMulhacen.tif
rem convertirmos el rater virtual en un raster con dos bandas
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\rutaMulhacen.vrt   d:\datos\input\rutaMulhacen_merge.tif

rem borramos el raster destino y los metadatos de qgis para evitar confusión al viasualizar max min
del d:\datos\input\Teide.tif.aux.xml
del d:\datos\input\Teide.tif
rem copiamos el raster sobre el que vamos a trabajar (extension resolucion y crs) pero con valor 0
c:\OSGeo4W\bin\gdal_create.exe -burn 0 -if d:\datos\input\bak\Teide.tif d:\datos\input\Teide.tif
rem quemamos el shape con valor 1
C:\OSGeo4W\bin\gdal_rasterize.exe -burn 1  -l Teide d:\onedrive\proyectos\ppnn\limites_red\Teide.shp d:/datos/input/Teide.tif
rem hacemos el merge en dos pasos (primero, creamos un rater virtual de dos bandas con dos tif)
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\Teide.vrt -separate d:\datos\input\bak\Teide.tif d:\datos\input\Teide.tif
rem convertirmos el rater virtual en un raster con dos bandas
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\Teide.vrt   d:\datos\input\Teide_merge.tif


copy d:\datos\input\bak\SierraNevada.tif d:\datos\input\SierraNevada.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 1  -l SierraNevada d:\onedrive\proyectos\ppnn\limites_red\SierraNevada.shp d:\datos\input\SierraNevada.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 0  -l SierraNevada d:\onedrive\proyectos\ppnn\limites_red\SierraNevada.shp d:\datos\input\SierraNevada.tif
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\SierraNevada.vrt -separate d:\datos\input\bak\SierraNevada.tif d:\datos\input\SierraNevada.tif
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\SierraNevada.vrt   d:\datos\input\SierraNevada_merge.tif

copy d:\datos\input\bak\SierraNieves.tif d:\datos\input\SierraNieves.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 1  -l SierraNieves d:\onedrive\proyectos\ppnn\limites_red\SierraNieves.shp d:\datos\input\SierraNieves.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 0  -l SierraNieves d:\onedrive\proyectos\ppnn\limites_red\SierraNieves.shp d:\datos\input\SierraNieves.tif
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\SierraNieves.vrt -separate d:\datos\input\bak\SierraNieves.tif d:\datos\input\SierraNieves.tif
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\SierraNieves.vrt   d:\datos\input\SierraNieves_merge.tif


copy d:\datos\input\bak\AiguesTortes.tif d:\datos\input\AiguesTortes.tif
C:\OSGeo4W\bin\gdal_rasterize.exe  -burn 1  -l AiguesTortes d:\onedrive\proyectos\ppnn\limites_red\AiguesTortes.shp d:\datos\input\AiguesTortes.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 0  -l AiguesTortes d:\onedrive\proyectos\ppnn\limites_red\AiguesTortes.shp d:\datos\input\AiguesTortes.tif
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\AiguesTortes.vrt -separate d:\datos\input\bak\AiguesTortes.tif d:\datos\input\AiguesTortes.tif
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\AiguesTortes.vrt   d:\datos\input\AiguesTortes_merge.tif

copy d:\datos\input\bak\Picos.tif d:\datos\input\Picos.tif
C:\OSGeo4W\bin\gdal_rasterize.exe  -burn 1  -l Picos d:\onedrive\proyectos\ppnn\limites_red\Picos.shp d:\datos\input\Picos.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 0  -l Picos d:\onedrive\proyectos\ppnn\limites_red\Picos.shp d:\datos\input\Picos.tif
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\Picos.vrt -separate d:\datos\input\bak\Picos.tif d:\datos\input\Picos.tif
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\Picos.vrt   d:\datos\input\Picos_merge.tif

copy d:\datos\input\bak\Teide.tif d:\datos\input\Teide.tif
C:\OSGeo4W\bin\gdal_rasterize.exe  -burn 1  -l Teide d:\onedrive\proyectos\ppnn\limites_red\Teide.shp d:\datos\input\Teide.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 0  -l Teide d:\onedrive\proyectos\ppnn\limites_red\Teide.shp d:\datos\input\Teide.tif
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\Teide.vrt -separate d:\datos\input\bak\Teide.tif d:\datos\input\Teide.tif
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\Teide.vrt   d:\datos\input\Teide_merge.tif

copy d:\datos\input\bak\Guadarrama.tif d:\datos\input\Guadarrama.tif
C:\OSGeo4W\bin\gdal_rasterize.exe  -burn 1  -l Guadarrama d:\onedrive\proyectos\ppnn\limites_red\Guadarrama.shp d:\datos\input\Guadarrama.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 0  -l Guadarrama d:\onedrive\proyectos\ppnn\limites_red\Guadarrama.shp d:\datos\input\Guadarrama.tif
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\Guadarrama.vrt -separate d:\datos\input\bak\Guadarrama.tif d:\datos\input\Guadarrama.tif
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\Guadarrama.vrt   d:\datos\input\Guadarrama_merge.tif

copy d:\datos\input\bak\Peneda.tif d:\datos\input\Peneda.tif
C:\OSGeo4W\bin\gdal_rasterize.exe  -burn 1  -l Peneda d:\onedrive\proyectos\ppnn\limites_red\Peneda.shp d:\datos\input\Peneda.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 0  -l Peneda d:\onedrive\proyectos\ppnn\limites_red\Peneda.shp d:\datos\input\Peneda.tif
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\Peneda.vrt -separate d:\datos\input\bak\Peneda.tif d:\datos\input\Peneda.tif
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\Peneda.vrt   d:\datos\input\Peneda_merge.tif

copy d:\datos\input\bak\Ordesa.tif d:\datos\input\Ordesa.tif
C:\OSGeo4W\bin\gdal_rasterize.exe  -burn 1  -l Ordesa d:\onedrive\proyectos\ppnn\limites_red\Ordesa.shp d:\datos\input\Ordesa.tif
C:\OSGeo4W\bin\gdal_rasterize.exe -i -burn 0  -l Ordesa d:\onedrive\proyectos\ppnn\limites_red\Ordesa.shp d:\datos\input\Ordesa.tif
C:\OSGeo4W\bin\gdalbuildvrt.exe d:\datos\input\bak\Ordesa.vrt -separate d:\datos\input\bak\Ordesa.tif d:\datos\input\Ordesa.tif
C:\OSGeo4W\bin\gdal_translate  d:\datos\input\bak\Ordesa.vrt   d:\datos\input\Ordesa_merge.tif






