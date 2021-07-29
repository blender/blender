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

# Repeats extrusion + rotation + scale for one or more faces
# Original code by liero
# Update by Jimmy Hazevoet 03/2017 for Blender 2.79
# normal rotation, probability, scaled offset, object coords, initial and per step noise


bl_info = {
    "name": "MExtrude Plus1",
    "author": "liero, Jimmy Hazevoet",
    "version": (1, 3, 0),
    "blender": (2, 77, 0),
    "location": "View3D > Tool Shelf",
    "description": "Repeat extrusions from faces to create organic shapes",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"}


import bpy
import bmesh
import random
from bpy.types import Operator
from random import gauss
from math import radians
from mathutils import (
        Euler, Vector,
        )
from bpy.props import (
        FloatProperty,
        IntProperty,
        BoolProperty,
        )


def gloc(self, r):
    return Vector((self.offx, self.offy, self.offz))


def vloc(self, r):
    random.seed(self.ran + r)
    return self.off * (1 + gauss(0, self.var1 / 3))


def nrot(self, n):
    return Euler((radians(self.nrotx) * n[0],
                  radians(self.nroty) * n[1],
                  radians(self.nrotz) * n[2]), 'XYZ')


def vrot(self, r):
    random.seed(self.ran + r)
    return Euler((radians(self.rotx) + gauss(0, self.var2 / 3),
                  radians(self.roty) + gauss(0, self.var2 / 3),
                  radians(self.rotz) + gauss(0, self.var2 / 3)), 'XYZ')


def vsca(self, r):
    random.seed(self.ran + r)
    return self.sca * (1 + gauss(0, self.var3 / 3))


class MExtrude(Operator):
    bl_idname = "object.mextrude"
    bl_label = "Multi Extrude"
    bl_description = ("Extrude selected Faces with Rotation,\n"
                      "Scaling, Variation, Randomization")
    bl_options = {"REGISTER", "UNDO", "PRESET"}

    off = FloatProperty(
            name="Offset",
            soft_min=0.001, soft_max=10,
            min=-100, max=100,
            default=1.0,
            description="Translation"
            )
    offx = FloatProperty(
            name="Loc X",
            soft_min=-10.0, soft_max=10.0,
            min=-100.0, max=100.0,
            default=0.0,
            description="Global Translation X"
            )
    offy = FloatProperty(
            name="Loc Y",
            soft_min=-10.0, soft_max=10.0,
            min=-100.0, max=100.0,
            default=0.0,
            description="Global Translation Y"
            )
    offz = FloatProperty(
            name="Loc Z",
            soft_min=-10.0, soft_max=10.0,
            min=-100.0, max=100.0,
            default=0.0,
            description="Global Translation Z"
            )
    rotx = FloatProperty(
            name="Rot X",
            min=-85, max=85,
            soft_min=-30, soft_max=30,
            default=0,
            description="X Rotation"
            )
    roty = FloatProperty(
            name="Rot Y",
            min=-85, max=85,
            soft_min=-30,
            soft_max=30,
            default=0,
            description="Y Rotation"
            )
    rotz = FloatProperty(
            name="Rot Z",
            min=-85, max=85,
            soft_min=-30, soft_max=30,
            default=-0,
            description="Z Rotation"
            )
    nrotx = FloatProperty(
            name="N Rot X",
            min=-85, max=85,
            soft_min=-30, soft_max=30,
            default=0,
            description="Normal X Rotation"
            )
    nroty = FloatProperty(
            name="N Rot Y",
            min=-85, max=85,
            soft_min=-30, soft_max=30,
            default=0,
            description="Normal Y Rotation"
            )
    nrotz = FloatProperty(
            name="N Rot Z",
            min=-85, max=85,
            soft_min=-30, soft_max=30,
            default=-0,
            description="Normal Z Rotation"
            )
    sca = FloatProperty(
            name="Scale",
            min=0.01, max=10,
            soft_min=0.5, soft_max=1.5,
            default=1.0,
            description="Scaling of the selected faces after extrusion"
            )
    var1 = FloatProperty(
            name="Offset Var", min=-10, max=10,
            soft_min=-1, soft_max=1,
            default=0,
            description="Offset variation"
            )
    var2 = FloatProperty(
            name="Rotation Var",
            min=-10, max=10,
            soft_min=-1, soft_max=1,
            default=0,
            description="Rotation variation"
            )
    var3 = FloatProperty(
            name="Scale Noise",
            min=-10, max=10,
            soft_min=-1, soft_max=1,
            default=0,
            description="Scaling noise"
            )
    var4 = IntProperty(
            name="Probability",
            min=0, max=100,
            default=100,
            description="Probability, chance of extruding a face"
            )
    num = IntProperty(
            name="Repeat",
            min=1, max=500,
            soft_max=100,
            default=5,
            description="Repetitions"
            )
    ran = IntProperty(
            name="Seed",
            min=-9999, max=9999,
            default=0,
            description="Seed to feed random values"
            )
    opt1 = BoolProperty(
            name="Polygon coordinates",
            default=True,
            description="Polygon coordinates, Object coordinates"
            )
    opt2 = BoolProperty(
            name="Proportional offset",
            default=False,
            description="Scale * Offset"
            )
    opt3 = BoolProperty(
            name="Per step rotation noise",
            default=False,
            description="Per step rotation noise, Initial rotation noise"
            )
    opt4 = BoolProperty(
            name="Per step scale noise",
            default=False,
            description="Per step scale noise, Initial scale noise"
            )

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.type == 'MESH')

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        col.label(text="Transformations:")
        col.prop(self, "off", slider=True)
        col.prop(self, "offx", slider=True)
        col.prop(self, "offy", slider=True)
        col.prop(self, "offz", slider=True)

        col = layout.column(align=True)
        col.prop(self, "rotx", slider=True)
        col.prop(self, "roty", slider=True)
        col.prop(self, "rotz", slider=True)
        col.prop(self, "nrotx", slider=True)
        col.prop(self, "nroty", slider=True)
        col.prop(self, "nrotz", slider=True)
        col = layout.column(align=True)
        col.prop(self, "sca", slider=True)

        col = layout.column(align=True)
        col.label(text="Variation settings:")
        col.prop(self, "var1", slider=True)
        col.prop(self, "var2", slider=True)
        col.prop(self, "var3", slider=True)
        col.prop(self, "var4", slider=True)
        col.prop(self, "ran")
        col = layout.column(align=False)
        col.prop(self, 'num')

        col = layout.column(align=True)
        col.label(text="Options:")
        col.prop(self, "opt1")
        col.prop(self, "opt2")
        col.prop(self, "opt3")
        col.prop(self, "opt4")

    def execute(self, context):
        obj = bpy.context.object
        om = obj.mode
        bpy.context.tool_settings.mesh_select_mode = [False, False, True]
        origin = Vector([0.0, 0.0, 0.0])

        # bmesh operations
        bpy.ops.object.mode_set()
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        sel = [f for f in bm.faces if f.select]

        after = []

        # faces loop
        for i, of in enumerate(sel):
            nro = nrot(self, of.normal)
            off = vloc(self, i)
            loc = gloc(self, i)
            of.normal_update()

            # initial rotation noise
            if self.opt3 is False:
                rot = vrot(self, i)
            # initial scale noise
            if self.opt4 is False:
                s = vsca(self, i)

            # extrusion loop
            for r in range(self.num):
                # random probability % for extrusions
                if self.var4 > int(random.random() * 100):
                    nf = of.copy()
                    nf.normal_update()
                    no = nf.normal.copy()

                    # face/obj coördinates
                    if self.opt1 is True:
                        ce = nf.calc_center_bounds()
                    else:
                        ce = origin

                    # per step rotation noise
                    if self.opt3 is True:
                        rot = vrot(self, i + r)
                    # per step scale noise
                    if self.opt4 is True:
                        s = vsca(self, i + r)

                    # proportional, scale * offset
                    if self.opt2 is True:
                        off = s * off

                    for v in nf.verts:
                        v.co -= ce
                        v.co.rotate(nro)
                        v.co.rotate(rot)
                        v.co += ce + loc + no * off
                        v.co = v.co.lerp(ce, 1 - s)

                    # extrude code from TrumanBlending
                    for a, b in zip(of.loops, nf.loops):
                        sf = bm.faces.new((a.vert, a.link_loop_next.vert,
                                           b.link_loop_next.vert, b.vert))
                        sf.normal_update()
                    bm.faces.remove(of)
                    of = nf

            after.append(of)

        for v in bm.verts:
            v.select = False
        for e in bm.edges:
            e.select = False

        for f in after:
            if f not in sel:
                f.select = True
            else:
                f.select = False

        bm.to_mesh(obj.data)
        obj.data.update()

        # restore user settings
        bpy.ops.object.mode_set(mode=om)

        if not len(sel):
            self.report({"WARNING"},
                        "No suitable Face selection found. Operation cancelled")
            return {'CANCELLED'}

        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == '__main__':
    register()
