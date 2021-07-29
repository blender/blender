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

from math import exp

import bpy
from bpy.props import IntProperty, FloatProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat

def rescale(seq, maxValue):
    if maxValue:
        actualMax = max(map(abs, seq))
        if actualMax == 0.0:
            return result
        return [x*maxValue/actualMax for x in seq]
    else:
        return seq

def exponential_e(x0, alpha, nmin, nmax, maxValue):
    result = []
    for n in list(range(int(nmin), int(nmax))) + [nmax]:
        x = x0 * exp(alpha * n)
        result.append(x)

    return rescale(result, maxValue)

def exponential_b(x0, base, nmin, nmax, maxValue):
    result = []
    for n in list(range(int(nmin), int(nmax))) + [nmax]:
        x = x0 * base ** n
        result.append(x)

    return rescale(result, maxValue)

class SvGenExponential(bpy.types.Node, SverchCustomTreeNode):
    ''' Generate exponential sequence '''
    bl_idname = 'SvGenExponential'
    bl_label = 'Exponential sequence'
    bl_icon = 'OUTLINER_OB_EMPTY'

    x0_ = FloatProperty(
        name='x0', description='Value for n = 0',
        default=1.0,
        update=updateNode)

    maxValue_ = FloatProperty(
        name='Max', description='Maximum (absolute) value',
        default=0.0, min=0.0,
        options={'ANIMATABLE'}, update=updateNode)

    alpha_ = FloatProperty(
        name='alpha', description='Coefficient in exp(alpha*n)',
        default=0.1, update=updateNode)
    
    base_ = FloatProperty(
        name='base', description='Base of exponent - in base^n',
        default=2.0, update=updateNode)
    
    nmin_ = IntProperty(
        name='N from', description='Minimal value of N',
        default=0, update=updateNode)

    nmax_ = IntProperty(
        name='N to', description='Maximal value of N',
        default=10, update=updateNode)

    modes = [
        ("alpha_", "Log", "Specify coefficient in exp(alpha*n)", 1),
        ("base_",  "Base", "Specify base in base^n", 2),
    ]

    def mode_change(self, context):
        self.inputs[1].prop_name = self.mode
        updateNode(self, context)

    mode = EnumProperty(items=modes, default='alpha_', update=mode_change)

    func_dict = {'alpha_': exponential_e,
                 'base_': exponential_b }

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "X0").prop_name = 'x0_'
        self.inputs.new('StringsSocket', "Alpha").prop_name = 'alpha_'
        self.inputs.new('StringsSocket', "NMin").prop_name = 'nmin_'
        self.inputs.new('StringsSocket', "NMax").prop_name = 'nmax_'
        self.inputs.new('StringsSocket', "Maximum").prop_name = 'maxValue_'

        self.outputs.new('StringsSocket', "Sequence")

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode", expand=True)

    def process(self):
        # inputs
        x0 = self.inputs['X0'].sv_get()[0]
        alpha = self.inputs['Alpha'].sv_get()[0]
        nmin = self.inputs['NMin'].sv_get()[0]
        nmax = self.inputs['NMax'].sv_get()[0]

        m = self.inputs['Maximum'].sv_get(default=None)
        if m is None:
            maxValue = None
        else:
            maxValue = m[0]

        if not self.outputs['Sequence'].is_linked:
            return

        parameters = match_long_repeat([x0, alpha, nmin, nmax, maxValue])
        func = self.func_dict[self.mode]
        result = [list(func(*args)) for args in zip(*parameters)]
        self.outputs['Sequence'].sv_set(result)


def register():
    bpy.utils.register_class(SvGenExponential)


def unregister():
    bpy.utils.unregister_class(SvGenExponential)

