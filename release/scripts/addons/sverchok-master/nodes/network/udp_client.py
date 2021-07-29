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

import socket

import bpy
from bpy.props import IntProperty, FloatProperty, EnumProperty, StringProperty, BoolProperty
from sverchok.node_tree import SverchCustomTreeNode, StringsSocket
from sverchok.utils.profile import profile
from sverchok.data_structure import updateNode


class UdpClientNode(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'UdpClientNode'
    bl_label = 'UDP Client'


    def send_msg(self, context):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setblocking(0)
        sock.sendto(bytes(self.send, 'UTF-8'), (self.ip, self.port))


    def recv_msg(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setblocking(0)
        sock.settimeout(self.timeout)
        try:
            data, _ = sock.recvfrom(self.buffer_size)
            self.receive = data.decode('UTF-8')
        except socket.timeout:
            print('Timeout')


    send = StringProperty(name='send',
                          description='Message to send',
                          default='message',
                          update=send_msg)

    receive = StringProperty(name='receive',
                             description='Received message',
                             default='',
                             update=updateNode)

    ip = StringProperty(name='ip',
                        description='IP address of server',
                        default='127.0.0.1')

    port = IntProperty(name='port',
                       description='Port number to send message',
                       default=9250)

    buffer_size = IntProperty(name='buffer_size',
                              description='Size of buffer',
                              default=8192)

    timeout = FloatProperty(name='timeout',
                            description='Timeout (sec)',
                            default=0.5)

    active = BoolProperty(default=False, name='Active')

    def draw_buttons(self, context, layout):
        layout.prop(self, 'active', text='Active')
        layout.prop(self, 'ip', text='IP')
        layout.prop(self, 'port', text='Port')
        layout.prop(self, 'buffer_size', text='Buffer')
        layout.prop(self, 'timeout', text='Timeout')

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'send', 'send').prop_name = 'send'
        self.outputs.new('StringsSocket', 'receive', 'receive')

    @profile
    def process(self):
        if not self.active:
            return

        print(type(self.send),type(self.ip),type(self.port))
        input_value = self.inputs[0].sv_get()
        if self.send != str(input_value):
            self.send = str(input_value)
        #self.send_msg(bpy.context)

        if self.outputs['receive'].is_linked:
            self.recv_msg()
            self.outputs['receive'].sv_set(self.receive)


def register():
    bpy.utils.register_class(UdpClientNode)


def unregister():
    bpy.utils.unregister_class(UdpClientNode)

if __name__ == '__main__':
    register()