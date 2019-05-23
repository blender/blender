"""
Dependency graph: Original object example
+++++++++++++++++++++++++++++++++++++++++

This example demonstrates access to the original ID.
Such access is needed to check whether object is selected, or to compare pointers.
"""
import bpy


class OBJECT_OT_original_example(bpy.types.Operator):
    """Access original object and do something with it"""
    bl_label = "DEG Access Original Object"
    bl_idname = "object.original_example"

    def check_object_selected(self, object_eval):
        # Selection depends on a context and is only valid for original objects. This means we need
        # to request the original object from the known evaluated one.
        #
        # NOTE: All ID types have an `original` field.
        obj = object_eval.original
        return obj.select_get()

    def execute(self, context):
        # NOTE: It seems redundant to iterate over original objects to request evaluated ones
        # just to get original back. But we want to keep example as short as possible, but in real
        # world there are cases when evaluated object is coming from a more meaningful source.
        depsgraph = context.evaluated_depsgraph_get()
        for obj in context.editable_objects:
            object_eval = obj.evaluated_get(depsgraph)
            if self.check_object_selected(object_eval):
                self.report({'INFO'}, f"Object is selected: {object_eval.name}")
        return {'FINISHED'}


def register():
    bpy.utils.register_class(OBJECT_OT_original_example)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_original_example)


if __name__ == "__main__":
    register()
