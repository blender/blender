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
import mathutils
from bpy.props import FloatProperty, EnumProperty, IntProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, match_long_repeat as mlr)


class SvKDTreeNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' KDT Closest Verts MK2 '''
    bl_idname = 'SvKDTreeNodeMK2'
    bl_label = 'KDT Closest Verts MK2'
    bl_icon = 'OUTLINER_OB_EMPTY'

    modes = [
        ('find_n', 'find_n', 'find certain number of closest tree vectors', '', 0),
        ('find_range', 'find_range', 'find closest tree vectors in range', '', 1),
    ]

    number = IntProperty(
        min=1, default=1, name='Number',
        description="find this amount", update=updateNode)

    radius = FloatProperty(
        min=0, default=1, name='Radius',
        description="search in this radius", update=updateNode)

    def update_mode(self, context):
        self.inputs['number'].hide_safe = self.mode == "find_range"
        self.inputs['radius'].hide_safe = self.mode == "find_n"
        updateNode(self, context)

    mode = EnumProperty(
        items=modes, description="mathutils kdtree metods",
        default="find_n", update=update_mode)

    def draw_buttons(self, context, layout):
        row = layout.row()
        row.prop(self, 'mode', expand=True)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'insert')
        self.inputs.new('VerticesSocket', 'find').use_prop = True
        self.inputs.new('StringsSocket', 'number').prop_name = "number"
        self.inputs.new('StringsSocket', 'radius').prop_name = "radius"
        self.inputs['radius'].hide_safe = True
        self.outputs.new('VerticesSocket', 'Co')
        self.outputs.new('StringsSocket', 'index')
        self.outputs.new('StringsSocket', 'distance')

    def process(self):
        V1, V2, N, R = [i.sv_get() for i in self.inputs]
        out = []
        Co, ind, dist = self.outputs
        find_n = self.mode == "find_n"
        for v, v2, k in zip(V1, V2, (N if find_n else R)):
            kd = mathutils.kdtree.KDTree(len(v))
            for idx, co in enumerate(v):
                kd.insert(co, idx)
            kd.balance()
            if find_n:
                out.extend([kd.find_n(vert, num) for vert, num in zip(*mlr([v2, k]))])
            else:
                out.extend([kd.find_range(vert, dist) for vert, dist in zip(*mlr([v2, k]))])
        if Co.is_linked:
            Co.sv_set([[i[0][:] for i in i2] for i2 in out])
        if ind.is_linked:
            ind.sv_set([[i[1] for i in i2] for i2 in out])
        if dist.is_linked:
            dist.sv_set([[i[2] for i in i2] for i2 in out])


def register():
    bpy.utils.register_class(SvKDTreeNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvKDTreeNodeMK2)
