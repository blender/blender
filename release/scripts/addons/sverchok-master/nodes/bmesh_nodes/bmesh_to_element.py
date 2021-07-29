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
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode)


class SvBMtoElementNode(bpy.types.Node, SverchCustomTreeNode):
    ''' BMesh Decompose '''
    bl_idname = 'SvBMtoElementNode'
    bl_label = 'BMesh Elements'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'bmesh_list')
        self.outputs.new('StringsSocket', 'BM_verts')
        self.outputs.new('StringsSocket', 'BM_edges')
        self.outputs.new('StringsSocket', 'BM_faces')

    def process(self):
        v, e, p = self.outputs
        vlist = []
        elist = []
        plist = []
        bml = self.inputs['bmesh_list'].sv_get()
        for i in bml:
            vlist.append(i.verts[:])
            elist.append(i.edges[:])
            plist.append(i.faces[:])
        v.sv_set(vlist)
        e.sv_set(elist)
        p.sv_set(plist)

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvBMtoElementNode)


def unregister():
    bpy.utils.unregister_class(SvBMtoElementNode)
