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

from ast import literal_eval

import bpy
from bpy.props import BoolProperty, StringProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (changable_sockets, updateNode)

from sverchok.utils.listutils import preobrazovatel


class ListLevelsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Lists Levels node '''
    bl_idname = 'ListLevelsNode'
    bl_label = 'List Del Levels'
    bl_icon = 'OUTLINER_OB_EMPTY'

    Sverch_LisLev = StringProperty(name='Sverch_LisLev',
                                   description='User defined nesty levels. (i.e. 1,2)',
                                   default='1,2,3',
                                   update=updateNode)
    typ = StringProperty(name='typ',
                         default='')
    newsock = BoolProperty(name='newsock',
                           default=False)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'data', 'data')
        self.outputs.new('StringsSocket', 'data', 'data')

    def draw_buttons(self, context, layout):
        layout.prop(self, "Sverch_LisLev", text="List levels")

    def update(self):
        if 'data' in self.inputs and len(self.inputs['data'].links) > 0:
            inputsocketname = 'data'
            outputsocketname = ['data', ]
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if self.outputs['data'].is_linked:
            data = self.inputs['data'].sv_get()
            userlevelb = literal_eval('['+self.Sverch_LisLev+']')
            self.outputs['data'].sv_set(preobrazovatel(data, userlevelb))


def register():
    bpy.utils.register_class(ListLevelsNode)


def unregister():
    bpy.utils.unregister_class(ListLevelsNode)
