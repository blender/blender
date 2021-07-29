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
from bpy.props import IntProperty, StringProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import get_other_socket, updateNode, match_long_repeat


class SvSwitchNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Switch Node '''
    bl_idname = 'SvSwitchNode'
    bl_label = 'Switch'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def draw_buttons(self, context, layout):
        row = layout.row()
        split = row.split(0.6)
        split.row().prop(self, "selected_mode", expand=True)
        if self.selected_mode == 'single':
            layout.prop(self, "switch_count")

    def wrapped_updateNode(self, context):
        self.switch_count = 1
        self.process()
    
    def change_count(self, context):
        in_count = len(self.inputs)
        if in_count < self.switch_count * 2 + 1:
            while len(self.inputs) != self.switch_count * 2 + 1:
                n = int((len(self.inputs)-1) / 2)
                self.inputs.new("StringsSocket", "T {}".format(n))
                self.inputs.move(len(self.inputs)-1, n+1)
                self.inputs.new("StringsSocket", "F {}".format(n))
                self.outputs.new("StringsSocket", "Out {}".format(n))
        else:
            while len(self.inputs) != self.switch_count * 2 + 1:
                n = int((len(self.inputs)-1) / 2)
                self.inputs.remove(self.inputs[-1])
                self.inputs.remove(self.inputs[n])
                self.outputs.remove(self.outputs[-1])
        
    switch_state = IntProperty(name="state", min=0, max=1, default=1, update=updateNode)
    switch_count = IntProperty(name="count", min=1, max=10, default=1, update=change_count)

    mode_options = [(k, k, '', i) for i, k in enumerate(["single", "multi"])]

    selected_mode = bpy.props.EnumProperty(
        description="pick a different processing mode", default="single",
        items=mode_options, update=wrapped_updateNode)
        
    def sv_init(self, context):
        self.inputs.new("StringsSocket", "State").prop_name = 'switch_state'
        self.inputs.new("StringsSocket", "T 0")
        self.inputs.new("StringsSocket", "F 0")
        self.outputs.new("StringsSocket", "Out 0")
        
    def update(self):
        if not len(self.inputs) == self.switch_count*2+1:
            return
        if not len(self.outputs) == self.switch_count:
            return
            
        for i,s in enumerate(self.inputs):
            if s.name.startswith("T"):
                other = get_other_socket(s)
                out_s = self.outputs[i-1]
                if other and not isinstance(out_s, type(other)):
                    name = out_s.name
                    self.outputs.remove(out_s)
                    self.outputs.new(other.bl_idname, name)
                    self.outputs.move(len(self.outputs)-1, i-1) 
                count = i

    
    def process(self):
        if self.selected_mode == 'single':
            # old approach
            state = self.inputs[0].sv_get()[0][0]
            count = self.switch_count
            if state:
                sockets = self.inputs[1:count + 1]
            else:
                sockets = self.inputs[1 + count:]
            '''
            dep_sockets = [get_other_socket(in_s) for in_s,out_s in zip(sockets, self.outputs) if out_s.is_linked]
            ul = make_tree_from_nodes([s.node.name for s in dep_sockets], self.id_data, True)
            do_update(ul, self.id_data.nodes)
            '''
            for in_s,out_s in zip(sockets, self.outputs):
                if out_s.is_linked:
                    data = in_s.sv_get(deepcopy=False)
                    out_s.sv_set(data)
        else:
            # this is an entirely different approach to switching
            state_lists = self.inputs[0].sv_get()
            A = self.inputs[1].sv_get()
            B = self.inputs[2].sv_get()

            # do this only for objects...
            if len(A) > 0 and len(B) > 0:
                if isinstance(A[0], (list, tuple)) and len(A[0]) > 0 and isinstance(A[0][0], bpy.types.Object):
                    pass
                elif isinstance(A[0], bpy.types.Object):
                    A = [A]
                    B = [B]

            data = []
            for params in zip(*match_long_repeat([state_lists, A, B])):
                data.append([[a, b][bool(state)] for state, a, b in zip(*match_long_repeat(params))])
            self.outputs[0].sv_set(data)

        
def register():
    bpy.utils.register_class(SvSwitchNode)

def unregister():
    bpy.utils.unregister_class(SvSwitchNode)
