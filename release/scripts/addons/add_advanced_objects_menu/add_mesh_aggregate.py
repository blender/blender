# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# Simple aggregate of particles / meshes
# Copy the selected objects on the active object
# Based on the position of the cursor and a defined volume
# Allows to control growth by using a Build modifier

bl_info = {
    "name": "Aggregate Mesh",
    "author": "liero",
    "version": (0, 0, 5),
    "blender": (2, 7, 0),
    "location": "View3D > Tool Shelf",
    "description": "Adds geometry to a mesh like in DLA aggregators",
    "category": "Object"}


import bpy
import bmesh
from random import (
        choice,
        gauss,
        seed,
        )
from mathutils import Matrix
from bpy.props import (
        BoolProperty,
        FloatProperty,
        IntProperty,
        )
from bpy.types import Operator


def use_random_seed(self):
    seed(self.rSeed)
    return


def rg(n):
    return (round(gauss(0, n), 2))


def remover(sel=False):
    bpy.ops.object.editmode_toggle()
    if sel:
        bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=0.0001)
    bpy.ops.object.mode_set()


class OBJECT_OT_agregate_mesh(Operator):
    bl_idname = "object.agregate_mesh"
    bl_label = "Aggregate"
    bl_description = ("Adds geometry to a mesh like in DLA aggregators\n"
                      "Needs at least two selected Mesh objects")
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    volX = FloatProperty(
            name="Volume X",
            min=0.1, max=25,
            default=3,
            description="The cloud around cursor"
            )
    volY = FloatProperty(
            name="Volume Y",
            min=0.1, max=25,
            default=3,
            description="The cloud around cursor"
            )
    volZ = FloatProperty(
            name="Volume Z",
            min=0.1, max=25,
            default=3,
            description="The cloud around cursor"
            )
    baseSca = FloatProperty(
            name="Scale",
            min=0.01, max=5,
            default=.25,
            description="Particle Scale"
            )
    varSca = FloatProperty(
            name="Var",
            min=0, max=1,
            default=0,
            description="Particle Scale Variation"
            )
    rotX = FloatProperty(
            name="Rot Var X",
            min=0, max=2,
            default=0,
            description="X Rotation Variation"
            )
    rotY = FloatProperty(
            name="Rot Var Y",
            min=0, max=2,
            default=0,
            description="Y Rotation Variation"
            )
    rotZ = FloatProperty(
            name="Rot Var Z",
            min=0, max=2,
            default=1,
            description="Z Rotation Variation"
            )
    rSeed = IntProperty(
            name="Random seed",
            min=0, max=999999,
            default=1,
            description="Seed to feed random values"
            )
    numP = IntProperty(
            name="Number",
            min=1,
            max=9999, soft_max=500,
            default=50,
            description="Number of particles"
            )
    nor = BoolProperty(
            name="Normal Oriented",
            default=False,
            description="Align Z axis with Faces normals"
            )
    cent = BoolProperty(
            name="Use Face Center",
            default=False,
            description="Center on Faces"
            )
    track = BoolProperty(
            name="Cursor Follows",
            default=False,
            description="Cursor moves as structure grows / more compact results"
            )
    anim = BoolProperty(
            name="Animatable",
            default=False,
            description="Sort faces so you can regrow with Build Modifier, materials are lost"
            )
    refresh = BoolProperty(
            name="Update",
            default=False
            )
    auto_refresh = BoolProperty(
            name="Auto",
            description="Auto update spline",
            default=False
            )

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        row = col.row(align=True)

        if self.auto_refresh is False:
            self.refresh = False
        elif self.auto_refresh is True:
            self.refresh = True

        row.prop(self, "auto_refresh", toggle=True, icon="AUTO")
        row.prop(self, "refresh", toggle=True, icon="FILE_REFRESH")

        col = layout.column(align=True)
        col.separator()

        col = layout.column(align=True)
        col.prop(self, "volX", slider=True)
        col.prop(self, "volY", slider=True)
        col.prop(self, "volZ", slider=True)

        layout.label(text="Particles:")
        col = layout.column(align=True)
        col.prop(self, "baseSca", slider=True)
        col.prop(self, "varSca", slider=True)

        col = layout.column(align=True)
        col.prop(self, "rotX", slider=True)
        col.prop(self, "rotY", slider=True)
        col.prop(self, "rotZ", slider=True)

        col = layout.column(align=True)
        col.prop(self, "rSeed", slider=False)
        col.prop(self, "numP")

        row = layout.row(align=True)
        row.prop(self, "nor")
        row.prop(self, "cent")

        row = layout.row(align=True)
        row.prop(self, "track")
        row.prop(self, "anim")

    @classmethod
    def poll(cls, context):
        return (len(bpy.context.selected_objects) > 1 and
                bpy.context.object.type == 'MESH')

    def invoke(self, context, event):
        self.refresh = True
        return self.execute(context)

    def execute(self, context):
        if not self.refresh:
            return {'PASS_THROUGH'}

        scn = bpy.context.scene
        obj = bpy.context.active_object

        use_random_seed(self)

        mat = Matrix((
                (1, 0, 0, 0),
                (0, 1, 0, 0),
                (0, 0, 1, 0),
                (0, 0, 0, 1))
                )
        if obj.matrix_world != mat:
            self.report({'WARNING'},
                         "Please, Apply transformations to Active Object first")
            return{'FINISHED'}

        par = [o for o in bpy.context.selected_objects if o.type == 'MESH' and o != obj]
        if not par:
            return{'FINISHED'}

        bpy.ops.object.mode_set()
        bpy.ops.object.select_all(action='DESELECT')
        obj.select = True
        msv = []

        for i in range(len(obj.modifiers)):
            msv.append(obj.modifiers[i].show_viewport)
            obj.modifiers[i].show_viewport = False

        cur = scn.cursor_location
        for i in range(self.numP):

            mes = choice(par).data
            newobj = bpy.data.objects.new('nuevo', mes)
            scn.objects.link(newobj)
            origen = (rg(self.volX) + cur[0], rg(self.volY) + cur[1], rg(self.volZ) + cur[2])

            cpom = obj.closest_point_on_mesh(origen)

            if self.cent:
                bm = bmesh.new()
                bm.from_mesh(obj.data)
                if hasattr(bm.verts, "ensure_lookup_table"):
                    bm.verts.ensure_lookup_table()
                    bm.faces.ensure_lookup_table()

                newobj.location = bm.faces[cpom[3]].calc_center_median()

                bm.free()
            else:
                newobj.location = cpom[1]

            if self.nor:
                newobj.rotation_mode = 'QUATERNION'
                newobj.rotation_quaternion = cpom[1].to_track_quat('Z', 'Y')
                newobj.rotation_mode = 'XYZ'
                newobj.rotation_euler[0] += rg(self.rotX)
                newobj.rotation_euler[1] += rg(self.rotY)
                newobj.rotation_euler[2] += rg(self.rotZ)
            else:
                newobj.rotation_euler = (rg(self.rotX), rg(self.rotY), rg(self.rotZ))

            newobj.scale = [self.baseSca + self.baseSca * rg(self.varSca)] * 3

            if self.anim:
                newobj.select = True
                bpy.ops.object.make_single_user(type='SELECTED_OBJECTS', obdata=True)
                bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

                bme = bmesh.new()
                bme.from_mesh(obj.data)

                tmp = bmesh.new()
                tmp.from_mesh(newobj.data)

                for f in tmp.faces:
                    # z = len(bme.verts)
                    for v in f.verts:
                        bme.verts.new(list(v.co))
                    bme.faces.new(bme.verts[-len(f.verts):])

                bme.to_mesh(obj.data)
                remover(True)
                # Note: foo.user_clear() is deprecated use do_unlink=True instead
                bpy.data.meshes.remove(newobj.data, do_unlink=True)

            else:
                scn.objects.active = obj
                newobj.select = True
                bpy.ops.object.join()

            if self.track:
                cur = scn.cursor_location = cpom[1]

        for i in range(len(msv)):
            obj.modifiers[i].show_viewport = msv[i]

        for o in par:
            o.select = True

        obj.select = True

        if self.auto_refresh is False:
            self.refresh = False

        return{'FINISHED'}


def register():
    bpy.utils.register_class(OBJECT_OT_agregate_mesh)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_agregate_mesh)


if __name__ == '__main__':
    register()
