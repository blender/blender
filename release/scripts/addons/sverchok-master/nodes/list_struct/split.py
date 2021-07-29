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

from itertools import chain

import bpy
from bpy.props import BoolProperty, IntProperty, StringProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (changable_sockets, repeat_last, updateNode)

# ListSplit
# by Linus Yng
def split(data, size):
    size = max(1, int(size))
    return [data[i:i+size] for i in range(0, len(data), size)]


class SvListSplitNode(bpy.types.Node, SverchCustomTreeNode):
    ''' List Split '''
    bl_idname = 'SvListSplitNode'
    bl_label = 'List Split'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def change_mode(self, context):
        if self.unwrap:
            self.level_unwrap = max(1, self.level)
        else:
            self.level = self.level_unwrap
        updateNode(self, context)

    level = IntProperty(name='Level',
                        default=1, min=0,
                        update=updateNode)
    level_unwrap = IntProperty(name='Level',
                               default=1, min=1,
                               update=updateNode)
    split = IntProperty(name='Split size',
                        default=1, min=1,
                        update=updateNode)
    unwrap = BoolProperty(name='Unwrap',
                          default=True,
                          update=change_mode)

    typ = StringProperty(name='typ',
                         default='')
    newsock = BoolProperty(name='newsock',
                           default=False)

    def draw_buttons(self, context, layout):
        if self.unwrap:
            layout.prop(self, "level_unwrap", text="level")
        else:
            layout.prop(self, "level", text="level")
        layout.prop(self, "unwrap")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Data")
        self.inputs.new('StringsSocket', "Split").prop_name = 'split'
        self.outputs.new('StringsSocket', "Split")

    def update(self):
        if 'Data' in self.inputs and self.inputs['Data'].links:
            inputsocketname = 'Data'
            outputsocketname = ['Split']
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if 'Split' in self.outputs and self.outputs['Split'].is_linked:
            if 'Data' in self.inputs and self.inputs['Data'].is_linked:
                data = self.inputs['Data'].sv_get()
                sizes = self.inputs['Split'].sv_get()[0]
                if self.unwrap:
                    out = self.get(data, self.level_unwrap, sizes)
                elif self.level:
                    out = self.get(data, self.level, sizes)
                else:
                    out = split(data, sizes[0])
                self.outputs['Split'].sv_set(out)

    def get(self, data, level, size):
        if not isinstance(data, (list, tuple)):
            return data
        if not isinstance(data[0], (list, tuple)):
            return data
        if level > 1:  # find level to work on
            return [self.get(d, level - 1, size) for d in data]
        elif level == 1:  # execute the chosen function
            sizes = repeat_last(size)
            if self.unwrap:
                return list(chain.from_iterable((split(d, next(sizes)) for d in data)))
            else:
                return [split(d, next(sizes)) for d in data]
        else:  # Fail
            return None



def register():
    bpy.utils.register_class(SvListSplitNode)


def unregister():
    bpy.utils.unregister_class(SvListSplitNode)
