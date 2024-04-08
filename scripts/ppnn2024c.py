input_tiff = "d:/datos/ppnn/DEM_3035_30m.tif"
for layer in layers:
    # Comprobar si la capa termina en "_buffer"
    if layer.name().endswith("_buffer"):
        # Calcular el bounding box
        bbox = layer.extent()

        # Crear una capa de memoria para el bounding box
        fields = QgsFields()
        fields.append(QgsField("id", QVariant.Int))
        tmp="d:/datos/input/"+os.path.basename(layer.name()).split('.')[0]+".tif"
        print(tmp)
        bbox_layer = QgsVectorLayer("Polygon", f"{layer.name()}_bbox", "memory")
        bbox_layer_provider = bbox_layer.dataProvider()
        bbox_layer_provider.addAttributes(fields)
        bbox_layer.updateFields()

        # Crear la geometría del bounding box
        bbox_geometry = QgsRectangle(bbox)
        bbox_polygon = [
            QgsPointXY(bbox_geometry.xMinimum(), bbox_geometry.yMinimum()),
            QgsPointXY(bbox_geometry.xMinimum(), bbox_geometry.yMaximum()),
            QgsPointXY(bbox_geometry.xMaximum(), bbox_geometry.yMaximum()),
            QgsPointXY(bbox_geometry.xMaximum(), bbox_geometry.yMinimum())
        ]
        bbox_polygon.append(bbox_polygon[0])  # Cerrar el polígono
        x_min = bbox_geometry.xMinimum()
        y_min = bbox_geometry.yMinimum()
        x_max = bbox_geometry.xMaximum()
        y_max = bbox_geometry.yMaximum()
        output_tiff = tmp

        print(f"Bounding box de {layer.name()}:")
        print(f"   Min X: {bbox.xMinimum()}")
        print(f"   Min Y: {bbox.yMinimum()}")
        print(f"   Max X: {bbox.xMaximum()}")
        print(f"   Max Y: {bbox.yMaximum()}")
        command = f"gdalwarp -te {x_min} {y_min} {x_max} {y_max} {input_tiff} {output_tiff}"

        # Ejecutar el comando
        os.system(command)