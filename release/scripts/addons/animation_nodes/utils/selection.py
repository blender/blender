import bpy
from . handlers import eventHandler

enableSelectionSorting = True
sortedSelectionNames = []

def getSortedSelectedObjects():
    objects = []
    for name in getSortedSelectedObjectNames():
        objects.append(bpy.data.objects.get(name))
    return objects

def getSortedSelectedObjectNames():
    return sortedSelectionNames

@eventHandler("SCENE_UPDATE_POST")
def updateSelectionSorting(scene):
    global sortedSelectionNames

    selectedNames = getSelectedObjectNames()

    if enableSelectionSorting:
        newSortedSelection = []

        selectedNamesSet = set(selectedNames)
        for name in sortedSelectionNames:
            if name in selectedNamesSet:
                newSortedSelection.append(name)

        for name in selectedNames:
            if name not in newSortedSelection:
                newSortedSelection.append(name)
                
        sortedSelectionNames = newSortedSelection
    else:
        sortedSelectionNames = selectedNames

def getSelectedObjectNames():
    selectedNames = []
    for object in getattr(bpy.context, "selected_objects", []):
        selectedNames.append(object.name)
    return selectedNames
