"""
Dependency graph: bpy.data.meshes.new_from_object()
+++++++++++++++++++++++++++++++++++++++++++++++++++

Function to copy a new mesh from any object with geometry. The mesh is added to the main
database and can be referenced by objects. Typically used by tools that create new objects
or apply modifiers.

When is used on original object, the result mesh is calculated from the object without taking
animation or modifiers into account:

- For meshes this is similar to duplicating the source mesh.
- For curves this disables own modifiers, and modifiers of objects used as bevel and taper.
- For metaballs this produces an empty mesh since polygonization is done as a modifier evaluation.

When is used on evaluated object all modifiers are taken into account.

All the references (such as materials) are re-mapped to original. This ensures validity and
consistency of the main database.

.. note:: If object does not have geometry (i.e. camera) the functions returns None.
"""
import bpy


class OBJECT_OT_mesh_from_object(bpy.types.Operator):
    """Convert selected object to mesh and show number of vertices"""
    bl_label = "DEG Mesh From Object"
    bl_idname = "object.mesh_from_object"

    def execute(self, context):
        # Access input original object.
        object = context.object
        if object is None:
            self.report({'INFO'}, "No active mesh object to convert to mesh")
            return {'CANCELLED'}
        # Avoid annoying None checks later on.
        if object.type not in {'MESH', 'CURVE', 'SURFACE', 'FONT', 'META'}:
            self.report({'INFO'}, "Object can not be converted to mesh")
            return {'CANCELLED'}
        depsgraph = context.evaluated_depsgraph_get()
        object_eval = object.evaluated_get(depsgraph)
        mesh_from_eval = bpy.data.meshes.new_from_object(object_eval)
        self.report({'INFO'}, f"{len(mesh_from_eval.vertices)} in new mesh, and is ready for use!")
        return {'FINISHED'}


def register():
    bpy.utils.register_class(OBJECT_OT_mesh_from_object)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_mesh_from_object)


if __name__ == "__main__":
    register()
