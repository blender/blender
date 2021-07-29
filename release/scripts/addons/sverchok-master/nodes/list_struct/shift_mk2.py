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
import numpy as np
from collections import deque

import bpy
from bpy.props import BoolProperty, IntProperty, StringProperty

from sverchok.node_tree import SverchCustomTreeNode, StringsSocket
from sverchok.data_structure import (updateNode, changable_sockets)


class ShiftNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' ls - shift list elements '''
    bl_idname = 'ShiftNodeMK2'
    bl_label = 'List Shift'
    bl_icon = 'OUTLINER_OB_EMPTY'

    shift_c = IntProperty(name='Shift', default=0, update=updateNode)
    enclose = BoolProperty(name='check_tail', default=True, update=updateNode)
    level = IntProperty(name='level', default=0, min=0, update=updateNode)

    mode_options = [(k, k, '', i) for i, k in enumerate(["np", "py"])]
    
    selected_mode = bpy.props.EnumProperty(
        items=mode_options, default="np", update=updateNode,
        description="np is numpy, py is handwritten shifting"
    )

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="level")
        layout.prop(self, "selected_mode", expand=True)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "data", "data")
        self.inputs.new('StringsSocket', "shift", "shift").prop_name = 'shift_c'
        self.outputs.new('StringsSocket', 'data', 'data')

    def update(self):
        if 'data' in self.inputs and self.inputs['data'].is_linked:
            inputsocketname = 'data'
            outputsocketname = ['data']
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if not self.outputs["data"].is_linked:
            return

        data = self.inputs['data'].sv_get()
        number = self.inputs["shift"].sv_get()[0][0]

        if self.selected_mode == 'np':
            dat = np.array(data)
            # levelsOfList replacement:
            depth = dat.ndim #len(np.shape(dat))-1
            # roll with enclose (we need case of declose and vectorization)
            output = np.roll(dat, number, axis=min(self.level, depth)).tolist()

        elif self.selected_mode == 'py':
            output = []

            if self.level == 0:
                d = deque(data)
                d.rotate(number)
                output = list(d)

            elif self.level == 1:
                for sublist in data:
                    d = deque(sublist)
                    d.rotate(number)
                    output.append(list(d))

            elif self.level > 1:
                # likely vectors or polygons... so going with list .
                for sublist in data:
                    sub_output = []
                    for subsublist in sublist:
                        d = deque(subsublist)
                        d.rotate(number)
                        sub_output.append(list(d))
                    output.append(sub_output)


        self.outputs['data'].sv_set(output)


def register():
    bpy.utils.register_class(ShiftNodeMK2)


def unregister():
    bpy.utils.unregister_class(ShiftNodeMK2)

#if __name__ == '__main__':
#    register()
