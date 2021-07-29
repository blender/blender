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
from bpy.props import IntProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat

def fibonacci(x1, x2, count, maxValue):
    result = [x1,x2]
    for i in range(count-2):
        r = x1 + x2
        result.append(r)
        x1 = x2
        x2 = r
    
    if maxValue:
        actualMax = max(map(abs, result))
        if actualMax == 0.0:
            return result
        result = [x*maxValue/actualMax for x in result]

    return result


class SvGenFibonacci(bpy.types.Node, SverchCustomTreeNode):
    ''' Generator range list of floats'''
    bl_idname = 'SvGenFibonacci'
    bl_label = 'Fibonacci sequence'
    bl_icon = 'OUTLINER_OB_EMPTY'

    x1_ = FloatProperty(
        name='x1', description='First sequence value',
        default=1.0,
        options={'ANIMATABLE'}, update=updateNode)

    x2_ = FloatProperty(
        name='x2', description='Second sequence value',
        default=1.0,
        options={'ANIMATABLE'}, update=updateNode)

    count_ = IntProperty(
        name='count', description='Number of items to generate',
        default=10,
        options={'ANIMATABLE'}, min=3, update=updateNode)

    maxValue_ = FloatProperty(
        name='max', description='Maximum (absolute) value',
        default=0.0, min=0.0,
        options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "X1").prop_name = 'x1_'
        self.inputs.new('StringsSocket', "X2").prop_name = 'x2_'
        self.inputs.new('StringsSocket', "Count").prop_name = 'count_'
        self.inputs.new('StringsSocket', "Maximum").prop_name = 'maxValue_'

        self.outputs.new('StringsSocket', "Sequence")

    def process(self):
        # inputs
        x1 = self.inputs['X1'].sv_get()[0]
        x2 = self.inputs['X2'].sv_get()[0]
        count = self.inputs['Count'].sv_get()[0]
        m = self.inputs['Maximum'].sv_get(default=None)
        if m is None:
            maxValue = None
        else:
            maxValue = m[0]

        if not self.outputs['Sequence'].is_linked:
            return

        parameters = match_long_repeat([x1,x2, count, maxValue])
        result = [list(fibonacci(*args)) for args in zip(*parameters)]
        self.outputs['Sequence'].sv_set(result)


def register():
    bpy.utils.register_class(SvGenFibonacci)


def unregister():
    bpy.utils.unregister_class(SvGenFibonacci)
