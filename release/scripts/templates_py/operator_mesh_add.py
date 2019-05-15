import bpy
import bmesh


def add_box(width, height, depth):
    """
    This function takes inputs and returns vertex and face arrays.
    no actual mesh data creation is done here.
    """

    verts = [
        (+1.0, +1.0, -1.0),
        (+1.0, -1.0, -1.0),
        (-1.0, -1.0, -1.0),
        (-1.0, +1.0, -1.0),
        (+1.0, +1.0, +1.0),
        (+1.0, -1.0, +1.0),
        (-1.0, -1.0, +1.0),
        (-1.0, +1.0, +1.0),
    ]

    faces = [
        (0, 1, 2, 3),
        (4, 7, 6, 5),
        (0, 4, 5, 1),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (4, 0, 3, 7),
    ]

    # apply size
    for i, v in enumerate(verts):
        verts[i] = v[0] * width, v[1] * depth, v[2] * height

    return verts, faces


from bpy.props import (
    BoolProperty,
    BoolVectorProperty,
    FloatProperty,
    FloatVectorProperty,
)


class AddBox(bpy.types.Operator):
    """Add a simple box mesh"""
    bl_idname = "mesh.primitive_box_add"
    bl_label = "Add Box"
    bl_options = {'REGISTER', 'UNDO'}

    width: FloatProperty(
        name="Width",
        description="Box Width",
        min=0.01, max=100.0,
        default=1.0,
    )
    height: FloatProperty(
        name="Height",
        description="Box Height",
        min=0.01, max=100.0,
        default=1.0,
    )
    depth: FloatProperty(
        name="Depth",
        description="Box Depth",
        min=0.01, max=100.0,
        default=1.0,
    )
    layers: BoolVectorProperty(
        name="Layers",
        description="Object Layers",
        size=20,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # generic transform props
    align_items = (
            ('WORLD', "World", "Align the new object to the world"),
            ('VIEW', "View", "Align the new object to the view"),
            ('CURSOR', "3D Cursor", "Use the 3D cursor orientation for the new object")
    )
    align: EnumProperty(
            name="Align",
            items=align_items,
            default='WORLD',
            update=AddObjectHelper.align_update_callback,
            )
    location: FloatVectorProperty(
        name="Location",
        subtype='TRANSLATION',
    )
    rotation: FloatVectorProperty(
        name="Rotation",
        subtype='EULER',
    )

    def execute(self, context):

        verts_loc, faces = add_box(
            self.width,
            self.height,
            self.depth,
        )

        mesh = bpy.data.meshes.new("Box")

        bm = bmesh.new()

        for v_co in verts_loc:
            bm.verts.new(v_co)

        bm.verts.ensure_lookup_table()
        for f_idx in faces:
            bm.faces.new([bm.verts[i] for i in f_idx])

        bm.to_mesh(mesh)
        mesh.update()

        # add the mesh as an object into the scene with this utility module
        from bpy_extras import object_utils
        object_utils.object_data_add(context, mesh, operator=self)

        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(AddBox.bl_idname, icon='MESH_CUBE')


def register():
    bpy.utils.register_class(AddBox)
    bpy.types.VIEW3D_MT_mesh_add.append(menu_func)


def unregister():
    bpy.utils.unregister_class(AddBox)
    bpy.types.VIEW3D_MT_mesh_add.remove(menu_func)


if __name__ == "__main__":
    register()

    # test call
    bpy.ops.mesh.primitive_box_add()
