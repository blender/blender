import bpy
from bpy.props import *
from ... math import composeMatrixList
from ... tree_info import getNodesByType
from ... base_types import VectorizedNode
from ... id_keys import keyDataTypeItems, IDKey, findsIDKeys, updateIdKeysList

class ObjectIDKeyNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ObjectIDKeyNode"
    bl_label = "Object ID Key"
    bl_width_default = 160

    def keyChanged(self, context):
        updateIdKeysList()
        self.refresh()

    searchTags = [("Object Initial Transforms",
                   {"keyDataType" : repr("Transforms"),
                    "keyName" : repr("Initial Transforms")})]

    keyDataType = EnumProperty(name = "Key Data Type", default = "Transforms",
        items = keyDataTypeItems, update = keyChanged)

    keyName = StringProperty(name = "Key Name", default = "",
        update = keyChanged)

    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Object", "useList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"))

        if self.keyName != "":
            if self.keyDataType == "Transforms":
                self.newVectorizedOutput("Vector", "useList",
                    ("Location", "location"), ("Locations", "locations"))
                self.newVectorizedOutput("Euler", "useList",
                    ("Rotation", "rotation"), ("Rotations", "rotations"))
                self.newVectorizedOutput("Vector", "useList",
                    ("Scale", "scale"), ("Scales", "scales"))
                self.newVectorizedOutput("Matrix", "useList",
                    ("Matrix", "matrix"), ("Matrices", "matrices"))
            elif self.keyDataType == "Text":
                self.newVectorizedOutput("Text", "useList",
                    ("Text", "text"), ("Texts", "texts"))
            elif self.keyDataType == "Integer":
                self.newVectorizedOutput("Integer", "useList",
                    ("Number", "number"), ("Numbers", "numbers"))
            elif self.keyDataType == "Float":
                self.newVectorizedOutput("Float", "useList",
                    ("Number", "number"), ("Numbers", "numbers"))

            self.newVectorizedOutput("Boolean", "useList",
                ("Exists", "exists", dict(hide = True)),
                ("Exists", "exists", dict(hide = True)))

    def drawAdvanced(self, layout):
        col = layout.column()
        col.prop(self, "keyDataType", text = "Type")
        col.prop(self, "keyName", text = "Name")

    def draw(self, layout):
        col = layout.column()
        col.scale_y = 1.5
        text = "Choose ID Key" if self.keyName == "" else repr(self.keyName)
        self.invokeSelector(col, "ID_KEY", "assignIDKey",
            text = text, icon = "VIEWZOOM")

    def assignIDKey(self, dataType, name):
        self.keyDataType = dataType
        self.keyName = name

    def getExecutionCode(self):
        if self.keyName == "":
            return

        keyName = repr(self.keyName)
        yield "_key = animation_nodes.id_keys.IDKeyTypes[{}]".format(repr(self.keyDataType))
        if self.useList:
            yield from self.getExecutionCode_List(keyName)
        else:
            yield from self.getExecutionCode_Base(keyName)

    def getExecutionCode_Base(self, keyName):
        dataType = self.keyDataType
        isLinked = self.getLinkedOutputsDict()

        if isLinked["exists"]:
            yield "exists = _key.exists(object, %s)" % keyName

        yield "data = _key.get(object, %s)" % keyName

        if dataType == "Transforms":
            yield "location, rotation, scale = data"
            if isLinked["matrix"]:
                yield "matrix = animation_nodes.utils.math.composeMatrix(location, rotation, scale)"
        elif dataType == "Text":
            yield "text = data"
        elif dataType in ("Integer", "Float"):
            yield "number = data"

    def getExecutionCode_List(self, keyName):
        dataType = self.keyDataType
        isLinked = self.getLinkedOutputsDict()

        if isLinked["exists"]:
            yield "exists = _key.existsList(objects, %s)" % keyName

        if dataType == "Transforms":
            useMatrices = isLinked["matrices"]
            if isLinked["locations"] or useMatrices:
                yield "locations = _key.getLocations(objects, %s)" % keyName
            if isLinked["rotations"] or useMatrices:
                yield "rotations = _key.getRotations(objects, %s)" % keyName
            if isLinked["scales"] or useMatrices:
                yield "scales = _key.getScales(objects, %s)" % keyName
            if useMatrices:
                yield "matrices = animation_nodes.math.composeMatrixList(locations, rotations, scales)"
        elif dataType == "Text":
            if isLinked["texts"]:
                yield "texts = _key.getList(objects, %s)" % keyName
        elif dataType in ("Integer", "Float"):
            if isLinked["numbers"]:
                yield "numbers = _key.getList(objects, %s)" % keyName

    def getList_Exists(self, objects):
        from animation_nodes.id_keys import doesIDKeyExist
        return [doesIDKeyExist(object, self.keyDataType, self.keyName) for object in objects]

    def delete(self):
        self.keyName = ""
        bpy.ops.an.update_id_keys_list()

@findsIDKeys(removable = False)
def getIDKeysOfNodes():
    idKeys = set()
    for node in getNodesByType("an_ObjectIDKeyNode"):
        if node.keyName != "":
            idKeys.add(IDKey(node.keyDataType, node.keyName))
    return idKeys
