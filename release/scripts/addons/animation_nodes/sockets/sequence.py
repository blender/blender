import bpy
from bpy.props import *
from bpy.types import Sequence
from .. events import propertyChanged
from .. base_types import AnimationNodeSocket, PythonListSocket

class SequenceSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_SequenceSocket"
    bl_label = "Sequence Socket"
    dataType = "Sequence"
    allowedInputTypes = ["Sequence"]
    drawColor = (0, 0.644, 0, 1)
    storable = False
    comparable = True

    sequenceName = StringProperty(update = propertyChanged)

    def drawProperty(self, layout, text, node):
        row = layout.row(align = True)

        editor = self.nodeTree.scene.sequence_editor
        if editor:
            row.prop_search(self, "sequenceName",  editor, "sequences", icon = "NLA", text = text)
            self.invokeFunction(row, node, "assignActiveSequence", icon = "EYEDROPPER")
        else:
            row.label("No Sequence Editor")


    def getValue(self):
        editor = self.nodeTree.scene.sequence_editor
        if editor: return editor.sequences.get(self.sequenceName)
        return None

    def setProperty(self, data):
        self.sequenceName = data

    def getProperty(self):
        return self.sequenceName

    def assignActiveSequence(self):
        sequenceEditor = self.nodeTree.scene.sequence_editor
        if not sequenceEditor: return

        sequence = sequenceEditor.active_strip
        if sequence:
            self.sequenceName = sequence.name

    @classmethod
    def getDefaultValue(cls):
        return None

    @classmethod
    def getDefaultValueCode(cls):
        return "None"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, Sequence) or value is None:
            return value, 0
        return cls.getDefaultValue(), 2


class SequenceListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_SequenceListSocket"
    bl_label = "Sequence List Socket"
    dataType = "Sequence List"
    baseDataType = "Sequence"
    allowedInputTypes = ["Sequence List"]
    drawColor = (0, 0.644, 0, 0.5)
    storable = False
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, Sequence) or element is None for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
