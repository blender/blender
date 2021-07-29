import bpy
from bpy.props import *
from .... events import executionCodeChanged
from .... base_types import AnimationNode

frameTypeItems = [
    ("OFFSET", "Offset", ""),
    ("ABSOLUTE", "Absolute", "") ]

class CopyTransformsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CopyTransformsNode"
    bl_label = "Copy Transforms"
    bl_width_default = 170

    def useCurrentTransformsChanged(self, context):
        self.inputs["Frame"].hide = self.useCurrentTransforms
        executionCodeChanged()

    useCurrentTransforms = BoolProperty(
        name = "Use Current Transforms", default = True,
        update = useCurrentTransformsChanged)

    frameType = EnumProperty(
        name = "Frame Type", default = "OFFSET",
        items = frameTypeItems, update = executionCodeChanged)

    def create(self):
        self.newInput("Object", "From", "fromObject")
        self.newInput("Object", "To", "toObject")
        self.newInput("Float", "Frame", "frame").hide = True
        self.newOutput("Object", "To", "toObject")

    def draw(self, layout):
        if not self.useCurrentTransforms:
            layout.prop(self, "frameType")

    def drawAdvanced(self, layout):
        layout.prop(self, "useCurrentTransforms")
        col = layout.column()
        col.active = not self.useCurrentTransforms
        col.prop(self, "frameType")

    def getExecutionCode(self):
        yield "if fromObject and toObject:"
        if self.useCurrentTransforms:
            yield "    toObject.location = fromObject.location"
            yield "    toObject.rotation_euler = fromObject.rotation_euler"
            yield "    toObject.scale = fromObject.scale"
        else:
            if self.frameType == "OFFSET": yield "    evaluationFrame = frame + self.nodeTree.scene.frame_current_final"
            else: yield "    evaluationFrame = frame"
            yield "    toObject.location = animation_nodes.utils.fcurve.getArrayValueAtFrame(fromObject, 'location', evaluationFrame)"
            yield "    toObject.rotation_euler = animation_nodes.utils.fcurve.getArrayValueAtFrame(fromObject, 'rotation_euler', evaluationFrame)"
            yield "    toObject.scale = animation_nodes.utils.fcurve.getArrayValueAtFrame(fromObject, 'scale', evaluationFrame)"
