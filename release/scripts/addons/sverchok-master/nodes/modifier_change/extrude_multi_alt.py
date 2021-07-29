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


import random
from random import gauss
from math import radians

import bpy
import bmesh
from bpy.types import Operator
from mathutils import Euler, Vector
from bpy.props import FloatProperty, IntProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh


sv_info = {
    "author": "liero, Jimmy Hazevoet",
    "converted_to_sverchok": "zeffii 2017",
    "original": "mesh_extra_tools/mesh_mextrude_plus.py"
}



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


class SvMExtrudeProps():

    off = FloatProperty(
        soft_min=0.001, soft_max=10, min=-100, max=100, default=1.0,
        name="Offset", description="Translation", update=updateNode)

    offx = FloatProperty(
        soft_min=-10.0, soft_max=10.0, min=-100.0, max=100.0, default=0.0,
        name="Loc X", description="Global Translation X", update=updateNode)

    offy = FloatProperty(
        soft_min=-10.0, soft_max=10.0, min=-100.0, max=100.0, default=0.0,
        name="Loc Y", description="Global Translation Y", update=updateNode)

    offz = FloatProperty(
        soft_min=-10.0, soft_max=10.0, min=-100.0, max=100.0, default=0.0,
        name="Loc Z", description="Global Translation Z", update=updateNode)

    rotx = FloatProperty(
        min=-85, max=85, soft_min=-30, soft_max=30, default=0,
        name="Rot X", description="X Rotation", update=updateNode)

    roty = FloatProperty(
        min=-85, max=85, soft_min=-30, soft_max=30, default=0,
        name="Rot Y", description="Y Rotation", update=updateNode)

    rotz = FloatProperty(
        min=-85, max=85, soft_min=-30, soft_max=30, default=-0,
        name="Rot Z", description="Z Rotation", update=updateNode)

    nrotx = FloatProperty(
        min=-85, max=85, soft_min=-30, soft_max=30, default=0,
        name="N Rot X", description="Normal X Rotation", update=updateNode)

    nroty = FloatProperty(
        min=-85, max=85, soft_min=-30, soft_max=30, default=0,     
        name="N Rot Y", description="Normal Y Rotation", update=updateNode)

    nrotz = FloatProperty(
        min=-85, max=85, soft_min=-30, soft_max=30, default=-0,
        name="N Rot Z", description="Normal Z Rotation", update=updateNode)

    sca = FloatProperty(
        min=0.01, max=10, soft_min=0.5, soft_max=1.5, default=1.0,
        name="Scale", description="Scaling of the selected faces after extrusion", update=updateNode)

    var1 = FloatProperty(
        soft_min=-1, soft_max=1, default=0, min=-10, max=10,
        name="Offset Var", description="Offset variation", update=updateNode)

    var2 = FloatProperty(
        min=-10, max=10, soft_min=-1, soft_max=1, default=0,
        name="Rotation Var", description="Rotation variation", update=updateNode)

    var3 = FloatProperty(
        min=-10, max=10, soft_min=-1, soft_max=1, default=0,
        name="Scale Noise", description="Scaling noise", update=updateNode)

    var4 = IntProperty(
        min=0, max=100, default=100,
        name="Probability", description="Probability, chance of extruding a face", update=updateNode)

    num = IntProperty(
        min=1, max=500, soft_max=100, default=5,
        name="Repeat", description="Repetitions", update=updateNode)

    ran = IntProperty(
        min=-9999, max=9999, default=0,     
        name="Seed", description="Seed to feed random values", update=updateNode)

    opt1 = BoolProperty(
        default=True, name="Polygon coordinates",
        description="Polygon coordinates, Object coordinates", update=updateNode)

    opt2 = BoolProperty(
        default=False, name="Proportional offset",
        description="Scale * Offset", update=updateNode)

    opt3 = BoolProperty(
        default=False, name="Per step rotation noise",
        description="Per step rotation noise, Initial rotation noise", update=updateNode)

    opt4 = BoolProperty(
        default=False, name="Per step scale noise",
        description="Per step scale noise, Initial scale noise", update=updateNode)


def draw_ui(self, context, layout):

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


def perform_mextrude(self, bm, sel):
    after = []
    origin = Vector([0.0, 0.0, 0.0])

    # faces loop
    for i, of in enumerate(sel):
        nro = nrot(self, of.normal)
        off = vloc(self, i)
        loc = gloc(self, i)
        of.normal_update()

        # initial rotation noise
        if not self.opt3:
            rot = vrot(self, i)
        # initial scale noise
        if not self.opt4:
            s = vsca(self, i)

        # extrusion loop
        for r in range(self.num):
            # random probability % for extrusions
            if self.var4 > int(random.random() * 100):
                nf = of.copy()
                nf.normal_update()
                no = nf.normal.copy()

                # face/obj coÃ¶rdinates
                if self.opt1:
                    ce = nf.calc_center_bounds()
                else:
                    ce = origin

                # per step rotation noise
                if self.opt3:
                    rot = vrot(self, i + r)
                # per step scale noise
                if self.opt4:
                    s = vsca(self, i + r)

                # proportional, scale * offset
                if self.opt2:
                    off = s * off

                for v in nf.verts:
                    v.co -= ce
                    v.co.rotate(nro)
                    v.co.rotate(rot)
                    v.co += ce + loc + no * off
                    v.co = v.co.lerp(ce, 1 - s)

                # extrude code from TrumanBlending
                for a, b in zip(of.loops, nf.loops):
                    sf = bm.faces.new((a.vert, a.link_loop_next.vert, b.link_loop_next.vert, b.vert))
                    sf.normal_update()
                bm.faces.remove(of)
                of = nf
            bm.verts.index_update()
            bm.faces.index_update()

        after.append(of)

   # for v in bm.verts:
   #     v.select = False
   # for e in bm.edges:
   #     e.select = False
   # for f in after:
   #     f.select = f not in sel

    out_verts, _, out_faces = pydata_from_bmesh(bm)
    del bm
    return (out_verts, out_faces) or None



class SvMultiExtrudeAlt(bpy.types.Node, SverchCustomTreeNode, SvMExtrudeProps):
    ''' a SvMultiExtrudeAlt f '''
    bl_idname = 'SvMultiExtrudeAlt'
    bl_label = 'MultiExtrude Alt from addons'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'verts')
        self.inputs.new('StringsSocket', 'faces')
        self.inputs.new('StringsSocket', 'face_masks')
        self.outputs.new('VerticesSocket', 'verts')
        self.outputs.new('StringsSocket', 'faces')

    def draw_buttons(self, context, layout):
        draw_ui(self, context, layout)

    def process(self):

        # bmesh operations
        verts = self.inputs['verts'].sv_get()
        faces = self.inputs['faces'].sv_get()
        face_masks = self.inputs['face_masks'].sv_get()
        out_verts, out_faces = [], []

        for _verts, _faces, _face_mask in zip(verts, faces, face_masks):

            bm = bmesh_from_pydata(_verts, [], _faces, normal_update=True)

            sel = []
            add_sell = sel.append
            for f in (f for f in bm.faces if f.index in set(_face_mask)):
                f.select = True
                add_sell(f)

            generated_data = perform_mextrude(self, bm, sel)
            if generated_data:
                outv, outf = generated_data
                out_verts.append(outv)
                out_faces.append(outf)

        self.outputs['verts'].sv_set(out_verts)
        self.outputs['faces'].sv_set(out_faces)


def register():
    bpy.utils.register_class(SvMultiExtrudeAlt)


def unregister():
    bpy.utils.unregister_class(SvMultiExtrudeAlt)
