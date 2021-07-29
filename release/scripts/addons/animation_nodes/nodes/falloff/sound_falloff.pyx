import bpy
from bpy.props import *
from ... base_types import AnimationNode
from . constant_falloff import ConstantFalloff
from . interpolate_list_falloff import createIndexBasedFalloff, createFalloffBasedFalloff
from ... data_structures cimport (AverageSound, BaseFalloff, CompoundFalloff,
                                  DoubleList, Interpolation)

soundTypeItems = [
    ("AVERAGE", "Average", "", "FORCE_TURBULENCE", 0),
    ("SPECTRUM", "Spectrum", "", "RNDCURVE", 1)
]

averageFalloffTypeItems = [
    ("INDEX_OFFSET", "Index Offset", "", "NONE", 0)
]

spectrumFalloffTypeItems = [
    ("INDEX_FREQUENCY", "Index Frequency", "NONE", 0),
    ("FALLOFF_FREQUENCY", "Falloff Frequency", "NONE", 1)
]

indexFrequencyExtensionTypeItems = [
    ("LOOP", "Loop", "", "NONE", 0),
    ("MIRROR", "Mirror", "", "NONE", 1),
    ("EXTEND", "Extend Last", "NONE", 2)
]

class SoundFalloffNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SoundFalloffNode"
    bl_label = "Sound Falloff"

    soundType = EnumProperty(name = "Sound Type", default = "AVERAGE",
        items = soundTypeItems, update = AnimationNode.refresh)

    averageFalloffType = EnumProperty(name = "Average Falloff Type", default = "INDEX_OFFSET",
        items = averageFalloffTypeItems, update = AnimationNode.refresh)

    spectrumFalloffType = EnumProperty(name = "Spectrum Falloff Type", default = "INDEX_FREQUENCY",
        items = spectrumFalloffTypeItems, update = AnimationNode.refresh)

    indexFrequencyExtensionType = EnumProperty(name = "Index Frequency Extension Type", default = "LOOP",
        items = indexFrequencyExtensionTypeItems, update = AnimationNode.refresh)

    fadeLowFrequenciesToZero = BoolProperty(name = "Fade Low Frequencies to Zero", default = False)
    fadeHighFrequenciesToZero = BoolProperty(name = "Fade High Frequencies to Zero", default = False)

    useCurrentFrame = BoolProperty(name = "Use Current Frame", default = True,
        update = AnimationNode.refresh)

    def create(self):
        self.newInput("Sound", "Sound", "sound",
            typeFilter = self.soundType, defaultDrawType = "PROPERTY_ONLY")
        if not self.useCurrentFrame:
            self.newInput("Float", "Frame", "frame")

        if self.soundType == "AVERAGE":
            if self.averageFalloffType == "INDEX_OFFSET":
                self.newInput("Integer", "Offset", "offset", value = 1, minValue = 0)
        elif self.soundType == "SPECTRUM":
            if self.spectrumFalloffType == "INDEX_FREQUENCY":
                self.newInput("Float", "Length", "length", value = 10, minValue = 1)
                self.newInput("Float", "Offset", "offset")
            elif self.spectrumFalloffType == "FALLOFF_FREQUENCY":
                self.newInput("Falloff", "Falloff", "falloff")
            self.newInput("Interpolation", "Interpolation", "interpolation",
                defaultDrawType = "PROPERTY_ONLY", category = "QUADRATIC",
                easeIn = True, easeOut = True)

        self.newOutput("Falloff", "Falloff", "outFalloff")

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "soundType", text = "")
        if self.soundType == "AVERAGE":
            col.prop(self, "averageFalloffType", text = "")
        else:
            col.prop(self, "spectrumFalloffType", text = "")
            if self.spectrumFalloffType == "INDEX_FREQUENCY":
                col.prop(self, "indexFrequencyExtensionType", text = "")

    def drawAdvanced(self, layout):
        col = layout.column(align = True)
        col.label("Fade to zero:")
        col.prop(self, "fadeLowFrequenciesToZero", text = "Low Frequencies")
        col.prop(self, "fadeHighFrequenciesToZero", text = "High Frequencies")

    def getExecutionCode(self):
        yield "if sound is not None and sound.type == self.soundType:"
        if self.useCurrentFrame: yield "    _frame = self.nodeTree.scene.frame_current_final"
        else:                    yield "    _frame = frame"

        if self.soundType == "AVERAGE":
            if self.averageFalloffType == "INDEX_OFFSET":
                yield "    outFalloff = self.execute_Average_IndexOffset(sound, _frame, offset)"
        elif self.soundType == "SPECTRUM":
            if self.spectrumFalloffType == "INDEX_FREQUENCY":
                yield "    outFalloff = self.execute_Spectrum_IndexFrequency(sound, _frame, length, offset, interpolation)"
            elif self.spectrumFalloffType == "FALLOFF_FREQUENCY":
                yield "    outFalloff = self.execute_Spectrum_FalloffFrequency(sound, _frame, falloff, interpolation)"

        yield "else: outFalloff = self.getConstantFalloff(0)"

    def getConstantFalloff(self, value = 0):
        return ConstantFalloff(value)

    def execute_Average_IndexOffset(self, sound, frame, offset):
        return Average_IndexOffset_SoundFalloff(sound, frame, offset)

    def execute_Spectrum_IndexFrequency(self, sound, frame, length, offset, interpolation):
        type = self.indexFrequencyExtensionType
        myList = self.getFrequenciesAtFrame(sound, frame)
        return createIndexBasedFalloff(type, myList, length, offset, interpolation)

    def execute_Spectrum_FalloffFrequency(self, sound, frame, falloff, interpolation):
        myList = self.getFrequenciesAtFrame(sound, frame)
        return createFalloffBasedFalloff(falloff, myList, interpolation)

    def getFrequenciesAtFrame(self, sound, frame):
        myList = DoubleList.fromValues(sound.evaluate(frame))
        if self.fadeLowFrequenciesToZero:
            myList = [0] + myList
        if self.fadeHighFrequenciesToZero:
            myList = myList + [0]
        return myList

cdef class Average_IndexOffset_SoundFalloff(BaseFalloff):
    cdef:
        AverageSound sound
        float frame, offsetInverse

    def __cinit__(self, AverageSound sound, float frame, offset):
        self.sound = sound
        self.frame = frame
        self.offsetInverse = 1 / offset if offset != 0 else 0
        self.dataType = "All"
        self.clamped = False

    cdef double evaluate(self, void *object, long index):
        return self.sound.evaluate(self.frame - index * self.offsetInverse)
