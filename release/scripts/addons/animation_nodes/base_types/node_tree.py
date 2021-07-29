import bpy
import time
from bpy.props import *
from .. utils.handlers import eventHandler
from .. utils.nodes import getAnimationNodeTrees
from . tree_auto_execution import AutoExecutionProperties
from .. events import treeChanged, isRendering, propertyChanged
from .. utils.blender_ui import iterActiveScreens, isViewportRendering
from .. preferences import getBlenderVersion, getAnimationNodesVersion
from .. tree_info import getNetworksByNodeTree, getSubprogramNetworksByNodeTree
from .. execution.units import getMainUnitsByNodeTree, setupExecutionUnits, finishExecutionUnits


class LastTreeExecutionInfo(bpy.types.PropertyGroup):
    bl_idname = "an_LastTreeExecutionInfo"

    isDefault = BoolProperty(default = True)
    executionTime = FloatProperty(name = "Execution Time")
    blenderVersion = IntVectorProperty(name = "Blender Version", default = (2, 77, 0))
    animationNodesVersion = IntVectorProperty(name = "Animation Nodes Version", default = (1, 0, 1))

    def updateVersions(self):
        self.blenderVersion = getBlenderVersion()
        self.animationNodesVersion = getAnimationNodesVersion()
        self.isDefault = False

    @property
    def blenderVersionString(self):
        return self.toVersionString(self.blenderVersion)

    @property
    def animationNodesVersionString(self):
        return self.toVersionString(self.animationNodesVersion)

    def toVersionString(self, intVector):
        numbers = tuple(intVector)
        return "{}.{}.{}".format(*numbers)

class AnimationNodeTree(bpy.types.NodeTree):
    bl_idname = "an_AnimationNodeTree"
    bl_label = "Animation"
    bl_icon = "ACTION"

    autoExecution = PointerProperty(type = AutoExecutionProperties)
    lastExecutionInfo = PointerProperty(type = LastTreeExecutionInfo)

    sceneName = StringProperty(name = "Scene",
        description = "The global scene used by this node tree (never none)")

    editNodeLabels = BoolProperty(name = "Edit Node Labels", default = False)

    def update(self):
        treeChanged()

    def canAutoExecute(self, events):
        def isAnimationPlaying():
            return any([screen.is_animation_playing for screen in iterActiveScreens()])

        a = self.autoExecution

        # always update the triggers for better visual feedback
        customTriggerHasBeenActivated = a.customTriggers.update()

        if not a.enabled: return False
        if not self.hasMainExecutionUnits: return False

        if isRendering():
            if events.intersection({"Scene", "Frame"}) and (a.sceneUpdate or a.frameChanged):
                return True
        else:
            if self.timeSinceLastAutoExecution < a.minTimeDifference: return False

            if isAnimationPlaying():
                if (a.sceneUpdate or a.frameChanged) and "Frame" in events: return True
            elif not isViewportRendering():
                if "Scene" in events and a.sceneUpdate: return True
            if "Frame" in events and a.frameChanged: return True
            if "Property" in events and a.propertyChanged: return True
            if "Tree" in events and a.treeChanged: return True
            if events.intersection({"File", "Addon"}) and \
                (a.sceneUpdate or a.frameChanged or a.propertyChanged or a.treeChanged): return True

        return customTriggerHasBeenActivated

    def autoExecute(self):
        self._execute()
        self.autoExecution.lastExecutionTimestamp = time.clock()

    def execute(self):
        setupExecutionUnits()
        self._execute()
        finishExecutionUnits()

    def _execute(self):
        units = self.mainUnits
        if len(units) == 0:
            self.lastExecutionInfo.executionTime = 0
            return

        allExecutionsSuccessfull = True

        start = time.perf_counter()
        for unit in units:
            success = unit.execute()
            if not success:
                allExecutionsSuccessfull = False
        end = time.perf_counter()

        if allExecutionsSuccessfull:
            self.lastExecutionInfo.executionTime = end - start
            self.lastExecutionInfo.updateVersions()

    @property
    def hasMainExecutionUnits(self):
        return len(self.mainUnits) > 0

    @property
    def mainUnits(self):
        return getMainUnitsByNodeTree(self)

    @property
    def scene(self):
        scene = bpy.data.scenes.get(self.sceneName)
        if scene is None:
            scene = bpy.data.scenes[0]
        return scene

    @property
    def timeSinceLastAutoExecution(self):
        return abs(time.clock() - self.autoExecution.lastExecutionTimestamp)

    @property
    def networks(self):
        return getNetworksByNodeTree(self)

    @property
    def subprogramNetworks(self):
        return getSubprogramNetworksByNodeTree(self)

@eventHandler("SCENE_UPDATE_POST")
def updateSelectedScenes(scene):
    for tree in getAnimationNodeTrees():
        scene = tree.scene
        if scene.name != tree.sceneName:
            tree.sceneName = scene.name
