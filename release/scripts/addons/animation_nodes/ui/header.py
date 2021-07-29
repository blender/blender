import bpy

class TemplatesMenuInHeader(bpy.types.Header):
    bl_idname = "an_templates_menu_in_header"
    bl_space_type = "NODE_EDITOR"

    def draw(self, context):
        if context.space_data.tree_type != "an_AnimationNodeTree": return

        layout = self.layout
        layout.separator()
        layout.menu("an_subprograms_menu", text = "Subprograms")

        layout.operator("an.remove_node_tree", text = "Remove", emboss = False)
