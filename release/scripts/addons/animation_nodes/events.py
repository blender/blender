import bpy
from . import tree_info
from . import event_handler
from . utils.handlers import eventHandler
from . execution.measurements import resetMeasurements

class EventState:
    def __init__(self):
        self.reset()
        self.isRendering = False

    def reset(self):
        self.treeChanged = False
        self.fileChanged = False
        self.sceneChanged = False
        self.frameChanged = False
        self.addonChanged = False
        self.propertyChanged = False

    def getActives(self):
        events = set()
        if self.treeChanged: events.add("Tree")
        if self.fileChanged: events.add("File")
        if self.addonChanged: events.add("Addon")
        if self.sceneChanged: events.add("Scene")
        if self.frameChanged: events.add("Frame")
        if self.propertyChanged: events.add("Property")
        return events

event = EventState()


@eventHandler("SCENE_UPDATE_POST")
def sceneUpdated(scene):
    event.sceneChanged = True
    evaluateRaisedEvents()

@eventHandler("RENDER_PRE")
def renderFramePre():
    event.frameChanged = True
    evaluateRaisedEvents()

def evaluateRaisedEvents():
    event_handler.update(event.getActives())
    event.reset()


@eventHandler("FRAME_CHANGE_POST")
def frameChanged(scene):
    event.frameChanged = True

def propertyChanged(self = None, context = None):
    event.propertyChanged = True
    resetMeasurements()

@eventHandler("FILE_LOAD_POST")
def fileLoaded():
    from . base_types.update_file import updateFile
    from . nodes.subprogram.subprogram_sockets import forceSubprogramUpdate
    updateFile()
    tree_info.updateIfNecessary()
    forceSubprogramUpdate()
    event.fileChanged = True
    treeChanged()

@eventHandler("ADDON_LOAD_POST")
def addonChanged():
    event.addonChanged = True
    treeChanged()

def executionCodeChanged(self = None, context = None):
    treeChanged()
    propertyChanged()

def networkChanged(self = None, context = None):
    treeChanged()

def treeChanged(self = None, context = None):
    event.treeChanged = True
    tree_info.treeChanged()


@eventHandler("RENDER_INIT")
def renderInitialized():
    event.isRendering = True

@eventHandler("RENDER_CANCEL")
@eventHandler("RENDER_COMPLETE")
def renderEnd():
    event.isRendering = False

def isRendering():
    return event.isRendering
