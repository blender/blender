import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from ... events import executionCodeChanged
from ... utils.fcurve import getArrayValueAtFrame

frameTypeItems = [
    ("OFFSET", "Offset", "", "NONE", 0),
    ("ABSOLUTE", "Absolute", "", "NONE", 1)]

class ObjectTransformsInputNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ObjectTransformsInputNode"
    bl_label = "Object Transforms Input"
    bl_width_default = 165
    autoVectorizeExecution = True

    useCurrentTransforms = BoolProperty(
        name = "Use Current Transforms", default = True,
        update = VectorizedNode.refresh)

    frameType = EnumProperty(
        name = "Frame Type", default = "OFFSET",
        items = frameTypeItems, update = executionCodeChanged)

    useObjectList = VectorizedNode.newVectorizeProperty()
    useFrameList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"))

        if self.useCurrentTransforms:
            props = "useObjectList"
        else:
            self.newVectorizedInput("Float", "useFrameList",
                ("Frame", "frame"), ("Frames", "frames"))
            props = [("useObjectList", "useFrameList")]

        self.newVectorizedOutput("Vector", props,
            ("Location", "location"), ("Locations", "locations"))

        self.newVectorizedOutput("Euler", props,
            ("Rotation", "rotation"), ("Rotations", "rotations"))

        self.newVectorizedOutput("Vector", props,
            ("Scale", "scale"), ("Scales", "scales"))

        self.newVectorizedOutput("Quaternion", props,
            ("Quaternion", "quaternion", dict(hide = True)),
            ("Quaternions", "quaternions"))

    def draw(self, layout):
        if not self.useCurrentTransforms:
            layout.prop(self, "frameType")

    def drawAdvanced(self, layout):
        layout.prop(self, "useCurrentTransforms")
        col = layout.column()
        col.active = not self.useCurrentTransforms
        col.prop(self, "frameType")
        self.invokeFunction(layout, "createAutoExecutionTrigger", text = "Create Execution Trigger")

    def getExecutionCode(self):
        isLinked = self.getLinkedBaseOutputsDict()
        if not any(isLinked.values()): return

        yield "try:"
        if self.useCurrentTransforms:
            if isLinked["location"]: yield "    location = object.location"
            if isLinked["rotation"]: yield "    rotation = object.rotation_euler"
            if isLinked["scale"]:    yield "    scale = object.scale"
            if isLinked["quaternion"]: yield "    quaternion = object.rotation_quaternion"
        else:
            if self.frameType == "OFFSET": yield "    evaluationFrame = frame + self.nodeTree.scene.frame_current_final"
            else: yield "    evaluationFrame = frame"
            if isLinked["location"]: yield "    location = Vector(animation_nodes.utils.fcurve.getArrayValueAtFrame(object, 'location', evaluationFrame))"
            if isLinked["rotation"]: yield "    rotation = Euler(animation_nodes.utils.fcurve.getArrayValueAtFrame(object, 'rotation_euler', evaluationFrame))"
            if isLinked["scale"]:    yield "    scale = Vector(animation_nodes.utils.fcurve.getArrayValueAtFrame(object, 'scale', evaluationFrame))"
            if isLinked["quaternion"]:    yield "    quaternion = Quaternion(animation_nodes.utils.fcurve.getArrayValueAtFrame(object, 'rotation_quaternion', evaluationFrame, arraySize = 4))"

        yield "except:"
        yield "    location = Vector((0, 0, 0))"
        yield "    rotation = Euler((0, 0, 0))"
        yield "    scale = Vector((0, 0, 0))"
        yield "    quaternion = Quaternion((1, 0, 0, 0))"

    def createAutoExecutionTrigger(self):
        isLinked = self.getLinkedOutputsDict()
        customTriggers = self.nodeTree.autoExecution.customTriggers

        objectName = self.inputs["Object"].objectName

        if isLinked["location"]:
            customTriggers.new("MONITOR_PROPERTY", idType = "OBJECT", dataPath = "location", idObjectName = objectName)
        if isLinked["rotation"]:
            customTriggers.new("MONITOR_PROPERTY", idType = "OBJECT", dataPath = "rotation_euler", idObjectName = objectName)
        if isLinked["scale"]:
            customTriggers.new("MONITOR_PROPERTY", idType = "OBJECT", dataPath = "scale", idObjectName = objectName)
        if isLinked["quaternion"]:
            customTriggers.new("MONITOR_PROPERTY", idType = "OBJECT", dataPath = "rotation_quaternion", idObjectName = objectName)
