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
from bpy.props import StringProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode, SverchCustomTree
from sverchok.data_structure import changable_sockets, SvGetSocketAnyType, SvSetSocketAnyType


class SvReRouteNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Reroute '''
    bl_idname = 'SvReRouteNode'
    bl_label = 'Reroute'
    bl_icon = 'OUTLINER_OB_EMPTY'
    
    typ = StringProperty(name='typ', default='')
    newsock = BoolProperty(name='newsock', default=False)

    def sv_init(self, context):
        self.hide = True
        self.inputs.new("StringsSocket", "In")
        self.outputs.new("StringsSocket", "Out")
        
    def update(self):

        if not 'Out' in self.outputs:
            return
        if self.inputs and self.inputs[0].links:
            in_socket = 'In'
            out_socket = ['Out']
            changable_sockets(self, in_socket, out_socket)
    
    def process(self):
        if self.outputs[0].links:
            data = SvGetSocketAnyType(self, self.inputs[0], deepcopy=False)
            SvSetSocketAnyType(self, 'Out', data)
        

def register():
    bpy.utils.register_class(SvReRouteNode)


def unregister():
    bpy.utils.unregister_class(SvReRouteNode)

if __name__ == '__main__':
    register()
