import bpy
from .. problems import canExecute
from .. utils.layout import writeText
from .. utils.timing import prettyTime
from .. preferences import getExecutionCodeSettings

class TreePanel(bpy.types.Panel):
    bl_idname = "an_tree_panel"
    bl_label = "Animation Node Tree"
    bl_space_type = "NODE_EDITOR"
    bl_region_type = "TOOLS"
    bl_category = "Animation Nodes"

    @classmethod
    def poll(cls, context):
        tree = cls.getTree()
        if tree is None: return False
        return tree.bl_idname == "an_AnimationNodeTree"

    def draw(self, context):
        tree = self.getTree()
        layout = self.layout

        col = layout.column()
        col.scale_y = 1.5
        props = col.operator("an.execute_tree", icon = "PLAY")
        props.name = tree.name

        if not canExecute():
            layout.label("Look in the 'Problems' panel", icon = "INFO")

        row = layout.row(align = True)
        row.label(prettyTime(tree.lastExecutionInfo.executionTime), icon = "TIME")
        row.prop(getExecutionCodeSettings(), "measureExecution", text = "Details", emboss = False)

        layout.separator()
        layout.prop_search(tree, "sceneName", bpy.data, "scenes", icon = "SCENE_DATA", text = "Scene")
        layout.prop(tree, "editNodeLabels")


    @classmethod
    def getTree(cls):
        return bpy.context.space_data.edit_tree
