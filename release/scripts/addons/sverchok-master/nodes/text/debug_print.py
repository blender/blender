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
from bpy.props import BoolProperty, BoolVectorProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import multi_socket, updateNode


class SvDebugPrintNode(bpy.types.Node, SverchCustomTreeNode):
    ''' print socket data to terminal '''
    bl_idname = 'SvDebugPrintNode'
    bl_label = 'Debug print'
    bl_icon = 'OUTLINER_OB_EMPTY'

    # I wanted to show the bool so you could turn off and on individual sockets
    # but needs changes in node_s, want to think a bit more before adding an index option to
    # stringsockets, for now draw_button_ext
    defaults = [True for i in range(32)]
    print_socket = BoolVectorProperty(name='Print',
                                      default=defaults, size=32,
                                      update=updateNode)
    base_name = 'Data '
    multi_socket_type = 'StringsSocket'
    print_data = BoolProperty(name='Active', description='Turn on/off printing to stdout',
                              default=True,
                              update=updateNode)

    def sv_init(self, context):
        socket = self.inputs.new('StringsSocket', "Data 0")

    def draw_buttons(self, context, layout):
        layout.prop(self, 'print_data')

    def draw_buttons_ext(self, context, layout):
        layout.label(text='Print?')
        for i, socket in enumerate(self.inputs):
            layout.prop(self, "print_socket", index=i, text=socket.name)

    def update(self):
        multi_socket(self, min=1)

    def process(self):
        if not self.print_data:
            return
        for i, socket in enumerate(self.inputs):
            if socket.is_linked and self.print_socket[i]:
                self.info(socket.sv_get(deepcopy=False))

def register():
    bpy.utils.register_class(SvDebugPrintNode)


def unregister():
    bpy.utils.unregister_class(SvDebugPrintNode)

if __name__ == '__main__':
    register()
