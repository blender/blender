import bpy
from bpy.props import *
from ... base_types import AnimationNode

class ObjectDataPathOutputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectDataPathOutputNode"
    bl_label = "Object Data Path Output"

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Text", "Path", "path")
        self.newInput("Integer", "Array Index", "arrayIndex", value = -1)
        self.newInput("Generic", "Value", "value")
        self.newOutput("Object", "Object", "object")

    def draw(self, layout):
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def drawAdvanced(self, layout):
        self.invokeFunction(layout, "clearCache", text = "Clear Cache")

    def execute(self, object, path, arrayIndex, value):
        if object is None: return object
        setAttributeFunction = getSetFunction(object, path)
        if setAttributeFunction is None: return object
        try:
            setAttributeFunction(object, arrayIndex, value)
            self.errorMessage = ""
        except:
            self.errorMessage = "Error"
        return object
    
    def getPropertyPath(self, object, path):
        if "." in path:
            propPath, propName = path.rsplit(".", 1)
            try:
                dataPath = object.path_resolve(propPath)
            except:
                dataPath = object
        else:
            dataPath = object
            propName = path
        return dataPath, propName
    
    def getBakeCode(self):
        yield "if object is not None:"
        yield "    try: object.keyframe_insert(path, index = arrayIndex)"
        yield "    except:"
        yield "        dataPath, propName = self.getPropertyPath(object, path)"
        yield "        try: dataPath.keyframe_insert(propName, index = arrayIndex)"
        yield "        except: pass"
    
    def clearCache(self):
        cache.clear()

cache = {}

def getSetFunction(object, attribute):
    if attribute in cache: return cache[attribute]

    function = createSetFunction(object, attribute)
    cache[attribute] = function
    return function

def createSetFunction(object, dataPath):
    needsIndex = dataPathBelongsToArray(object, dataPath)
    if needsIndex is None: return None
    data = {}
    if needsIndex:
        exec(setAttributeWithIndex.replace("#dataPath#", dataPath), data, data)
        return data["setAttributeWithIndex"]
    else:
        exec(setAttributeWithoutIndex.replace("#dataPath#", dataPath), data, data)
        return data["setAttributeWithoutIndex"]

setAttributeWithIndex = '''
def setAttributeWithIndex(object, index, value):
    object.#dataPath#[index] = value
'''

setAttributeWithoutIndex = '''
def setAttributeWithoutIndex(object, index, value):
    object.#dataPath# = value
'''

def dataPathBelongsToArray(object, dataPath):
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
        # Means that the property has not been found
        return None
