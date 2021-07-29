import bpy
from .. utils.handlers import eventHandler
from .. utils.nodes import getAnimationNodeTrees

treeNameBySpace = {}

@eventHandler("SCENE_UPDATE_POST")
def updateAutoNodeTreeSelection(scene):
    nodeTrees = getAnimationNodeTrees()
    if len(nodeTrees) == 0: return

    for space in getAnimationNodeEditorSpaces():
        spaceHash = str(hash(space))

        if space.node_tree is None:
            if len(nodeTrees) == 1:
                space.node_tree = nodeTrees[0]
            else:
                lastUsedTree = bpy.data.node_groups.get(treeNameBySpace.get(spaceHash, ""))
                if lastUsedTree is not None:
                    space.node_tree = lastUsedTree
                else:
                    space.node_tree = nodeTrees[0]

        treeName = getattr(space.node_tree, "name", None)
        if treeName is not None:
            treeNameBySpace[spaceHash] = treeName

def getAnimationNodeEditorSpaces():
    spaces = []
    for area in getattr(bpy.context.screen, "areas", []):
        if area.type == "NODE_EDITOR":
            space = area.spaces.active
            if space.tree_type == "an_AnimationNodeTree":
                spaces.append(space)
    return spaces
