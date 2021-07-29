import bpy
import os
from bpy.props import *
from ... data_structures import FloatList
from ... utils.names import getRandomString
from ... tree_info import getNodeByIdentifier
from ... base_types import AnimationNode
from ... utils.blender_ui import getDpiFactor
from ... utils.path import getAbsolutePathOfSound
from ... utils.fcurve import getSingleFCurveWithDataPath
from ... utils.sequence_editor import getOrCreateSequencer, getEmptyChannel

class SoundFrequencyRange(bpy.types.PropertyGroup):
    bl_idname = "an_SoundFrequencyRange"
    low = IntProperty(name = "Low", min = 0)
    high = IntProperty(name = "High", min = 0)


class SoundBakeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_BakeSoundNode"
    bl_label = "Bake Sound"
    bl_width_default = 300

    soundName = StringProperty(name = "Sound")

    activeBakeDataIndex = IntProperty()
    activeSpectrumDataIndex = IntProperty()

    low = IntProperty(name = "Low", default = 0, min = 0, max = 20000)
    high = IntProperty(name = "High", default = 10000, min = 0, max = 20000)
    attack = FloatProperty(name = "Attack", default = 0.005, precision = 3)
    release = FloatProperty(name = "Release", default = 0.2, precision = 3)

    showSpectrumFrequencyRanges = BoolProperty(default = False)
    spectrumFrequencyRanges = CollectionProperty(type = SoundFrequencyRange)

    bakeProgress = StringProperty()

    def setup(self):
        self.setSpectrumFrequencyRanges(frequencyRanges)

    def draw(self, layout):
        layout.separator()
        col = layout.column()
        col.scale_y = 1.4
        self.invokeSelector(col, "PATH", "loadSound",
            text = "Load New Sound", icon = "NEW")
        layout.separator()

        if len(bpy.data.sounds) == 0: return

        col = layout.column(align = True)
        row = col.row(align = True)
        row.prop_search(self, "soundName",  bpy.data, "sounds", text = "")
        self.invokeFunction(row, "removeSequencesWithActiveSound", icon = "X",
            description = "Remove sound sequences using this sound")

        if self.sound is None:
            if len(bpy.data.sounds) > 0:
                layout.label("Select a sound for more settings", icon = "INFO")
            return

        box = col.box()
        self.drawForSound(box, self.sound)

    def drawForSound(self, layout, sound):
        if sound.users == 0:
            col = layout.column()
            col.scale_y = 1.5
            self.invokeFunction(col, "removeActiveSound", text = "This sound is not used. Remove it?", icon = "CANCEL")
        row = layout.row()
        col = row.column(align = True)
        col.prop(self, "low")
        col.prop(self, "high")
        col = row.column(align = True)
        col.prop(self, "attack")
        col.prop(self, "release")

        row = layout.row(align = True)
        row.scale_y = 1.3
        self.invokeFunction(row, "bakeAverageSound", text = "Bake Average", icon = "FORCE_TURBULENCE")
        self.invokeFunction(row, "bakeSpectrumData", text = "Bake Spectrum", icon = "RNDCURVE")
        self.drawSpectrumFrequencyRanges(layout)

        if self.bakeProgress != "":
            layout.label(self.bakeProgress, icon = "INFO")

        self.drawBakedData_Average(layout, sound)
        self.drawBakedData_Spectrum(layout, sound)

    def drawSpectrumFrequencyRanges(self, layout):
        col = layout.column()

        row = col.row(align = True)
        icon = "TRIA_DOWN" if self.showSpectrumFrequencyRanges else "TRIA_RIGHT"
        row.prop(self, "showSpectrumFrequencyRanges", icon = icon, text = "", icon_only = True)
        self.invokeFunction(row, "openFrequencyCalculator", text = "Calculate Spectrum Frequencies ({})".format(len(self.spectrumFrequencyRanges)))

        if self.showSpectrumFrequencyRanges:
            subcol = col.column(align = True)
            for i, item in enumerate(self.spectrumFrequencyRanges):
                row = subcol.row(align = True)
                row.alignment = "RIGHT"
                row.label(str(i) + ":")
                right = row.split(percentage = 0.5, align = True)
                right.prop(item, "low")
                right.prop(item, "high")

    def drawBakedData_Average(self, layout, sound):
        items = sound.bakedData.average
        if len(items) > 0:
            col = layout.column()
            row = col.row(align = True)
            row.label("Average Data:")
            self.invokeFunction(row, "moveItemUp", icon = "TRIA_UP")
            self.invokeFunction(row, "moveItemDown", icon = "TRIA_DOWN")
            col.template_list("an_AverageItemsUiList", "", sound.bakedData, "average",
                self, "activeBakeDataIndex", rows = len(items) + 1)

    def drawBakedData_Spectrum(self, layout, sound):
        items = sound.bakedData.spectrum
        if len(items) > 0:
            col = layout.column()
            row = col.row()
            row.label("Spectrum Data:")
            col.template_list("an_SpectrumItemsUiList", "", sound.bakedData, "spectrum",
                self, "activeSpectrumDataIndex", rows = len(items) + 1)

    def loadSound(self, path):
        editor = getOrCreateSequencer(self.nodeTree.scene)
        channel = getEmptyChannel(editor)
        sequence = editor.sequences.new_sound(
            name = os.path.basename(path),
            filepath = path,
            channel = channel,
            frame_start = bpy.context.scene.frame_start)
        self.soundName = sequence.sound.name

    def setSpectrumFrequencyRanges(self, ranges):
        self.spectrumFrequencyRanges.clear()
        for low, high in ranges:
            item = self.spectrumFrequencyRanges.add()
            item.low = low
            item.high = high

    def openFrequencyCalculator(self):
        bpy.ops.an.calculate_spectrum_frequency_ranges("INVOKE_DEFAULT",
            nodeIdentifier = self.identifier,
            frequencyRangeStart = self.low,
            frequencyRangeEnd = self.high)

    def bakeAverageSound(self):
        sound, low, high, attack, release = self.sound, self.low, self.high, self.attack, self.release
        if getAverageDataItem(sound, low, high, attack, release): return

        soundData = bake(sound, low, high, attack, release)
        bakeDataItem = createAverageDataItem(sound, low, high, attack, release)
        bakeDataItem.setSamples(soundData)

    def bakeSpectrumData(self):
        bpy.ops.an.bake_sound_spectrum_data("INVOKE_DEFAULT",
            nodeIdentifier = self.identifier,
            soundName = self.soundName,
            attack = self.attack,
            release = self.release,
            frequencyRanges = [{"low" : item.low, "high" : item.high, "name" : ""} for item in self.spectrumFrequencyRanges])

    def removeAverageBakedData(self, index):
        self.sound.bakedData.average.remove(int(index))

    def removeSpectrumBakedData(self, index):
        self.sound.bakedData.spectrum.remove(int(index))

    def moveItemUp(self):
        fromIndex = self.activeBakeDataIndex
        toIndex = fromIndex - 1
        self.moveItem(fromIndex, toIndex)

    def moveItemDown(self):
        fromIndex = self.activeBakeDataIndex
        toIndex = fromIndex + 1
        self.moveItem(fromIndex, toIndex)

    def moveItem(self, fromIndex, toIndex):
        self.sound.bakedData.average.move(fromIndex, toIndex)
        self.activeBakeDataIndex = min(max(toIndex, 0), len(self.sound.bakedData.average) - 1)

    def removeSequencesWithActiveSound(self):
        sound = self.sound
        editor = getOrCreateSequencer(self.nodeTree.scene)
        if not editor: return
        sequences = [sequence for sequence in editor.sequences if getattr(sequence, "sound", -1) == sound]
        for sequence in sequences:
            editor.sequences.remove(sequence)
        self.removeActiveSound()

    def removeActiveSound(self):
        sound = self.sound
        if sound is not None and sound.users == 0:
            bpy.data.sounds.remove(sound)


    @property
    def sound(self):
        return bpy.data.sounds.get(self.soundName)


class AverageItemsUiList(bpy.types.UIList):
    bl_idname = "an_AverageItemsUiList"

    def draw_item(self, context, layout, bakedData, item, icon, node, activePropname):
        layout.label("#" + str(list(bakedData.average).index(item)))
        layout.label(str(round(item.low)))
        layout.label(str(round(item.high)))
        layout.label(str(round(item.attack, 3)))
        layout.label(str(round(item.release, 3)))
        node.invokeFunction(layout, "removeAverageBakedData", icon = "X", emboss = False,
            data = list(bakedData.average).index(item))

class SpectrumItemsUiList(bpy.types.UIList):
    bl_idname = "an_SpectrumItemsUiList"

    def draw_item(self, context, layout, bakedData, item, icon, node, activePropname):
        layout.label("#" + str(list(bakedData.spectrum).index(item)))
        layout.label(str(round(item.attack, 3)))
        layout.label(str(round(item.release, 3)))
        node.invokeFunction(layout, "removeSpectrumBakedData", icon = "X", emboss = False,
            data = list(bakedData.spectrum).index(item))



# Bake Spectrum Data
################################

frequencySteps = [0, 20, 40, 80, 250, 600, 2000, 4000, 6000, 8000, 20000]
frequencyRanges = list(zip(frequencySteps[:-1], frequencySteps[1:]))

class BakeSpectrumData(bpy.types.Operator):
    bl_idname = "an.bake_sound_spectrum_data"
    bl_label = "Bake Spectrum Data"

    nodeIdentifier = StringProperty()

    soundName = StringProperty()
    attack = FloatProperty()
    release = FloatProperty()

    frequencyRanges = CollectionProperty(type = SoundFrequencyRange)

    def invoke(self, context, event):
        if len(self.frequencyRanges) == 0:
            self.report({"INFO"}, "There has to be at least one frequency range")
            return {"FINISHED"}

        try: self.node = getNodeByIdentifier(self.nodeIdentifier)
        except: self.node = None
        self.sound = bpy.data.sounds[self.soundName]
        self.currentIndex = 0
        self.spectrumData = []
        self.counter = 0
        context.window_manager.modal_handler_add(self)
        self.timer = context.window_manager.event_timer_add(0.001, context.window)
        self.setNodeMessage("Baking Started")
        return {"RUNNING_MODAL"}

    def finish(self):
        bpy.context.window_manager.event_timer_remove(self.timer)
        self.setNodeMessage("")
        return {"FINISHED"}

    def modal(self, context, event):
        if "ESC" == event.type: return self.finish()
        self.counter += 1
        if self.counter % 20 != 0: return {"RUNNING_MODAL"}

        if self.currentIndex < len(self.frequencyRanges):
            item = self.frequencyRanges[self.currentIndex]
            data = bake(self.sound, item.low, item.high, self.attack, self.release)
            self.spectrumData.append(data)
            self.currentIndex += 1
            self.setNodeMessage("Frequency range {}-{} baked".format(item.low, item.high))
        else:
            self.storeBakedData()
            return self.finish()

        context.area.tag_redraw()
        return {"RUNNING_MODAL"}

    def storeBakedData(self):
        item = self.getNewSpectrumItem()
        frequencyAmount = len(self.frequencyRanges)

        data = self.spectrumData
        samples = FloatList(length = len(data[0]) * frequencyAmount)
        for i, samplesForFrequency in enumerate(data):
            samples[i::frequencyAmount] = samplesForFrequency
        item.setSamples(samples, frequencyAmount)

    def getNewSpectrumItem(self):
        spectrumItem = self.sound.bakedData.spectrum.add()
        spectrumItem.attack = self.attack
        spectrumItem.release = self.release
        spectrumItem.identifier = getRandomString(10)
        return spectrumItem

    def setNodeMessage(self, message):
        if self.node: self.node.bakeProgress = message


# Sound Baking
################################

def bake(sound, low = 0.0, high = 100000, attack = 0.005, release = 0.2):
    '''Returns a float list containing the sampled data'''
    object = createObjectWithFCurveAsTarget()
    oldFrame = setCurrentFrame(0)
    oldArea = switchArea("GRAPH_EDITOR")
    usedUnpacking, filepath = getRealFilePath(sound)

    bpy.ops.graph.sound_bake(
        filepath = filepath,
        low = low,
        high = high,
        attack = attack,
        release = release)

    if usedUnpacking: os.remove(filepath)
    soundData = getSamplesFromFCurve(object)
    removeObject(object)
    setCurrentFrame(oldFrame)
    switchArea(oldArea)
    return soundData

def createObjectWithFCurveAsTarget():
    bpy.ops.object.add()
    object = bpy.context.active_object
    object.keyframe_insert(frame = 0, data_path = "location", index = 0)
    return object

def removeObject(object):
    bpy.context.scene.objects.unlink(object)
    bpy.data.objects.remove(object)

def setCurrentFrame(frame):
    oldFrame = bpy.context.scene.frame_current
    bpy.context.scene.frame_current = frame
    return oldFrame

def switchArea(targetType):
    area = bpy.context.area
    oldType = area.type
    area.type = targetType
    return oldType

def getRealFilePath(sound):
    filepath = getAbsolutePathOfSound(sound)
    if os.path.exists(filepath): return False, filepath
    if not sound.packed_file: raise Exception("Sound file not found")
    path = os.path.join(os.path.dirname(__file__), "TEMPORARY SOUND FILE")
    file = open(path, "w+b")
    file.write(sound.packed_file.data)
    file.close()
    return True, path

def createAverageDataItem(sound, low, high, attack, release):
    item = sound.bakedData.average.add()
    item.low = low
    item.high = high
    item.attack = attack
    item.release = release
    item.identifier = getRandomString(10)
    return item

def getSamplesFromFCurve(object):
    fcurve = getSingleFCurveWithDataPath(object, "location")
    samples = FloatList(length = len(fcurve.sampled_points) * 2)
    fcurve.sampled_points.foreach_get("co", samples.asMemoryView())
    return samples[1::2]



# Bake Spectrum Data
################################

class CalculateSpectrumFrequencyRanges(bpy.types.Operator):
    bl_idname = "an.calculate_spectrum_frequency_ranges"
    bl_label = "Calculate Frequency Ranges"
    bl_options = {"INTERNAL", "REGISTER"}

    nodeIdentifier = StringProperty()
    amount = IntProperty(name = "Amount", default = 10, min = 1, soft_max = 50)
    frequencyRangeStart = IntProperty(name = "Frequency Range Start", default = 0, min = 0)
    frequencyRangeEnd = IntProperty(name = "Frequency Range End", default = 20000, min = 0)

    base = FloatProperty(name = "Base", min = 0.001, default = 0.22)
    exponent = IntProperty(name = "Exponent", min = 1, max = 20, default = 4)

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width = 200 * getDpiFactor())

    def check(self, context):
        return True

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "amount")

        row = layout.row(align = True)
        row.prop(self, "frequencyRangeStart", text = "Start")
        row.prop(self, "frequencyRangeEnd", text = "End")

        row = layout.row(align = True)
        row.prop(self, "base", text = "Base")
        row.prop(self, "exponent", text = "Exponent")

        col = layout.column(align = True)
        for low, high in self.calculate_ranges():
            row = col.row(align = True)
            row.label(str(int(low)))
            row.label(str(int(high)))

    def execute(self, context):
        node = getNodeByIdentifier(self.nodeIdentifier)
        node.setSpectrumFrequencyRanges(self.calculate_ranges())
        return {"FINISHED"}

    def calculate_ranges(self):
        steps = self.calculate_steps()
        return list(zip(steps[:-1], steps[1:]))

    def calculate_steps(self):
        from ... algorithms.interpolations import ExponentialOut
        algorithm = ExponentialOut(self.base, self.exponent)
        return algorithm.sample(self.amount + 1, self.frequencyRangeStart, self.frequencyRangeEnd)



# Utils
################################

def getAverageDataItem(sound, low, high, attack, release):
    for item in sound.bakedData.average:
        if all([item.low == low,
                item.high == high,
                item.attack == attack,
                item.release == release]): return item



# Register
################################

class AverageData(bpy.types.PropertyGroup):
    bl_idname = "an_SoundAverageData"
    low = IntProperty(name = "Low")
    high = IntProperty(name = "High")
    attack = FloatProperty(name = "Attack", precision = 3)
    release = FloatProperty(name = "Release", precision = 3)
    identifier = StringProperty(name = "Identifier", default = "")

    def setSamples(self, samples):
        self["samples"] = samples

    def getSamples(self):
        return FloatList.fromValues(self["samples"])

class SpectrumData(bpy.types.PropertyGroup):
    bl_idname = "an_SoundSpectrumData"
    attack = FloatProperty(name = "Attack", precision = 3)
    release = FloatProperty(name = "Release", precision = 3)
    frequencyAmount = IntProperty(name = "Frequency Amount")
    identifier = StringProperty(name = "Identifier", default = "")

    def setSamples(self, samples, frequencyAmount):
        if len(samples) % frequencyAmount != 0:
            raise Exception("amount of samples has to be a multiple of frequencyAmount")
        self["samples"] = samples
        self.frequencyAmount = frequencyAmount

    def getSamples(self):
        return FloatList.fromValues(self["samples"])

class BakedData(bpy.types.PropertyGroup):
    bl_idname = "an_SoundBakedData"

    average = CollectionProperty(name = "Average Data", type = AverageData)
    spectrum = CollectionProperty(name = "Spectrum Data", type = SpectrumData)

def register():
    bpy.types.Sound.bakedData = PointerProperty(name = "Baked Data", type = BakedData)

def unregister():
    del bpy.types.Sound.bakedData
