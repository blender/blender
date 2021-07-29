import bpy
from bpy.props import *
from ... data_structures import DoubleList
from ... base_types import AnimationNode

modeItems = [
    ("AVERAGE", "Average", "", "FORCE_TURBULENCE", 0),
    ("SPECTRUM", "Spectrum", "", "RNDCURVE", 1)
]

class EvaluateSoundNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_EvaluateSoundNode"
    bl_label = "Evaluate Sound"

    mode = EnumProperty(name = "Mode", default = "AVERAGE",
        items = modeItems, update = AnimationNode.refresh)

    useCurrentFrame = BoolProperty(name = "Use Current Frame", default = True,
        update = AnimationNode.refresh)

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Sound", "Sound", "sound",
            typeFilter = self.mode, defaultDrawType = "PROPERTY_ONLY")

        if not self.useCurrentFrame:
            self.newInput("Float", "Frame", "frame")

        if self.mode == "AVERAGE":
            self.newOutput("Float", "Volume", "volume")
        elif self.mode == "SPECTRUM":
            self.newOutput("Float List", "Volumes", "volumes")

    def draw(self, layout):
        layout.prop(self, "mode", text = "")

        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def drawAdvanced(self, layout):
        layout.prop(self, "useCurrentFrame")

    def getExecutionCode(self):
        yield "self.errorMessage = ''"
        if self.useCurrentFrame: yield "_frame = self.nodeTree.scene.frame_current_final"
        else:                    yield "_frame = frame"

        if self.mode == "AVERAGE":
            yield "volume = self.execute_Average(sound, _frame)"
        elif self.mode == "SPECTRUM":
            yield "volumes = self.execute_Spectrum(sound, _frame)"

    def execute_Average(self, sound, frame):
        if sound is None: return 0
        if sound.type != "AVERAGE":
            self.errorMessage = "Wrong sound type"
            return 0
        return sound.evaluate(frame)

    def execute_Spectrum(self, sound, frame):
        if sound is None: return DoubleList()
        if sound.type != "SPECTRUM":
            self.errorMessage = "Wrong sound type"
            return DoubleList()
        return DoubleList.fromValues(sound.evaluate(frame))
