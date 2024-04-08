layers = project.mapLayers().values()
project_crs = QgsProject.instance().crs()
for layer in layers:
    # Comprobar si la capa termina en "_buffer"
    if layer.name().endswith("_buffer"):
        # Calcular el bounding box
        bbox = layer.extent()

        # Crear la ruta para guardar el archivo shape
        output_shapefile = os.path.join(project_dir, f"{layer.name()}_bbox.shp")

        # Crear el archivo shape y el escritor de capa
        writer = QgsVectorFileWriter(output_shapefile, "UTF-8", QgsFields(), QgsWkbTypes.Polygon, project_crs, "ESRI Shapefile")

        # Crear la geometría del bounding box
        bbox_geometry = QgsRectangle(bbox)
        bbox_polygon = [
            QgsPointXY(bbox_geometry.xMinimum(), bbox_geometry.yMinimum()),
            QgsPointXY(bbox_geometry.xMinimum(), bbox_geometry.yMaximum()),
            QgsPointXY(bbox_geometry.xMaximum(), bbox_geometry.yMaximum()),
            QgsPointXY(bbox_geometry.xMaximum(), bbox_geometry.yMinimum())
        ]
        bbox_polygon.append(bbox_polygon[0])  # Cerrar el polígono

        # Crear la característica y agregarla al escritor de capa
        feature = QgsFeature()
        feature.setGeometry(QgsGeometry.fromPolygonXY([bbox_polygon]))
        feature.setAttributes([1])  # Cambia esto si necesitas diferentes atributos
        writer.addFeature(feature)

        # Cerrar el escritor de capa
        del writer