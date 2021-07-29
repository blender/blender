import bpy
from bpy.props import *
from ... utils.layout import writeText
from ... base_types import AnimationNode
from ... data_structures import AverageSound, SpectrumSound

soundTypeItems = [
    ("AVERAGE", "Average", "", "FORCE_TURBULENCE", 0),
    ("SPECTRUM", "Spectrum", "", "RNDCURVE", 1)
]

class SoundFromSequencesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SoundFromSequencesNode"
    bl_label = "Sound from Sequences"
    bl_width_default = 160

    soundType = EnumProperty(name = "Sound Type", items = soundTypeItems)
    errorMessage = StringProperty()

    def create(self):
        self.newInput("Sequence List", "Sequences", "sequences")
        self.newInput("Integer", "Bake Index", "bakeIndex")
        self.newOutput("Sound", "Sound", "sound")

    def draw(self, layout):
        layout.prop(self, "soundType", text = "")
        if self.errorMessage != "":
            writeText(layout, self.errorMessage, width = 25, icon = "ERROR")

    def execute(self, sequences, bakeIndex):
        self.errorMessage = ""
        if len(sequences) == 0: return None
        sequences = list(filter(lambda x: getattr(x, "type", None) == "SOUND", sequences))
        try:
            if self.soundType == "AVERAGE": return AverageSound.fromSequences(sequences, bakeIndex)
            if self.soundType == "SPECTRUM": return SpectrumSound.fromSequences(sequences, bakeIndex)
        except IndexError:
            self.errorMessage = "At least one sequence does not have this bake index"
            return None
