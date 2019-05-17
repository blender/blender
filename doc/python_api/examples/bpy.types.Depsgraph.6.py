"""
Dependency graph: Simple exporter
+++++++++++++++++++++++++++++++++

This example is a combination of all previous ones, and shows how to write a simple exporter
script.
"""
import bpy


class OBJECT_OT_simple_exporter(bpy.types.Operator):
    """Simple (fake) exporter of selected objects"""
    bl_label = "DEG Export Selected"
    bl_idname = "object.simple_exporter"

    apply_modifiers: bpy.props.BoolProperty(name="Apply Modifiers")

    def execute(self, context):
        depsgraph = context.evaluated_depsgraph_get()
        for object_instance in depsgraph.object_instances:
            if not self.is_object_instance_from_selected(object_instance):
                # We only export selected objects
                continue
            # NOTE: This will create a mesh for every instance, which is not ideal at all. In
            # reality destination format will support some sort of instancing mechanism, so the
            # code here will simply say "instance this object at object_instance.matrix_world".
            mesh = self.create_mesh_for_object_instance(object_instance)
            if mesh is None:
                # Happens for non-geometry objects.
                continue
            print(f"Exporting mesh with {len(mesh.vertices)} vertices "
                   f"at {object_instance.matrix_world}")
            object_instace.to_mesh_clear()

        return {'FINISHED'}

    def is_object_instance_from_selected(self, object_instance):
        # For instanced objects we check selection of their instancer (more accurately: check
        # selection status of the original object corresponding to the instancer).
        if object_instance.parent:
            return object_instance.parent.original.select_get()
        # For non-instanced objects we check selection state of the original object.
        return object_instance.object.original.select_get()

    def create_mesh_for_object_instance(self, object_instance):
        if self.apply_modifiers:
            return object_instance.object.to_mesh()
        else:
            return object_instance.object.original.to_mesh()


def register():
    bpy.utils.register_class(OBJECT_OT_simple_exporter)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_simple_exporter)


if __name__ == "__main__":
    register()
