# NOTE:
# Non-render objects have a zero size bounding box
# when only one of these objects is selected you will get
# a zero sized cube. Such as camera, lamp, lattice, armature, empty

bl_info = {
    "name": "Create Bounding Box",
    "author": "sambler",
    "version": (1,3),
    "blender": (2, 73, 0),
    "location": "View3D > Add > Mesh > Create Bounding Box",
    "description": "Create a mesh cube that encompasses all selected objects",
    "warning": "",
    "wiki_url": "https://github.com/sambler/addonsByMe/blob/master/create_bound_box.py",
    "tracker_url": "https://github.com/sambler/addonsByMe/issues",
    "category": "Add Mesh",
}

import bpy
import bmesh
from bpy.props import BoolProperty, FloatVectorProperty
import mathutils
from bpy_extras import object_utils

# from blender templates
def add_box(width, height, depth):
    """
    This function takes inputs and returns vertex and face arrays.
    no actual mesh data creation is done here.
    """

    verts = [(+1.0, +1.0, -1.0),
             (+1.0, -1.0, -1.0),
             (-1.0, -1.0, -1.0),
             (-1.0, +1.0, -1.0),
             (+1.0, +1.0, +1.0),
             (+1.0, -1.0, +1.0),
             (-1.0, -1.0, +1.0),
             (-1.0, +1.0, +1.0),
             ]

    faces = [(0, 1, 2, 3),
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

class CreateBoundingBox(bpy.types.Operator, object_utils.AddObjectHelper):
    """Create a mesh cube that encompasses all selected objects"""
    bl_idname = "mesh.boundbox_add"
    bl_label = "Create Bounding Box"
    bl_description = "Create a bounding box around selected objects"
    bl_options = {'REGISTER', 'UNDO'}

    # generic transform props
    view_align = BoolProperty(
            name="Align to View",
            default=False,
            )
    location = FloatVectorProperty(
            name="Location",
            subtype='TRANSLATION',
            )
    rotation = FloatVectorProperty(
            name="Rotation",
            subtype='EULER',
            )

    @classmethod
    def poll(cls, context):
        if len(context.selected_objects) == 0:
            return False
        return True

    def execute(self, context):
        minx, miny, minz = (999999.0,)*3
        maxx, maxy, maxz = (-999999.0,)*3
        for obj in context.selected_objects:
            for v in obj.bound_box:
                v_world = obj.matrix_world * mathutils.Vector((v[0],v[1],v[2]))

                if v_world[0] < minx:
                    minx = v_world[0]
                if v_world[0] > maxx:
                    maxx = v_world[0]

                if v_world[1] < miny:
                    miny = v_world[1]
                if v_world[1] > maxy:
                    maxy = v_world[1]

                if v_world[2] < minz:
                    minz = v_world[2]
                if v_world[2] > maxz:
                    maxz = v_world[2]

        verts_loc, faces = add_box((maxx-minx)/2, (maxz-minz)/2, (maxy-miny)/2)
        mesh = bpy.data.meshes.new("BoundingBox")
        bm = bmesh.new()
        for v_co in verts_loc:
            bm.verts.new(v_co)

        bm.verts.ensure_lookup_table()

        for f_idx in faces:
            bm.faces.new([bm.verts[i] for i in f_idx])

        bm.to_mesh(mesh)
        mesh.update()
        self.location[0] = minx+((maxx-minx)/2)
        self.location[1] = miny+((maxy-miny)/2)
        self.location[2] = minz+((maxz-minz)/2)
        bbox = object_utils.object_data_add(context, mesh, operator=self)
        # does a bounding box need to display more than the bounds??
        bbox.object.draw_type = 'BOUNDS'
        bbox.object.hide_render = True

        return {'FINISHED'}

def menu_boundbox(self, context):
    self.layout.operator(CreateBoundingBox.bl_idname, text=CreateBoundingBox.bl_label, icon="PLUGIN")

def register():
    bpy.utils.register_class(CreateBoundingBox)
    bpy.types.INFO_MT_mesh_add.append(menu_boundbox)

def unregister():
    bpy.utils.unregister_class(CreateBoundingBox)
    bpy.types.INFO_MT_mesh_add.remove(menu_boundbox)

if __name__ == "__main__":
    register()