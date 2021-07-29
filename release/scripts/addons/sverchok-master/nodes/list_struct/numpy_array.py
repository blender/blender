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
from bpy.props import EnumProperty,StringProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, enum_item as e)


class SvNumpyArrayNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Numpy Props '''
    bl_idname = 'SvNumpyArrayNode'
    bl_label = 'Numpy Array'
    bl_icon = 'OUTLINER_OB_EMPTY'

    Modes = ['x.tolist()','x.conj()','x.flatten()','np.add(x,y)','np.subtract(x,y)','x.resize()',
             'x.transpose()','np.trunc(x)','x.squeeze()','np.ones_like(x)','np.minimum(x,y)',
             'x.round()','np.maximum(x,y)','np.sin(x)','x.ptp()','x.all()','x.any()','np.remainder(x,y)',
             'np.unique(x)','x.sum()','x.cumsum()','x.mean()','x.var()','x.std()','x.prod()',
             'x.cumprod()','np.array(x)','np.array_equal(x,y)','np.invert(x)','np.rot90(x,1)',
             'x[y]','x+y','x*y','Custom']
    Mod = EnumProperty(name="getmodes", default="np.array(x)", items=e(Modes), update=updateNode)
    Cust = StringProperty(default='x[y.argsort()]', update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'x')
        self.inputs.new('StringsSocket', 'y')
        self.outputs.new('StringsSocket', 'Value')

    def draw_buttons(self, context, layout):
        layout.prop(self, "Mod", "Get")
        if self.Mod == 'Custom':
            layout.prop(self, "Cust", text="")

    def process(self):
        out = self.outputs[0]
        if out.is_linked:
            X,Y = self.inputs
            Mod = self.Mod
            string = self.Cust if Mod == 'Custom' else Mod
            if X.is_linked and Y.is_linked:
                out.sv_set(eval("["+string+" for x,y in zip(X.sv_get(),Y.sv_get())]"))
            elif X.is_linked:
                out.sv_set(eval("["+string+" for x in X.sv_get()]"))
            else:
                out.sv_set([eval(string)])

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvNumpyArrayNode)


def unregister():
    bpy.utils.unregister_class(SvNumpyArrayNode)
