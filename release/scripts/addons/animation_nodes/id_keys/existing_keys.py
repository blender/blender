from collections import namedtuple
from .. utils.handlers import eventHandler
from .. utils.operators import makeOperator

idKeysInFile = []
unremovableIDKeys = set()

@eventHandler("FILE_LOAD_POST")
@eventHandler("ADDON_LOAD_POST")
@makeOperator("an.update_id_keys_list", "Update ID Key List", redraw = True)
def updateIdKeysList():
    idKeysInFile.clear()
    unremovableIDKeys.clear()

    keys, unremovable = findIDKeysInCurrentFile()

    idKeysInFile.extend(keys)
    unremovableIDKeys.update(unremovable)

def getAllIDKeys():
    return idKeysInFile

def getUnremovableIDKeys():
    return unremovableIDKeys

def findIDKeysInCurrentFile():
    foundKeys = set()
    unremovableKeys = set()

    for findKeys, removable in findIDKeysFunctions:
        keys = findKeys()
        foundKeys.update(keys)
        if not removable:
            unremovableKeys.update(keys)

    # default keys should stay in order
    allKeys = list()
    allKeys.extend(defaultIDKeys)
    unremovableKeys.update(defaultIDKeys)
    for key in foundKeys:
        if key not in allKeys:
            allKeys.append(key)
    return allKeys, unremovableKeys

IDKey = namedtuple("IDKey", ["type", "name"])

defaultIDKeys = [
    IDKey("Transforms", "Initial Transforms"),
    IDKey("Integer", "Index")
]

findIDKeysFunctions = []
def findsIDKeys(removable = True):
    def findsIDKeysDecorator(function):
        findIDKeysFunctions.append((function, removable))
        return function
    return findsIDKeysDecorator

removeIDKeyFunctions = []
def removesIDKey(function):
    removeIDKeyFunctions.append(function)
    return function

@makeOperator("an.remove_id_key", "Remove ID Key", arguments = ["String", "String"],
              redraw = True, confirm = True,
              description = "Remove this ID Key from the whole file.")
def removeIDKey(dataType, propertyName):
    idKey = IDKey(dataType, propertyName)
    for removeFunction in removeIDKeyFunctions:
        removeFunction(idKey)
    updateIdKeysList()
