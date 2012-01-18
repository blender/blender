bl_info = {
    "name": "New Object",
    "author": "YourNameHere",
    "version": (1, 0),
    "blender": (2, 5, 5),
    "location": "View3D > Add > Mesh > New Object",
    "description": "Adds a new Mesh Object",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Add Mesh"}


import bpy
from bpy.props import FloatVectorProperty
from add_utils import AddObjectHelper, add_object_data
from mathutils import Vector


def add_object(self, context):
    scale_x = self.scale.x
    scale_y = self.scale.y

    verts = [Vector((-1 * scale_x, 1 * scale_y, 0)),
             Vector((1 * scale_x, 1 * scale_y, 0)),
             Vector((1 * scale_x, -1 * scale_y, 0)),
             Vector((-1 * scale_x, -1 * scale_y, 0)),
            ]

    edges = []
    faces = [[0, 1, 2, 3]]

    mesh = bpy.data.meshes.new(name='New Object Mesh')
    mesh.from_pydata(verts, edges, faces)
    # useful for development when the mesh may be invalid.
    # mesh.validate(verbose=True)
    add_object_data(context, mesh, operator=self)


class OBJECT_OT_add_object(bpy.types.Operator, AddObjectHelper):
    """Add a Mesh Object"""
    bl_idname = "mesh.add_object"
    bl_label = "Add Mesh Object"
    bl_description = "Create a new Mesh Object"
    bl_options = {'REGISTER', 'UNDO'}

    scale = FloatVectorProperty(
            name='scale',
            default=(1.0, 1.0, 1.0),
            subtype='TRANSLATION',
            description='scaling',
            )

    def execute(self, context):

        add_object(self, context)

        return {'FINISHED'}


# Registration

def add_object_button(self, context):
    self.layout.operator(
        OBJECT_OT_add_object.bl_idname,
        text="Add Object",
        icon="PLUGIN")


def register():
    bpy.utils.register_class(OBJECT_OT_add_object)
    bpy.types.INFO_MT_mesh_add.append(add_object_button)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_add_object)
    bpy.types.INFO_MT_mesh_add.remove(add_object_button)


if __name__ == '__main__':
    register()
