import bpy
from ... base_types import AnimationNode
from ... data_structures import FloatList, DoubleList

class FCurveKeyframesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FCurveKeyframesNode"
    bl_label = "FCurve Keyframes"

    def create(self):
        self.newInput("FCurve", "FCurve", "fCurve")
        self.newOutput("Float List", "Keyframes Frames", "keyframesFrames")
        self.newOutput("Float List", "Keyframes Values", "keyframesValues")

    def execute(self, fCurve):
        if fCurve is None:
            return DoubleList(), DoubleList()

        allValues = FloatList(len(fCurve.keyframe_points) * 2)
        fCurve.keyframe_points.foreach_get("co", allValues.asMemoryView())
        return DoubleList.fromValues(allValues[0::2]), DoubleList.fromValues(allValues[1::2])
