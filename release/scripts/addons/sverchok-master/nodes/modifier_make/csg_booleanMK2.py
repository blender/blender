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

import bpy
import sys
from bpy.props import EnumProperty, BoolProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_cycle as mlr
from sverchok.utils.csg_core import CSG


def Boolean(VA, PA, VB, PB, operation):
    a = CSG.Obj_from_pydata(VA, PA)
    b = CSG.Obj_from_pydata(VB, PB)
    faces = []
    vertices = []
    if operation == 'DIFF':
        polygons = a.subtract(b).toPolygons()
    elif operation == 'JOIN':
        polygons = a.union(b).toPolygons()
    elif operation == 'ITX':
        polygons = a.intersect(b).toPolygons()
    for polygon in polygons:
        indices = []
        for v in polygon.vertices:
            pos = [v.pos.x, v.pos.y, v.pos.z]
            if pos not in vertices:
                vertices.append(pos)
            index = vertices.index(pos)
            indices.append(index)
        faces.append(indices)
    return [vertices, faces]


class SvCSGBooleanNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    '''CSG Boolean Node MK2'''
    bl_idname = 'SvCSGBooleanNodeMK2'
    bl_label = 'CSG Boolean 2'
    bl_icon = 'OUTLINER_OB_EMPTY'

    mode_options = [
        ("ITX", "Intersect", "", 0),
        ("JOIN", "Join", "", 1),
        ("DIFF", "Diff", "", 2)
    ]

    selected_mode = EnumProperty(
        items=mode_options,
        description="offers basic booleans using CSG",
        default="ITX",
        update=updateNode)

    def update_mode(self, context):
        self.inputs['Verts A'].hide_safe = self.nest_objs
        self.inputs['Polys A'].hide_safe = self.nest_objs
        self.inputs['Verts B'].hide_safe = self.nest_objs
        self.inputs['Polys B'].hide_safe = self.nest_objs
        self.inputs['Verts Nested'].hide_safe = not self.nest_objs
        self.inputs['Polys Nested'].hide_safe = not self.nest_objs
        updateNode(self, context)

    nest_objs = BoolProperty(name="accumulate nested",
                             description="bool first two objs, then applies rest to result one by one",
                             default=False,
                             update=update_mode)

    out_last = BoolProperty(name="only final result",
                            description="output only last iteration result",
                            default=True,
                            update=update_mode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Verts A')
        self.inputs.new('StringsSocket',  'Polys A')
        self.inputs.new('VerticesSocket', 'Verts B')
        self.inputs.new('StringsSocket',  'Polys B')
        self.inputs.new('VerticesSocket', 'Verts Nested').hide_safe = True
        self.inputs.new('StringsSocket',  'Polys Nested').hide_safe = True
        self.outputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.outputs.new('StringsSocket', 'Polygons', 'Polygons')

    def draw_buttons(self, context, layout):
        row = layout.row()
        row.prop(self, 'selected_mode', expand=True)
        col = layout.column(align=True)
        col.prop(self, "nest_objs", toggle=True)
        if self.nest_objs:
            col.prop(self, "out_last", toggle=True)

    def process(self):
        OutV, OutP = self.outputs
        if not OutV.is_linked:
            return
        VertA, PolA, VertB, PolB, VertN, PolN = self.inputs
        SMode = self.selected_mode
        out = []
        recursionlimit = sys.getrecursionlimit()
        sys.setrecursionlimit(10000)
        if not self.nest_objs:
            for v1, p1, v2, p2 in zip(*mlr([VertA.sv_get(), PolA.sv_get(), VertB.sv_get(), PolB.sv_get()])):
                out.append(Boolean(v1, p1, v2, p2, SMode))
        else:
            vnest, pnest = VertN.sv_get(), PolN.sv_get()
            First = Boolean(vnest[0], pnest[0], vnest[1], pnest[1], SMode)
            if not self.out_last:
                out.append(First)
                for i in range(2, len(vnest)):
                    out.append(Boolean(First[0], First[1], vnest[i], pnest[i], SMode))
                    First = out[-1]
            else:
                for i in range(2, len(vnest)):
                    First = Boolean(First[0], First[1], vnest[i], pnest[i], SMode)
                out.append(First)
        sys.setrecursionlimit(recursionlimit)
        OutV.sv_set([i[0] for i in out])
        if OutP.is_linked:
            OutP.sv_set([i[1] for i in out])


def register():
    bpy.utils.register_class(SvCSGBooleanNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvCSGBooleanNodeMK2)
