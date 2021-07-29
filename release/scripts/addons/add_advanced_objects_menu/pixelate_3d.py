# gpl author: liero
# very simple 'pixelization' or 'voxelization' engine #

bl_info = {
    "name": "3D Pixelate",
    "author": "liero",
    "version": (0, 5, 3),
    "blender": (2, 74, 0),
    "location": "View3D > Tool Shelf",
    "description": "Creates a 3d pixelated version of the object",
    "category": "Object"}

# Note: winmgr properties are moved to the operator


import bpy
from bpy.types import Operator
from bpy.props import (
        FloatProperty,
        IntProperty,
        )


def pix(self, obj):
    sce = bpy.context.scene
    obj.hide = obj.hide_render = True
    mes = obj.to_mesh(sce, True, 'RENDER')
    mes.transform(obj.matrix_world)
    dup = bpy.data.objects.new('dup', mes)
    sce.objects.link(dup)
    dup.dupli_type = 'VERTS'
    sce.objects.active = dup
    bpy.ops.object.mode_set()
    ver = mes.vertices

    for i in range(250):
        fin = True
        for i in dup.data.edges:
            d = ver[i.vertices[0]].co - ver[i.vertices[1]].co
            if d.length > self.size:
                ver[i.vertices[0]].select = True
                ver[i.vertices[1]].select = True
                fin = False
        bpy.ops.object.editmode_toggle()
        bpy.ops.mesh.subdivide(number_cuts=1, smoothness=self.smooth)
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.object.editmode_toggle()
        if fin:
            break

    for i in ver:
        for n in range(3):
            i.co[n] -= (.001 + i.co[n]) % self.size

    bpy.ops.object.mode_set(mode='EDIT', toggle=False)
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=0.0001)
    bpy.ops.mesh.delete(type='EDGE_FACE')
    bpy.ops.object.mode_set()
    sca = self.size * (100 - self.gap) * .005
    bpy.ops.mesh.primitive_cube_add(layers=[True] + [False] * 19)
    bpy.ops.transform.resize(value=[sca] * 3)
    bpy.context.scene.objects.active = dup
    bpy.ops.object.parent_set(type='OBJECT')


class Pixelate(Operator):
    bl_idname = "object.pixelate"
    bl_label = "Pixelate Object"
    bl_description = ("Create a 3d pixelated version of the object\n"
                      "using a Duplivert Box around each copied vertex\n"
                      "With high poly objects, it can take some time\n"
                      "Needs an existing Active Mesh Object")
    bl_options = {'REGISTER', 'UNDO'}

    size = FloatProperty(
            name="Size",
            min=.05, max=5,
            default=.25,
            description="Size of the cube / grid \n"
                        "Small values (below 0.1) can create a high polygon count"
            )
    gap = IntProperty(
            name="Gap",
            min=0, max=90,
            default=10,
            subtype='PERCENTAGE',
            description="Separation - percent of size"
            )
    smooth = FloatProperty(
            name="Smooth",
            min=0, max=1,
            default=.0,
            description="Smooth factor when subdividing mesh"
            )

    @classmethod
    def poll(cls, context):
        return (context.active_object and
                context.active_object.type == 'MESH' and
                context.mode == 'OBJECT')

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.prop(self, "size")
        col.prop(self, "gap")
        layout.prop(self, "smooth")

    def execute(self, context):
        objeto = bpy.context.object
        try:
            pix(self, objeto)

        except Exception as e:
            self.report({'WARNING'},
                        "Some operations could not be performed (See Console for more info)")

            print("\n[Add Advanced  Objects]\nOperator: "
                  "object.pixelate\nError: {}".format(e))

            return {'CANCELLED'}

        return {'FINISHED'}


def register():
    bpy.utils.register_class(Pixelate)


def unregister():
    bpy.utils.unregister_class(Pixelate)


if __name__ == '__main__':
    register()
