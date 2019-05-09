"""
Dependency graph: Object.to_mesh()
+++++++++++++++++++++++++++++++++++

Object.to_mesh() (and bpy.data.meshes.new_from_object()) are closely interacting with dependency
graph: their behavior depends on whether they are used on original or evaluated object.

When is used on original object, the result mesh is calculated from the object without taking
animation or modifiers into account:

- For meshes this is similar to duplicating the source mesh.
- For curves this disables own modifiers, and modifiers of objects used as bevel and taper.
- For metaballs this produces an empty mesh since polygonization is done as a modifier evaluation.

When is used on evaluated object all modifiers are taken into account.

.. note:: The result mesh is added to the main database.
.. note:: If object does not have geometry (i.e. camera) the functions returns None.
"""
import bpy


class OBJECT_OT_object_to_mesh(bpy.types.Operator):
    """Convert selected object to mesh and show number of vertices"""
    bl_label = "DEG Object to Mesh"
    bl_idname = "object.object_to_mesh"

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
        # Invoke to_mesh() for original object.
        mesh_from_orig = object.to_mesh()
        self.report({'INFO'}, f"{len(mesh_from_orig.vertices)} in new mesh without modifiers.")
        # Remove temporary mesh.
        bpy.data.meshes.remove(mesh_from_orig)
        # Invoke to_mesh() for evaluated object.
        object_eval = object.evaluated_get(depsgraph)
        mesh_from_eval = object_eval.to_mesh()
        self.report({'INFO'}, f"{len(mesh_from_eval.vertices)} in new mesh with modifiers.")
        # Remove temporary mesh.
        bpy.data.meshes.remove(mesh_from_eval)
        return {'FINISHED'}


def register():
    bpy.utils.register_class(OBJECT_OT_object_to_mesh)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_object_to_mesh)


if __name__ == "__main__":
    register()
