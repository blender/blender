# gpl: author Dannyboy

bl_info = {
    "name": "Add Random Box Structure",
    "author": "Dannyboy",
    "version": (1, 0, 1),
    "location": "View3D > Add > Make Box Structure",
    "description": "Fill selected box shaped meshes with randomly sized cubes",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "dannyboypython.blogspot.com",
    "category": "Object"}

import bpy
import random
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        FloatProperty,
        FloatVectorProperty,
        IntProperty,
        )


class makestructure(Operator):
    bl_idname = "object.make_structure"
    bl_label = "Add Random Box Structure"
    bl_description = ("Create a randomized structure made of boxes\n"
                      "with various control parameters\n"
                      "Needs an existing Active Mesh Object")
    bl_options = {'REGISTER', 'UNDO'}

    dc = BoolProperty(
            name="Delete Base Mesh(es)",
            default=True
            )
    wh = BoolProperty(
            name="Stay Within Bounds",
            description="Keeps cubes from exceeding base mesh bounds",
            default=True
            )
    uf = BoolProperty(
            name="Uniform Cube Quantity",
            default=False
            )
    qn = IntProperty(
            name="Cube Quantity",
            default=10,
            min=1, max=1500
            )
    mn = FloatVectorProperty(
            name="Min Scales",
            default=(0.1, 0.1, 0.1),
            subtype='XYZ'
            )
    mx = FloatVectorProperty(
            name="Max Scales",
            default=(2.0, 2.0, 2.0),
            subtype='XYZ'
            )
    lo = FloatVectorProperty(
            name="XYZ Offset",
            default=(0.0, 0.0, 0.0),
            subtype='XYZ'
            )
    rsd = FloatProperty(
            name="Random Seed",
            default=1
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.type == "MESH" and obj.mode == "OBJECT"

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        box.label(text="Options:")
        box.prop(self, "dc")
        box.prop(self, "wh")
        box.prop(self, "uf")

        box = layout.box()
        box.label(text="Parameters:")
        box.prop(self, "qn")
        box.prop(self, "mn")
        box.prop(self, "mx")
        box.prop(self, "lo")
        box.prop(self, "rsd")

    def execute(self, context):
        rsdchange = self.rsd
        oblst = []
        uvyes = 0
        bpy.ops.group.create(name='Cubagrouper')
        bpy.ops.group.objects_remove()

        for ob in bpy.context.selected_objects:
            oblst.append(ob)

        for obj in oblst:
            bpy.ops.object.select_pattern(pattern=obj.name)  # Select base mesh
            bpy.context.scene.objects.active = obj
            if obj.data.uv_layers[:] != []:
                uvyes = 1
            else:
                uvyes = 0
            bpy.ops.object.group_link(group='Cubagrouper')
            dim = obj.dimensions
            rot = obj.rotation_euler
            if self.uf is True:
                area = dim.x * dim.y * dim.z
            else:
                area = 75

            for cube in range(round((area / 75) * self.qn)):
                random.seed(rsdchange)
                pmn = self.mn  # Proxy values
                pmx = self.mx
                if self.wh is True:
                    if dim.x < pmx.x:  # Keeping things from exceeding proper size
                        pmx.x = dim.x
                    if dim.y < pmx.y:
                        pmx.y = dim.y
                    if dim.z < pmx.z:
                        pmx.z = dim.z
                if 0.0 > pmn.x:  # Keeping things from going under zero
                    pmn.x = 0.0
                if 0.0 > pmn.y:
                    pmn.y = 0.0
                if 0.0 > pmn.z:
                    pmn.z = 0.0
                sx = (random.random() * (pmx.x - pmn.x)) + pmn.x  # Just changed self.mx and .mn to pmx.
                sy = (random.random() * (pmx.y - pmn.y)) + pmn.y
                sz = (random.random() * (pmx.z - pmn.z)) + pmn.z
                if self.wh is True:  # This keeps the cubes within the base mesh
                    ex = (random.random() * (dim.x - sx)) - ((dim.x - sx) / 2) + obj.location.x
                    wy = (random.random() * (dim.y - sy)) - ((dim.y - sy) / 2) + obj.location.y
                    ze = (random.random() * (dim.z - sz)) - ((dim.z - sz) / 2) + obj.location.z
                elif self.wh is False:
                    ex = (random.random() * dim.x) - (dim.x / 2) + obj.location.x
                    wy = (random.random() * dim.y) - (dim.y / 2) + obj.location.y
                    ze = (random.random() * dim.z) - (dim.z / 2) + obj.location.z
                bpy.ops.mesh.primitive_cube_add(
                            radius=0.5, location=(ex + self.lo.x, wy + self.lo.y, ze + self.lo.z)
                            )
                bpy.ops.object.mode_set(mode='EDIT')
                bpy.ops.mesh.select_all(action='SELECT')
                bpy.ops.transform.resize(
                    value=(sx, sy, sz), constraint_axis=(True, True, True),
                    constraint_orientation='GLOBAL', mirror=False, proportional='DISABLED',
                    proportional_edit_falloff='SMOOTH', proportional_size=1, release_confirm=True
                    )
                bpy.ops.object.mode_set(mode='OBJECT')
                select = bpy.context.object  # This is used to keep something selected for poll()
                bpy.ops.object.group_link(group='Cubagrouper')
                rsdchange += 3
            bpy.ops.object.select_grouped(type='GROUP')
            bpy.ops.transform.rotate(
                    value=rot[0], axis=(1, 0, 0), constraint_axis=(False, False, False),
                    constraint_orientation='GLOBAL', mirror=False, proportional='DISABLED',
                    proportional_edit_falloff='SMOOTH', proportional_size=1, release_confirm=True
                    )
            bpy.ops.transform.rotate(
                    value=rot[1], axis=(0, 1, 0), constraint_axis=(False, False, False),
                    constraint_orientation='GLOBAL', mirror=False, proportional='DISABLED',
                    proportional_edit_falloff='SMOOTH', proportional_size=1, release_confirm=True
                    )
            bpy.ops.transform.rotate(
                    value=rot[2], axis=(0, 0, 1), constraint_axis=(False, False, False),
                    constraint_orientation='GLOBAL', mirror=False, proportional='DISABLED',
                    proportional_edit_falloff='SMOOTH', proportional_size=1, release_confirm=True
                    )
            bpy.context.scene.objects.active = obj  # Again needed to avoid poll() taking me down
            bpy.ops.object.make_links_data(type='MODIFIERS')
            bpy.ops.object.make_links_data(type='MATERIAL')

            if uvyes == 1:
                bpy.ops.object.join_uvs()

            bpy.ops.group.objects_remove()
            bpy.context.scene.objects.active = select

            if self.dc is True:
                bpy.context.scene.objects.unlink(obj)

        return {'FINISHED'}


def register():
    bpy.utils.register_class(makestructure)


def unregister():
    bpy.utils.unregister_class(makestructure)


if __name__ == "__main__":
    register()
