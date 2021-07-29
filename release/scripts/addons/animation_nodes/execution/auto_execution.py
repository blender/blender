import bpy
from .. import problems
from .. preferences import getPreferences
from .. utils.blender_ui import redrawAll
from .. utils.nodes import getAnimationNodeTrees

def iterAutoExecutionNodeTrees(events):
    if not problems.canExecute(): return
    for nodeTree in getAnimationNodeTrees():
        if nodeTree.canAutoExecute(events):
            yield nodeTree

def executeNodeTrees(nodeTrees):
    for nodeTree in nodeTrees:
        nodeTree.autoExecute()

def afterExecution():
    for scene in bpy.data.scenes:
        scene.update()

    from .. events import isRendering
    if not isRendering():
        redrawAll()
