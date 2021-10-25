import bpy
from ... base_types import AnimationNode

class FCurveInfoNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FCurveInfoNode"
    bl_label = "FCurve Info"

    def create(self):
        self.newInput("FCurve", "FCurve", "fCurve")
        self.newOutput("Text", "Data Path", "dataPath")
        self.newOutput("Integer", "Array Index", "arrayIndex")

    def getExecutionCode(self, required):
        if len(required) == 0:
            return

        yield "if fCurve is not None:"
        if "dataPath" in required:   yield "    dataPath = fCurve.data_path"
        if "arrayIndex" in required: yield "    arrayIndex = fCurve.array_index"
        yield "else: dataPath, arrayIndex = '', 0"
