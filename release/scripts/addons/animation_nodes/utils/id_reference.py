import bpy

hashByObjectName = {}

def updateObjectReference(object):
    hashByObjectName[object.name] = hash(object)

def tryToFindObjectReference(name):
    object = bpy.data.objects.get(name)
    if object is not None:
        updateObjectReference(object)
        return object

    savedHash = hashByObjectName.get(name, None)
    if savedHash is None: return None

    for object in bpy.data.objects:
        if hash(object) == savedHash:
            updateObjectReference(object)
            return object

    return None
