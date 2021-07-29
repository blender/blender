import bpy
import itertools
from . import problems
from . update import updateEverything
from . utils.recursion import noRecursion
from . tree_info import iterSocketsThatNeedUpdate
from . utils.nodes import iterNodesInAnimationNodeTrees, getAnimationNodeTrees
from . execution.units import setupExecutionUnits, finishExecutionUnits
from . execution.auto_execution import iterAutoExecutionNodeTrees, executeNodeTrees, afterExecution

@noRecursion
def update(events):
    if failsToWriteToIDClasses():
        print("Skip event: cannot write to ID classes")
        return

    if didNameChange() or events.intersection({"File", "Addon", "Tree"}):
        updateEverything()

    updateProperties()

    if problems.canAutoExecute():
        nodeTrees = list(iterAutoExecutionNodeTrees(events))
        if len(nodeTrees) > 0:
            setupExecutionUnits()
            executeNodeTrees(nodeTrees)
            afterExecution()
            finishExecutionUnits()


def failsToWriteToIDClasses():
    try:
        scene = bpy.data.scenes[0]
        # just try to set a random property
        scene["AN Prop Test"] = 0
        del scene["AN Prop Test"]
        return False
    except: return True

oldNamesHash = 0

def didNameChange():
    global oldNamesHash
    newHash = getNamesHash()
    if newHash != oldNamesHash:
        oldNamesHash = newHash
        return True
    return False

def getNamesHash():
    names = set(itertools.chain(
        (tree.name for tree in getAnimationNodeTrees()),
        (node.name for node in iterNodesInAnimationNodeTrees())))
    return names

def updateProperties():
    for socket in iterSocketsThatNeedUpdate():
        socket.updateProperty()

    for tree in getAnimationNodeTrees():
        tree.autoExecution.customTriggers.updateProperties()
