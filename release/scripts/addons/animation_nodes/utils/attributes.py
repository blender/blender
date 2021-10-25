import functools

def setattrRecursive(owner, propName, value):
    getAttributeSetter(propName)(owner, value)

def getattrRecursive(owner, propName):
    return getAttributeGetter(propName)(owner)

@functools.lru_cache(maxsize = 1024)
def getAttributeSetter(propName):
    variables = {}
    exec("def attrSetter(owner, value): owner.{} = value".format(propName), variables)
    return variables["attrSetter"]

@functools.lru_cache(maxsize = 1024)
def getAttributeGetter(propName):
    return eval("lambda owner: owner.{}".format(propName))

@functools.lru_cache(maxsize = 1024)
def getMultiAttibuteSetter(propNames):
    code = "def setter(owner, values):\n"
    for i, prop in enumerate(propNames):
        line = getAttributeSetterLine("owner", prop, "values[{}]".format(i))
        code += "    " + line + "\n"
    code += "    pass"
    variables = {}
    exec(code, variables)
    return variables["setter"]

def getAttributeSetterLine(objectName, propName, valueName):
    return "{}.{} = {}".format(objectName, propName, valueName)

def hasEvaluableRepr(value):
    try: return eval(repr(value)) == value
    except: return False

pathArrayCache = {}
def pathBelongsToArray(object, dataPath):
    inputHash = hash((object, dataPath))
    if inputHash not in pathArrayCache:
        result = _pathBelongsToArray(object, dataPath)
        if result is not None:
            pathArrayCache[inputHash] = result
    return pathArrayCache.get(inputHash, None)

def _pathBelongsToArray(object, dataPath):
    if "." in dataPath:
        pathToProperty, propertyName = dataPath.rsplit(".", 1)
        pathToProperty = "." + pathToProperty
    else:
        pathToProperty = ""
        propertyName = dataPath

    try:
        amount = eval("object{}.bl_rna.properties[{}].array_length".format(pathToProperty, repr(propertyName)))
        return amount > 0
    except:
        # Path not found
        return None
