# Basic tasks
def exportMap(i=0): 
    iface.mapCanvas().saveAsImage( "d:/output/qgis/mapa_" + str(i).zfill(2) + ".png" )



# Load proyect and layers

def loadProject(verbose=False):
    global project,canvas,layerList,project_dir,layers
    project=QgsProject.instance()
    project_dir = QgsProject.instance().readPath("./")
    layerList = project.layerTreeRoot().findLayers()
# Forma usada por chatGpt para cargar las capas (instancias)
# La diferencia principal radica en qué tipo de capas quieres acceder: 
# si quieres todas las capas del proyecto, incluidas las ocultas, usa findLayers(), 
# y si solo quieres las capas que están cargadas en QGIS, usa mapLayers().values().
    layers = project.mapLayers().values()
    for layer in layerList:
        if verbose:
            print(layer.name())
    canvas = iface.mapCanvas()

# 
def loadLayer(name="da_cartografiaBarrioPolygon", order=0):
	return QgsProject.instance().mapLayersByName(name)[order]


loadProject(True)

