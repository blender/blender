import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... events import executionCodeChanged
from .. vector.vector_math import operationByName
subtractVectorLists = operationByName["Subtract"].execute_vA_vB
from .. vector.c_utils import calculateVectorDistances, calculateVectorCenters

stateItems = [
    ("REST", "Rest", "Return information in rest position.", "NONE", 0),
    ("POSE", "Pose", "Return information in pose position.", "NONE", 1) ]

class ArmatureInfoNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ArmatureInfoNode"
    bl_label = "Armature Info"

    state = EnumProperty(name = "State", default = "POSE",
        items = stateItems, update = executionCodeChanged)

    def create(self):
        self.newInput("Object", "Armature", "armature", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Boolean", "Use World Space", "useWorldSpace", value = True)

        self.newOutput("Matrix List", "Matrices", "matrices")
        self.newOutput("Vector List", "Centers", "centers")
        self.newOutput("Vector List", "Directions", "directions")
        self.newOutput("Float List", "Lengths", "lengths")
        self.newOutput("Vector List", "Heads", "heads", hide = True)
        self.newOutput("Vector List", "Tails", "tails", hide = True)
        self.newOutput("Text List", "Names", "names", hide = True)

    def draw(self, layout):
        layout.prop(self, "state")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return

        loadMatrices = isLinked["matrices"]
        headsAndTailsRequired = isLinked["directions"] or isLinked["lengths"] or isLinked["centers"]
        loadHeads = isLinked["heads"] or headsAndTailsRequired
        loadTails = isLinked["tails"] or headsAndTailsRequired

        yield "if getattr(armature, 'type', '') == 'ARMATURE':"
        if self.state == "REST":
            yield "    bones = armature.data.bones"
            matrixName = "matrix_local"
        else:
            yield "    bones = armature.pose.bones"
            matrixName = "matrix"

        if loadMatrices:
            yield "    matrices = Matrix4x4List(length = len(bones))"
            yield "    bones.foreach_get('{}', matrices.asMemoryView())".format(matrixName)
            yield "    matrices.transpose()"
        if loadHeads:
            yield "    heads = Vector3DList(length = len(bones))"
            yield "    bones.foreach_get('head', heads.asMemoryView())"
        if loadTails:
            yield "    tails = Vector3DList(length = len(bones))"
            yield "    bones.foreach_get('tail', tails.asMemoryView())"

        yield "    if useWorldSpace:"
        yield "        worldMatrix = armature.matrix_world"
        if loadMatrices: yield "        matrices.transform(worldMatrix)"
        if loadHeads:    yield "        heads.transform(worldMatrix)"
        if loadTails:    yield "        tails.transform(worldMatrix)"

        if isLinked["directions"]:
            yield "    directions = self.calcDirections(heads, tails)"
        if isLinked["lengths"]:
            yield "    lengths = self.calcLengths(heads, tails)"
        if isLinked["centers"]:
            yield "    centers = self.calcCenters(heads, tails)"

        if isLinked["names"]:
            yield "    names = [bone.name for bone in bones]"

        yield "else:"
        yield "    matrices = Matrix4x4List()"
        yield "    centers = Vector3DList()"
        yield "    directions = Vector3DList()"
        yield "    lengths = DoubleList()"
        yield "    heads = Vector3DList()"
        yield "    tails = Vector3DList()"
        yield "    names = []"

    def calcDirections(self, heads, tails):
        return subtractVectorLists(tails, heads)

    def calcLengths(self, heads, tails):
        return calculateVectorDistances(heads, tails)

    def calcCenters(self, heads, tails):
        return calculateVectorCenters(heads, tails)
