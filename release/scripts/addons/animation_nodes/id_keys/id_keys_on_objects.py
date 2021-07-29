import bpy
from . existing_keys import IDKey, findsIDKeys, removesIDKey
from . data_types import dataTypeByIdentifier, dataTypeIdentifiers

@findsIDKeys(removable = True)
def getIDKeysOnObjects():
    possibleKeys = set()
    for object in getAllObjects():
        for key in object.keys():
            if key.startswith("AN*"): possibleKeys.add(key)
    return filterRealIDKeys(possibleKeys)


def filterRealIDKeys(possibleKeys):
    realIDKeys = set()
    for key in possibleKeys:
        parts = key.split("*")
        if len(parts) == 3:
            if parts[1] in dataTypeIdentifiers:
                realIDKeys.add(IDKey(parts[1], parts[2]))
        elif len(parts) == 4:
            if parts[1] in dataTypeIdentifiers:
                realIDKeys.add(IDKey(parts[1], parts[3]))
    return realIDKeys

@removesIDKey
def removeIDKey(idKey):
    typeClass = dataTypeByIdentifier[idKey.type]
    for object in getAllObjects():
        typeClass.remove(object, idKey.name)

def getAllObjects():
    return bpy.data.objects
