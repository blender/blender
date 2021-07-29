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

from copy import copy

import bpy
from bpy.props import BoolProperty, IntProperty, StringProperty
from sverchok.node_tree import SverchCustomTreeNode, StringsSocket
from sverchok.data_structure import updateNode, changable_sockets


class MaskListNode(bpy.types.Node, SverchCustomTreeNode):
    ''' MaskList node '''
    bl_idname = 'MaskListNode'
    bl_label = 'List Mask (out)'
    bl_icon = 'OUTLINER_OB_EMPTY'

    Level = IntProperty(name='Level', description='Choose list level of data (see help)',
                        default=1, min=1, max=10,
                        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "data")
        self.inputs.new('StringsSocket', "mask")

        self.outputs.new('StringsSocket', "mask")
        self.outputs.new('StringsSocket', "ind_true")
        self.outputs.new('StringsSocket', "ind_false")
        self.outputs.new('StringsSocket', 'dataTrue')
        self.outputs.new('StringsSocket', 'dataFalse')

    def draw_buttons(self, context, layout):
        layout.prop(self, "Level", text="Level lists")

    def update(self):
        inputsocketname = 'data'
        outputsocketname = ['dataTrue', 'dataFalse']
        changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        inputs = self.inputs
        outputs = self.outputs

        data = inputs['data'].sv_get()
        mask = inputs['mask'].sv_get(default=[[1, 0]])

        result = self.getMask(data, mask, self.Level)

        if self.outputs['dataTrue'].is_linked:
            outputs['dataTrue'].sv_set(result[0])

        if self.outputs['dataFalse'].is_linked:
            outputs['dataFalse'].sv_set(result[1])

        if self.outputs['mask'].is_linked:
            outputs['mask'].sv_set(result[2])

        if self.outputs['ind_true'].is_linked:
            outputs['ind_true'].sv_set(result[3])

        if self.outputs['ind_false'].is_linked:
            outputs['ind_false'].sv_set(result[4])

    # working horse
    def getMask(self, list_a, mask_l, level):
        list_b = self.getCurrentLevelList(list_a, level)
        res = list_b
        if list_b:
            res = self.putCurrentLevelList(list_a, list_b, mask_l, level)
        return res

    def putCurrentLevelList(self, list_a, list_b, mask_l, level, idx=0):
        result_t = []
        result_f = []
        mask_out = []
        ind_true = []
        ind_false = []
        if level > 1:
            if isinstance(list_a, (list, tuple)):
                for idx, l in enumerate(list_a):
                    l2 = self.putCurrentLevelList(l, list_b, mask_l, level - 1, idx)
                    result_t.append(l2[0])
                    result_f.append(l2[1])
                    mask_out.append(l2[2])
                    ind_true.append(l2[3])
                    ind_false.append(l2[4])
            else:
                print('AHTUNG!!!')
                return list_a
        else:
            indx = min(len(mask_l)-1, idx)
            mask = mask_l[indx]
            mask_0 = copy(mask)
            while len(mask) < len(list_a):
                if len(mask_0) == 0:
                    mask_0 = [1, 0]
                mask = mask+mask_0

            for idx, l in enumerate(list_a):
                tmp = list_b.pop(0)
                if mask[idx]:
                    result_t.append(tmp)
                    ind_true.append(idx)
                else:
                    result_f.append(tmp)
                    ind_false.append(idx)
            mask_out = mask[:len(list_a)]

        return (result_t, result_f, mask_out, ind_true, ind_false)

    def getCurrentLevelList(self, list_a, level):
        list_b = []
        if level > 1:
            if isinstance(list_a, (list, tuple)):
                for l in list_a:
                    l2 = self.getCurrentLevelList(l, level-1)
                    if isinstance(l2, (list, tuple)):
                        list_b.extend(l2)
                    else:
                        return False
            else:
                return False
        else:
            if isinstance(list_a, (list, tuple)):
                return copy(list_a)
            else:
                return list_a
        return list_b


def register():
    bpy.utils.register_class(MaskListNode)


def unregister():
    bpy.utils.unregister_class(MaskListNode)
