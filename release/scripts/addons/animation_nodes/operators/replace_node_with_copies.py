import bpy

class ReplaceNodesWithCopies(bpy.types.Operator):
    bl_idname = "an.replace_nodes_with_copies"
    bl_label = "Replace Nodes with Copies"

    @classmethod
    def poll(cls, context):
        try: return context.space_data.node_tree.bl_idname == "an_AnimationNodeTree"
        except: return False

    def execute(self, context):
        bpy.ops.node.select_all(action = "SELECT")
        bpy.ops.node.clipboard_copy()
        bpy.ops.node.delete()
        bpy.ops.node.clipboard_paste()
        return {"FINISHED"}
