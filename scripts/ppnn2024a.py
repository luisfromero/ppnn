

# Forma usada por chatGpt para cargar las capas (nstancias)
layers = project.mapLayers().values()
project_dir = QgsProject.instance().readPath("./")

for layer in layers:
    # Comprobar si la capa es un shapefile
    if isinstance(layer, QgsVectorLayer) and layer.dataProvider().name() == "ogr"  and layer.name().startswith("Shape"):
        # Crear el nombre del archivo de salida para el buffer
        output_file = os.path.join(project_dir, f"{os.path.basename(layer.name())}_buffer.shp")
        
        # Calcular el buffer de 60 km
        processing.run("native:buffer", {
            'INPUT': layer,
            'DISTANCE': 60000,  # 60 km en metros
            'SEGMENTS': 5,  # Número de segmentos para aproximar el círculo
            'END_CAP_STYLE': 0,  # Estilo de la tapa final del buffer
            'JOIN_STYLE': 0,  # Estilo de unión de los segmentos
            'MITER_LIMIT': 2,  # Límite de inglete para vértices agudos
            'DISSOLVE': False,  # No disolver los buffers
            'OUTPUT': output_file
        })
        
        print(f"Buffer de 60km creado para la capa {os.path.basename(layer.name())} en {output_file}")
    else:
        print(isinstance(layer, QgsVectorLayer))
        
        
