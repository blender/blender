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
import bmesh
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, match_long_repeat)


class SvBMinputNode(bpy.types.Node, SverchCustomTreeNode):
    ''' BMesh In '''
    bl_idname = 'SvBMinputNode'
    bl_label = 'BMesh in'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        si = self.inputs.new
        si('StringsSocket', 'Objects')
        si('VerticesSocket', 'Vert')
        si('StringsSocket', 'Edge')
        si('StringsSocket', 'Poly')
        self.outputs.new('StringsSocket', 'bmesh_list')

    def process(self):
        Val = []
        siob, v, e, p = self.inputs
        if siob.is_linked:
            obj = siob.sv_get()
            for OB in obj:
                bm = bmesh.new()
                bm.from_mesh(OB.data)
                Val.append(bm)
        if v.is_linked:
            sive, sied, sipo = match_long_repeat([v.sv_get(), e.sv_get([[]]), p.sv_get([[]])])
            for i in zip(sive, sied, sipo):
                bm = bmesh_from_pydata(i[0], i[1], i[2])
                Val.append(bm)
        self.outputs['bmesh_list'].sv_set(Val)

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvBMinputNode)


def unregister():
    bpy.utils.unregister_class(SvBMinputNode)
