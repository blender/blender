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
from sverchok.data_structure import (updateNode)


class SvBVHnearNode(bpy.types.Node, SverchCustomTreeNode):
    ''' BVH Find Nearest '''
    bl_idname = 'SvBVHnearNode'
    bl_label = 'bvh_nearest'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'BVH_tree_list')
        self.inputs.new('VerticesSocket', 'Points').use_prop=True
        self.outputs.new('VerticesSocket', 'Location')
        self.outputs.new('VerticesSocket', 'Normal')
        self.outputs.new('StringsSocket', 'Index')
        self.outputs.new('StringsSocket', 'Distance')

    def process(self):
        outFin = []
        bvhl, p = self.inputs
        oL,oN,oI,oD = self.outputs
        for BV in bvhl.sv_get():
            outFin.append([BV.find(i) for i in p.sv_get()[0]])
        if oL.is_linked:
            oL.sv_set([[i[0][:] for i in o] for o in outFin])
        if oN.is_linked:
            oN.sv_set([[i[1][:] for i in o] for o in outFin])
        if oI.is_linked:
            oI.sv_set([[i[2] for i in o] for o in outFin])
        if oD.is_linked:
            oD.sv_set([[i[3] for i in o] for o in outFin])

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvBVHnearNode)


def unregister():
    bpy.utils.unregister_class(SvBVHnearNode)
