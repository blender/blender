import bpy
from bpy.props import *
from .. events import propertyChanged
from .. tree_info import nodeOfTypeExists
from .. base_types import AnimationNodeSocket
from .. algorithms.hashing import strToEnumItemID
from .. data_structures import AverageSound, SpectrumSound
from .. utils.nodes import newNodeAtCursor, invokeTranslation

typeFilterItems = [
    ("ALL", "All", "", "NONE", 0),
    ("AVERAGE", "Average", "", "NONE", 1),
    ("SPECTRUM", "Spectrum", "", "NONE", 2)]

def getBakeDataItems(self, context):
    items = []
    sequences = getattr(self.nodeTree.scene.sequence_editor, "sequences", [])
    for index, sequence in enumerate(sequences):
        if sequence.type != "SOUND": continue

        if self.typeFilter in {"ALL", "AVERAGE"}:
            items.extend(iterAverageItems(index, sequence))

        if self.typeFilter in {"ALL", "SPECTRUM"}:
            items.extend(iterSpectrumItems(index, sequence))

    items.append(("NONE", "None", "", "NONE", 0))
    return items

def iterAverageItems(sequenceIndex, sequence):
    for bakeIndex, data in enumerate(sequence.sound.bakedData.average):
        yield ("AVERAGE_{}_{}".format(sequenceIndex, bakeIndex),
               "#{} - {} - Average".format(bakeIndex, sequence.name),
               "Low: {}  High: {}  Attack: {:.3f}  Release: {:.3f}".format(
                   data.low, data.high, data.attack, data.release),
               strToEnumItemID(data.identifier))

def iterSpectrumItems(sequenceIndex, sequence):
    for bakeIndex, data in enumerate(sequence.sound.bakedData.spectrum):
        yield ("SPECTRUM_{}_{}".format(sequenceIndex, bakeIndex),
               "#{} - {} - Spectrum".format(bakeIndex, sequence.name),
               "Attack: {:.3f}  Release: {:.3f}".format(data.attack, data.release),
               strToEnumItemID(data.identifier))

class SoundSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_SoundSocket"
    bl_label = "Sound Socket"
    dataType = "Sound"
    allowedInputTypes = ["Sound"]
    drawColor = (0.9, 0.7, 0.4, 1)
    storable = False
    comparable = False

    bakeData = EnumProperty(name = "Bake Data", items = getBakeDataItems,
        update = propertyChanged)

    typeFilter = EnumProperty(items = typeFilterItems, default = "ALL")

    def drawProperty(self, layout, text, node):
        row = layout.row(align = True)
        row.prop(self, "bakeData", text = text)
        if self.bakeData == "NONE" and not nodeOfTypeExists("an_BakeSoundNode"):
            self.invokeFunction(row, node, "createSoundBakeNode", icon = "PLUS",
                description = "Create sound bake node")

    def getValue(self):
        if self.bakeData == "NONE":
            return None

        soundType, sequenceIndex, bakeIndex = self.bakeData.split("_")
        try: sequence = self.nodeTree.scene.sequence_editor.sequences[int(sequenceIndex)]
        except IndexError:
            return None

        if soundType == "AVERAGE": return AverageSound.fromSequences([sequence], int(bakeIndex))
        if soundType == "SPECTRUM": return SpectrumSound.fromSequences([sequence], int(bakeIndex))
        return None

    def setProperty(self, data):
        try: self.bakeData = data
        except: pass

    def getProperty(self):
        return self.bakeData

    def createSoundBakeNode(self):
        newNodeAtCursor("an_BakeSoundNode")
        invokeTranslation()

    @classmethod
    def getDefaultValue(cls):
        return None

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, (AverageSound, SpectrumSound)) or value is None:
            return value, 0
        return cls.getDefaultValue(), 2
