# Basic tasks
def exportMap(i=0): 
    iface.mapCanvas().saveAsImage( "d:/output/qgis/mapa_" + str(i).zfill(2) + ".png" )



# Load proyect and layers

def loadProject(verbose=False):
    global project,canvas,layerList
    project=QgsProject.instance()
    layerList = project.layerTreeRoot().findLayers()
    for layer in layerList:
        if verbose:
            print(layer.name())
    canvas = iface.mapCanvas()


def loadLayer(name="da_cartografiaBarrioPolygon", order=0):
	return QgsProject.instance().mapLayersByName(name)[order]


loadProject(True)