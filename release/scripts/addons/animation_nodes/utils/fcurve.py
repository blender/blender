import bpy
from . names import toDataPath


# Misc
###########################

def deselectAllFCurves(object):
    for fcurve in getAllFCurves(object):
        fcurve.select = False

def newFCurveForCustomProperty(object, propertyName, defaultValue):
    removeCustomProperty(object, propertyName)
    object[propertyName] = defaultValue
    object.keyframe_insert(frame = 0, data_path = toDataPath(propertyName))

def removeCustomProperty(object, propertyName):
    if propertyName in object:
        del object[propertyName]
    removeFCurveWithDataPath(object, toDataPath(propertyName))



# get value in one frame
###########################

def getArrayValueAtFrame(object, dataPath, frame, arraySize = 3):
    fCurves = getFCurvesWithDataPath(object, dataPath)
    values = [0] * arraySize
    for index in range(arraySize):
        fCurve = getFCurveWithIndex(fCurves, index)
        if fCurve is None: values[index] = getattr(object, dataPath)[index]
        else: values[index] = fCurve.evaluate(frame)
    return values

def getSingleValueAtFrame(object, dataPath, frame):
    fCurves = getFCurvesWithDataPath(object, dataPath)
    if len(fCurves) == 0:
        return getattr(object, dataPath)
    return fCurves[0].evaluate(frame)

def getSingleValueOfArrayAtFrame(object, dataPath, index, frame):
    fCurves = getFCurvesWithDataPath(object, dataPath)
    fCurve = getFCurveWithIndex(fCurves, index)
    if fCurve is None: return getattr(object, dataPath)[index]
    return fCurve.evaluate(frame)

def getMultipleValuesOfArrayAtFrame(object, dataPath, indices, frame):
    fCurves = getFCurvesWithDataPath(object, dataPath)
    values = [0] * len(indices)
    for i, index in enumerate(indices):
        fCurve = getFCurveWithIndex(fCurves, index)
        if fCurve is None: values[i] = getattr(object, dataPath)[index]
        else: values[i] = fCurve.evaluate(frame)
    return values



# get values of multiple frames
###################################

def getArrayValueAtMultipleFrames(object, dataPath, frames, arraySize = 3):
    values = [0] * len(frames)
    for i in range(len(frames)): values[i] = [0] * arraySize
    fCurves = getFCurvesWithDataPath(object, dataPath)
    for index in range(arraySize):
        fCurve = getFCurveWithIndex(fCurves, index)
        if fCurve is None:
            value = getattr(object, dataPath)[index]
            for i, frame in enumerate(frames):
                values[i][index] = value
        else:
            for i, frame in enumerate(frames):
                values[i][index] = fCurve.evaluate(frame)
    return values

def getFCurveWithIndex(fCurves, index):
    for fCurve in fCurves:
        if fCurve.array_index == index: return fCurve
    return None



# remove fcurves
########################

def removeFCurveWithDataPath(object, dataPath):
    fcurve = getSingleFCurveWithDataPath(object, dataPath)
    try: object.animation_data.action.fcurves.remove(fcurve)
    except: pass



# search fcurves
########################

cache = {}

def clearCache():
    cache.clear()

def getFCurvesWithDataPath(object, dataPath, storeInCache = True):
    identifier = (object.type, object.name, dataPath)
    if identifier in cache: return cache[identifier]

    fCurves = []
    for fCurve in getAllFCurves(object):
        if fCurve.data_path == dataPath:
            fCurves.append(fCurve)
    cache[identifier] = fCurves
    return fCurves


def getSingleFCurveWithDataPath(object, dataPath, storeInCache = True):
    identifier = (object.type, object.name, dataPath, "first")
    if identifier in cache: return cache[identifier]

    for fCurve in getAllFCurves(object):
        if fCurve.data_path == dataPath:
            cache[identifier] = fCurve
            return fCurve



# get fcurves
######################

def getAllFCurves(object):
    try: return object.animation_data.action.fcurves
    except: return []
