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
import numpy as np
from bpy.props import IntProperty, BoolProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, second_as_first_cycle as safc)


class SvIndexToMaskNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Create mask list from index '''
    bl_idname = 'SvIndexToMaskNode'
    bl_label = 'Index To Mask'
    bl_icon = 'OUTLINER_OB_EMPTY'

    ML = IntProperty(name='Mask Length', default=10, min=2, update=updateNode)
    
    def update_mode(self, context):
        self.inputs['mask size'].hide_safe = self.data_to_mask
        self.inputs['data to mask'].hide_safe = not self.data_to_mask
        updateNode(self, context)

    data_to_mask = BoolProperty(name = "data masking",
            description = "Use data to define mask length",
            default = False,
            update=update_mode)
    complex_data = BoolProperty(name = "topo mask",
            description = "data consists of verts or polygons\edges. Otherwise the two vertices will be masked as [[[T, T, T], [F, F, F]]] instead of [[T, F]]",
            default = False,
            update=update_mode)

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        col.prop(self, "data_to_mask", toggle=True)
        if self.data_to_mask:
            col.prop(self, "complex_data", toggle=True)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Index')
        self.inputs.new('StringsSocket', 'mask size').prop_name = "ML"
        self.inputs.new('StringsSocket', 'data to mask').hide_safe = True
        self.outputs.new('StringsSocket', 'mask')

    def process(self):
        Inds, MaSi, Dat = self.inputs
        OM = self.outputs[0]
        if OM.is_linked:
            out = []
            I = Inds.sv_get()
            if not self.data_to_mask:
                for Ind, Size in zip(I, safc(I, MaSi.sv_get()[0])):
                    Ma = np.zeros(Size, dtype= np.bool)
                    Ma[Ind] = 1
                    out.append(Ma.tolist())
            else:
                Ma = np.zeros_like(Dat.sv_get(), dtype= np.bool)
                if not self.complex_data:
                    for m, i in zip(Ma, safc(Ma, I)):
                        m[i] = 1
                        out.append(m.tolist())
                else:
                    for m, i in zip(Ma, safc(Ma, I)):
                        m[i] = 1
                        out.append(m[:, 0].tolist())
            OM.sv_set(out)


def register():
    bpy.utils.register_class(SvIndexToMaskNode)


def unregister():
    bpy.utils.unregister_class(SvIndexToMaskNode)
