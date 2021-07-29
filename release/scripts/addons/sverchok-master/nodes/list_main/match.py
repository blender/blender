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
from bpy.props import IntProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (match_short, match_long_cycle, updateNode,
                                     match_long_repeat, match_cross2)

#
# List Match Node by Linus Yng
#


class ListMatchNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Stream Matching node '''
    bl_idname = 'ListMatchNode'
    bl_label = 'List Match'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level = IntProperty(name='level', description='Choose level of data (see help)',
                        default=1, min=1,
                        update=updateNode)

    modes = [("SHORT", "Short", "Shortest List",    1),
             ("CYCLE",   "Cycle", "Longest List",   2),
             ("REPEAT",   "Repeat", "Longest List", 3),
             ("XREF",   "X-Ref", "Cross reference", 4)]

    mode = EnumProperty(default='REPEAT', items=modes,
                        update=updateNode)
    mode_final = EnumProperty(default='REPEAT', items=modes,
                              update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Data 0', 'Data 0')
        self.inputs.new('StringsSocket', 'Data 1', 'Data 1')
        self.outputs.new('StringsSocket', 'Data 0', 'Data 0')
        self.outputs.new('StringsSocket', 'Data 1', 'Data 1')

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="Level")
        layout.label("Recurse/Final")
        layout.prop(self, "mode", expand=True)
        layout.prop(self, "mode_final", expand=True)

# recursive update of match function, now performs match function until depth
# works for short&long and simple scenarios. respect sub lists
# matches until the chosen level
# f2 is applied to the final level of matching,
# f1 is applied to every level until the final, where f2 is used.

    def match(self, lsts, level, f1, f2):
        level -= 1
        if level:
            tmp = [self.match(obj, level, f1, f2) for obj in zip(*f1(lsts))]
            return list(map(list, zip(*tmp)))
        elif type(lsts) == list:
            return f2(lsts)
        elif type(lsts) == tuple:
            return tuple(f2(list(lsts)))
        return None

    def update(self):
        # inputs
        # these functions are in util.py

        # socket handling
        if self.inputs[-1].links:
            name = 'Data '+str(len(self.inputs))
            self.inputs.new('StringsSocket', name, name)
            self.outputs.new('StringsSocket', name, name)
        else:
            while len(self.inputs) > 2 and not self.inputs[-2].links:
                self.inputs.remove(self.inputs[-1])
                self.outputs.remove(self.outputs[-1])
        # check number of connections and type match input socket n with output socket n
        count_inputs = 0
        count_outputs = 0
        for idx, socket in enumerate(self.inputs):
            if socket.name in self.outputs and self.outputs[socket.name].links:
                count_outputs += 1
            if socket.links:
                count_inputs += 1
                if type(socket.links[0].from_socket) != type(self.outputs[socket.name]):
                    self.outputs.remove(self.outputs[socket.name])
                    self.outputs.new(socket.links[0].from_socket.bl_idname, socket.name, socket.name)
                    self.outputs.move(len(self.outputs)-1, idx)

    def process(self):
        # check inputs and that there is at least one output
        func_dict = {
            'SHORT': match_short,
            'CYCLE': match_long_cycle,
            'REPEAT': match_long_repeat,
            'XREF': match_cross2
            }
        count_inputs = sum(s.is_linked for s in self.inputs)
        count_outputs = sum(s.is_linked for s in self.outputs)
        if count_inputs == len(self.inputs)-1 and count_outputs:
            out = []
            lsts = []
            # get data
            for socket in self.inputs:
                if socket.is_linked:
                    lsts.append(socket.sv_get())

            out = self.match(lsts, self.level, func_dict[self.mode], func_dict[self.mode_final])

            # output into linked sockets s
            for i, socket in enumerate(self.outputs):
                if i == len(out):  # never write to last socket
                    break
                if socket.is_linked:
                    socket.sv_set(out[i])


def register():
    bpy.utils.register_class(ListMatchNode)


def unregister():
    bpy.utils.unregister_class(ListMatchNode)
