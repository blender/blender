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
from bpy.props import BoolProperty, IntProperty, StringProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, changable_sockets,
                                     repeat_last, match_long_repeat)

# ListSlice
# by Linus Yng

# Slices a list using function like:
# Slice  = data[start:stop]
# Other = data[:start]+data[stop:]


class ListSliceNode(bpy.types.Node, SverchCustomTreeNode):
    ''' List Slice '''
    bl_idname = 'ListSliceNode'
    bl_label = 'List Slice'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level = IntProperty(name='level_to_count',
                        default=2, min=0,
                        update=updateNode)
    start = IntProperty(name='Start',
                        default=0,
                        update=updateNode)
    stop = IntProperty(name='Stop',
                       default=1,
                       update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="level")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Data")
        self.inputs.new('StringsSocket', "Start").prop_name = 'start'
        self.inputs.new('StringsSocket', "Stop").prop_name = 'stop'
        self.outputs.new('StringsSocket', "Slice", "Slice")
        self.outputs.new('StringsSocket', "Other", "Other")

    def update(self):
        if 'Data' in self.inputs and self.inputs['Data'].links:
            inputsocketname = 'Data'
            outputsocketname = ['Slice', 'Other']
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        data = self.inputs['Data'].sv_get()
        start = self.inputs['Start'].sv_get()[0]
        stop = self.inputs['Stop'].sv_get()[0]

        if self.outputs['Slice'].is_linked:
            if self.level:
                out = self.get(data, start, stop, self.level, self.slice)
            else:
                out = self.slice(data, start[0], stop[0])
            self.outputs['Slice'].sv_set(out)

        if self.outputs['Other'].is_linked:
            if self.level:
                out = self.get(data, start, stop, self.level, self.other)
            else:
                out = self.other(data, start[0], stop[0])
            self.outputs['Other'].sv_set(out)

    def slice(self, data, start, stop):
        if isinstance(data, (tuple, list)):
            return data[start:stop]
        else:
            return None

    def other(self, data, start, stop):
        if isinstance(data, (tuple, list)):
            return data[:start] + data[stop:]
        else:
            return None

    def get(self, data, start, stop, level, f):
        if level > 1:  # find level to work on
                return [self.get(obj, start, stop, level - 1, f) for obj in data]
        elif level == 1:  # execute the chosen function
            data, start, stop = match_long_repeat([data, start, stop])
            out = []
            for da, art, op in zip(data, start, stop):
                out.append(f(da, art, op))
            return out
        else:  # Fail
            return None


def register():
    bpy.utils.register_class(ListSliceNode)


def unregister():
    bpy.utils.unregister_class(ListSliceNode)
