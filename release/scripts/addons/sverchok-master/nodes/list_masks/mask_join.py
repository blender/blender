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

from itertools import cycle

import bpy
from bpy.props import BoolProperty, IntProperty, StringProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import changable_sockets, updateNode


class SvMaskJoinNode(bpy.types.Node, SverchCustomTreeNode):
    '''Mask Join'''
    bl_idname = 'SvMaskJoinNode'
    bl_label = 'List Mask Join (in)'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level = IntProperty(name="Level",
                        default=1, min=1,
                        update=updateNode)
    choice = BoolProperty(name="Choice",
                          default=False,
                          update=updateNode)
    typ = StringProperty(name='typ',
                         default='')
    newsock = BoolProperty(name='newsock',
                           default=False)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Mask')
        self.inputs.new('StringsSocket', 'Data True')
        self.inputs.new('StringsSocket', 'Data False')

        self.outputs.new('StringsSocket', 'Data')

    def draw_buttons(self, context, layout):
        layout.prop(self, 'level')
        layout.prop(self, 'choice')

    def update(self):
        if 'Data' not in self.outputs:
            return
        if not self.outputs['Data'].links:
            return
        inputsocketname = 'Data True'
        outputsocketname = ['Data']
        changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if all(s.is_linked for s in self.inputs[1:]):
            if self.inputs['Mask'].is_linked:
                mask = self.inputs['Mask'].sv_get()
            else:  # to match MaskList
                mask = [[1, 0]]
            data_t = self.inputs['Data True'].sv_get()
            data_f = self.inputs['Data False'].sv_get()

            data_out = self.get_level(mask, data_t, data_f, self.level-1)

            self.outputs['Data'].sv_set(data_out)

    def apply_choice_mask(self, mask, data_t, data_f):
        out = []
        for m, t, f in zip(cycle(mask), data_t, data_f):
            if m:
                out.append(t)
            else:
                out.append(f)
        return out

    def apply_mask(self, mask, data_t, data_f):
        ind_t, ind_f = 0, 0
        out = []
        for m in cycle(mask):
            if m:
                if ind_t == len(data_t):
                    return out
                out.append(data_t[ind_t])
                ind_t += 1
            else:
                if ind_f == len(data_f):
                    return out
                out.append(data_f[ind_f])
                ind_f += 1
        return out

    def get_level(self, mask, data_t, data_f, level):
        if level == 1:
            out = []
            param = (mask, data_t, data_f)
            if not all((isinstance(p, (list, tuple)) for p in param)):
                print("Fail")
                return
            max_index = min(map(len, param))
            if self.choice:
                apply_mask = self.apply_choice_mask
            else:
                apply_mask = self.apply_mask

            for i in range(max_index):
                out.append(apply_mask(mask[i], data_t[i], data_f[i]))
            return out
        elif level > 2:
            out = []
            for t, f in zip(data_t, data_f):
                out.append(self.get_level(mask, t, f, level - 1))
            return out
        else:
            return self.apply_mask(mask[0], data_t, data_f)


def register():
    bpy.utils.register_class(SvMaskJoinNode)


def unregister():
    bpy.utils.unregister_class(SvMaskJoinNode)
