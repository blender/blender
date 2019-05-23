"""
Dependency graph: Evaluated ID example
++++++++++++++++++++++++++++++++++++++

This example demonstrates access to the evaluated ID (such as object, material, etc.) state from
an original ID.
This is needed every time one needs to access state with animation, constraints, and modifiers
taken into account.
"""
import bpy


class OBJECT_OT_evaluated_example(bpy.types.Operator):
    """Access evaluated object state and do something with it"""
    bl_label = "DEG Access Evaluated Object"
    bl_idname = "object.evaluated_example"

    def execute(self, context):
        # This is an original object. Its data does not have any modifiers applied.
        obj = context.object
        if obj is None or obj.type != 'MESH':
            self.report({'INFO'}, "No active mesh object to get info from")
            return {'CANCELLED'}
        # Evaluated object exists within a specific dependency graph.
        # We will request evaluated object from the dependency graph which corresponds to the
        # current scene and view layer.
        #
        # NOTE: This call ensure the dependency graph is fully evaluated. This might be expensive
        # if changes were made made to the scene, but is needed to ensure no dangling or incorrect
        # pointers are exposed.
        depsgraph = context.evaluated_depsgraph_get()
        # Actually request evaluated object.
        #
        # This object has animation and drivers applied on it, together with constraints and
        # modifiers.
        #
        # For mesh objects the object.data will be a mesh with all modifiers applied.
        # This means that in access to vertices or faces after modifier stack happens via fields of 
        # object_eval.object.
        #
        # For other types of objects the object_eval.data does not have modifiers applied on it,
        # but has animation applied.
        #
        # NOTE: All ID types have `evaluated_get()`, including materials, node trees, worlds.
        object_eval = obj.evaluated_get(depsgraph)
        mesh_eval = object_eval.data
        self.report({'INFO'}, f"Number of evaluated vertices: {len(mesh_eval.vertices)}")
        return {'FINISHED'}


def register():
    bpy.utils.register_class(OBJECT_OT_evaluated_example)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_evaluated_example)


if __name__ == "__main__":
    register()
