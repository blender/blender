import bpy
from bpy.props import *
from ... events import propertyChanged
from ... data_structures import DoubleList
from ... utils.data_blocks import removeNotUsedDataBlock
from ... base_types import AnimationNode, VectorizedSocket

modeItems = [
    ("ALL", "All", "Get weight of every vertex", "NONE", 0),
    ("INDEX", "Index", "Get weight of a specific vertex", "NONE", 1)
]

groupIdentifierTypeItems = [
    ("INDEX", "Index", "Get vertex group based on the index", "NONE", 0),
    ("NAME", "Name", "Get vertex group based on the name", "NONE", 1)
]

groupNotFoundMessage = "group not found"
noMeshMessage = "no mesh object"
noSceneMessage = "scene required"

class VertexGroupInputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_VertexGroupInputNode"
    bl_label = "Vertex Group Input"
    errorHandlingType = "EXCEPTION"

    mode = EnumProperty(name = "Mode", default = "ALL",
        items = modeItems, update = AnimationNode.refresh)

    groupIdentifierType = EnumProperty(name = "Group Identifier Type", default = "INDEX",
        items = groupIdentifierTypeItems, update = AnimationNode.refresh)

    useIndexList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")

        if self.groupIdentifierType == "INDEX":
            self.newInput("Integer", "Group Index", "groupIndex")
        elif self.groupIdentifierType == "NAME":
            self.newInput("Text", "Name", "groupName")

        if self.mode == "INDEX":
            self.newInput(VectorizedSocket("Integer", "useIndexList",
                ("Index", "index"), ("Indices", "indices")))
            self.newOutput(VectorizedSocket("Float", "useIndexList",
                ("Weight", "weight"), ("Weights", "weights")))
        elif self.mode == "ALL":
            self.newInput("Boolean", "Use Modifiers", "useModifiers", value = False)
            self.newInput("Scene", "Scene", "scene", hide = True)
            self.newOutput("Float List", "Weights", "weights")

    def draw(self, layout):
        layout.prop(self, "mode", text = "")

    def drawAdvanced(self, layout):
        layout.prop(self, "groupIdentifierType", text = "Type")

    def getExecutionFunctionName(self):
        if self.mode == "INDEX":
            if self.useIndexList:
                return "execute_Indices"
            else:
                return "execute_Index"
        elif self.mode == "ALL":
            return "execute_All"

    def execute_Index(self, object, identifier, index):
        if object is None:
            return 0

        vertexGroup = self.getVertexGroup(object, identifier)
        if vertexGroup is None:
            self.raiseErrorMessage(groupNotFoundMessage)

        try: return vertexGroup.weight(index)
        except: return 0

    def execute_Indices(self, object, identifier, indices):
        vertexGroup = self.getVertexGroup(object, identifier)
        if vertexGroup is None:
            if object is not None:
                self.raiseErrorMessage(groupNotFoundMessage)
            return DoubleList()

        weights = DoubleList(length = len(indices))
        getWeight = vertexGroup.weight

        for i, index in enumerate(indices):
            try: weights[i] = getWeight(index)
            except: weights[i] = 0
        return weights

    def execute_All(self, object, identifier, useModifiers, *optionalScene):
        if object is None:
            return DoubleList()

        if object.type != "MESH":
            self.raiseErrorMessage(noMeshMessage)

        vertexGroup = self.getVertexGroup(object, identifier)
        if vertexGroup is None:
            self.raiseErrorMessage(groupNotFoundMessage)

        if useModifiers:
            return self.execute_All_WithModifiers(object, vertexGroup, optionalScene[0])
        else:
            return self.execute_All_WithoutModifiers(object, vertexGroup)

    def execute_All_WithoutModifiers(self, object, vertexGroup):
        vertexAmount = len(object.data.vertices)
        weights = DoubleList(length = vertexAmount)
        getWeight = vertexGroup.weight

        for i in range(vertexAmount):
            try: weights[i] = getWeight(i)
            except: weights[i] = 0
        return weights

    def execute_All_WithModifiers(self, object, vertexGroup, scene):
        if scene is None:
            self.raiseErrorMessage(noSceneMessage)

        mesh = object.an.getMesh(scene, applyModifiers = True)
        index = vertexGroup.index
        weights = DoubleList(length = len(mesh.vertices))
        weights.fill(0)
        for i, vertex in enumerate(mesh.vertices):
            for vertexGroupElement in vertex.groups:
                if vertexGroupElement.group == index:
                    weights[i] = vertexGroupElement.weight
                    break

        if mesh.users == 0:
            removeNotUsedDataBlock(mesh, "MESH")

        return weights

    def getVertexGroup(self, object, identifier):
        try: return object.vertex_groups[identifier]
        except: return None
