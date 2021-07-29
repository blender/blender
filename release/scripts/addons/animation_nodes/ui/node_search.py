import bpy
import itertools
from bpy.props import *
from .. utils.nodes import iterAnimationNodeClasses, newNodeAtCursor, invokeTranslation

itemsByIdentifier = {}

class NodeSearch(bpy.types.Operator):
    bl_idname = "an.node_search"
    bl_label = "Node Search"
    bl_options = {"REGISTER"}
    bl_property = "item"

    def getSearchItems(self, context):
        itemsByIdentifier.clear()
        items = []
        for item in itertools.chain(iterSingleNodeItems()):
            itemsByIdentifier[item.identifier] = item
            items.append((item.identifier, item.searchTag, ""))
        return items

    item = EnumProperty(items = getSearchItems)

    @classmethod
    def poll(cls, context):
        try: return context.space_data.node_tree.bl_idname == "an_AnimationNodeTree"
        except: return False

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {"CANCELLED"}

    def execute(self, context):
        itemsByIdentifier[self.item].insert()
        return {"FINISHED"}


class InsertItem:
    @property
    def identifier(self):
        return ""

    @property
    def searchTag(self):
        return ""

    def insert(self):
        pass



# Single Nodes
#################################

def iterSingleNodeItems():
    for node in iterAnimationNodeClasses():
        if not node.onlySearchTags:
            yield SingleNodeInsertionItem(node.bl_idname, node.bl_label)
        for customSearch in node.getSearchTags():
            if isinstance(customSearch, tuple):
                yield SingleNodeInsertionItem(node.bl_idname, customSearch[0], customSearch[1])
            else:
                yield SingleNodeInsertionItem(node.bl_idname, customSearch)

class SingleNodeInsertionItem:
    def __init__(self, idName, tag, settings = {}):
        self.idName = idName
        self.tag = tag
        self.settings = settings

    @property
    def identifier(self):
        return "single node - " + self.tag

    @property
    def searchTag(self):
        return self.tag

    def insert(self):
        node = newNodeAtCursor(self.idName)
        for key, value in self.settings.items():
            setattr(node, key, eval(value))
        invokeTranslation()
