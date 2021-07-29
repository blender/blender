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
from bpy.props import BoolProperty, IntProperty, StringProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, changable_sockets,
                                     levelsOflist)


class ListFLNode(bpy.types.Node, SverchCustomTreeNode):
    ''' List First and last item of list '''
    bl_idname = 'ListFLNode'
    bl_label = 'List First & Last'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level = IntProperty(name='level_to_count',
                        default=2, min=0,
                        update=updateNode)
    typ = StringProperty(name='typ',
                         default='')
    newsock = BoolProperty(name='newsock',
                           default=False)

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="level")

    def sv_init(self, context):

        self.inputs.new('StringsSocket', "Data", "Data")
        self.outputs.new('StringsSocket', "Middl", "Middl")
        self.outputs.new('StringsSocket', "First", "First")
        self.outputs.new('StringsSocket', "Last", "Last")

    def update(self):
        if self.inputs['Data'].links:
            # адаптивный сокет
            inputsocketname = 'Data'
            outputsocketname = ["Middl",'First', 'Last']
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if 'First' in self.outputs and self.outputs['First'].is_linked or \
                'Last' in self.outputs and self.outputs['Last'].is_linked or \
                'Middl' in self.outputs and self.outputs['Middl'].is_linked:
            data = self.inputs['Data'].sv_get(deepcopy=False)

            # blocking too height values of levels, reduce
            levels = levelsOflist(data)-2
            if levels >= self.level:
                levels = self.level-1
            elif levels < 1:
                levels = 1
            # assign out
            if self.outputs['First'].is_linked:
                out = self.count(data, levels, 0)
                self.outputs['First'].sv_set(out)
            if self.outputs['Middl'].is_linked:
                out = self.count(data, levels, 1)
                self.outputs['Middl'].sv_set(out)
            if self.outputs['Last'].is_linked:
                out = self.count(data, levels, 2)
                self.outputs['Last'].sv_set(out)

    def count(self, data, level, mode):
        out = []
        if level:
            for obj in data:
                out.append(self.count(obj, level-1, mode))
        elif type(data) in [tuple, list]:
            if mode == 0:
                out.append(data[0])
            elif mode == 1 and len(data) >= 3:
                out.extend(data[1:-1])
            elif mode == 2:
                out.append(data[-1])
            else:
                out = [[0]]
        return out


def register():
    bpy.utils.register_class(ListFLNode)


def unregister():
    bpy.utils.unregister_class(ListFLNode)
