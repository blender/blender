import bpy
from ... base_types import AnimationNode

class SequencesFromChannelNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SequencesFromChannelNode"
    bl_label = "Sequences from Channel"

    def create(self):
        self.newInput("Integer", "Channel", "channel", value = 1).setRange(1, 32)
        self.newInput("Scene", "Scene", "scene", hide = True)
        self.newOutput("Sequence List", "Sequences", "sequences")

    def getExecutionCode(self):
        return ("editor = scene.sequence_editor if scene else None",
                "sequences = [sequence for sequence in getattr(editor, 'sequences', []) if sequence.channel == channel]")
