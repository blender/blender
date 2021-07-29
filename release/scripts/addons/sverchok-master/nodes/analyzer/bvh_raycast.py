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
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, match_long_cycle as C)


class SvBVHRaycastNode(bpy.types.Node, SverchCustomTreeNode):
    ''' BVH Tree Raycast '''
    bl_idname = 'SvBVHRaycastNode'
    bl_label = 'bvh_raycast'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        si,so = self.inputs.new,self.outputs.new
        si('StringsSocket', 'BVH_tree_list')
        si('VerticesSocket', 'Start').use_prop=True
        si('VerticesSocket', 'Direction').use_prop=True
        so('VerticesSocket', 'Location')
        so('VerticesSocket', 'Normal')
        so('StringsSocket', 'Index')
        so('StringsSocket', 'Distance')

    def process(self):
        L,N,I,D = self.outputs
        bvhl, st, di = self.inputs
        RL = []
        st,di = C([st.sv_get()[0], di.sv_get()[0]])
        for bvh in bvhl.sv_get():
            RL.append([bvh.ray_cast(i, i2) for i, i2 in zip(st,di)])
        if L.is_linked:
            L.sv_set([[r[0][:] if r[0] != None else (0,0,0) for r in L] for L in RL])
        if N.is_linked:
            N.sv_set([[r[1][:] if r[1] != None else (0,0,0) for r in L] for L in RL])
        if I.is_linked:
            I.sv_set([[r[2] if r[2] != None else -1 for r in L] for L in RL])
        if D.is_linked:
            D.sv_set([[r[3] if r[3] != None else 0 for r in L] for L in RL])

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvBVHRaycastNode)


def unregister():
    bpy.utils.unregister_class(SvBVHRaycastNode)
