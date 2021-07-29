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
from math import isclose

import bpy
from bpy.props import BoolProperty, FloatProperty, IntProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat


class SvRndNumGen(bpy.types.Node, SverchCustomTreeNode):
    ''' Generate a random number (int of float) thru a given range (inclusive) '''
    bl_idname = 'SvRndNumGen'
    bl_label = 'Random Num Gen'
    bl_icon = 'OUTLINER_OB_EMPTY'

    low_f = FloatProperty(
        name='Float Low', description='Minimum float value',
        default=0.0,
        options={'ANIMATABLE'}, update=updateNode)

    high_f = FloatProperty(
        name='Float High', description='Maximum float value',
        default=1.0,
        options={'ANIMATABLE'}, update=updateNode)

    low_i = IntProperty(
        name='Int Low', description='Minimum integer value',
        default=0,
        options={'ANIMATABLE'}, update=updateNode)

    high_i = IntProperty(
        name='Int High', description='Maximum integer value',
        default=10,
        options={'ANIMATABLE'}, update=updateNode)

    size = IntProperty(
        name='Size', description='number of values to output (count.. or size)',
        default=10,
        options={'ANIMATABLE'}, update=updateNode)

    seed = IntProperty(
        name='Seed', description='seed, grow',
        default=0,
        options={'ANIMATABLE'}, update=updateNode)

    as_list = BoolProperty(
        name='As List', description='on means output list, off means output np.array 1d',
        default=True, 
        update=updateNode)

    mode_options = [
        ("Simple", "Simple", "", 0),
        ("Advanced", "Advanced", "", 1)
    ]
    
    selected_mode = bpy.props.EnumProperty(
        items=mode_options,
        description="offers....",
        default="Simple", update=updateNode
    )


    def adjust_inputs(self, context):
        m = self.type_selected_mode
        si = self.inputs

        if m == 'Int' and si[2].prop_name[-1] == 'f':
            si[2].prop_name = 'low_i'
            si[3].prop_name = 'high_i'
        
        elif m == 'Float' and si[2].prop_name[-1] == 'i':
            si[2].prop_name = 'low_f'
            si[3].prop_name = 'high_f'

        updateNode(self, context)


    type_mode_options = [
        ("Int", "Int", "", 0),
        ("Float", "Float", "", 1)
    ]
    
    type_selected_mode = bpy.props.EnumProperty(
        items=type_mode_options,
        description="offers....",
        default="Int", update=adjust_inputs
    )


    def sv_init(self, context):
        si = self.inputs
        si.new('StringsSocket', "Size").prop_name = 'size'
        si.new('StringsSocket', "Seed").prop_name = 'seed'
        si.new('StringsSocket', "Low").prop_name = 'low_i'
        si.new('StringsSocket', "High").prop_name = 'high_i'
        so = self.outputs
        so.new('StringsSocket', "Value")


    def draw_buttons(self, _, layout):
        row = layout.row()
        row.prop(self, 'type_selected_mode', expand=True)
        layout.prop(self, "as_list")
    

    def produce_range(self, *params):
        size, seed, low, high = params

        size = max(size, 1)
        seed = max(seed, 0)

        np.random.seed(seed)

        if self.type_selected_mode == 'Int':
            low, high = sorted([low, high])
            result = np.random.random_integers(low, high, size)
        else:
            result = np.random.ranf(size)
            epsilon_relative = 1e-06
            if isclose(low, 0.0, rel_tol=epsilon_relative) and isclose(high, 1.0, rel_tol=epsilon_relative):
                pass
            else:
                my_func = lambda inval: np.interp(inval, [0.0, 1.0], [low, high])
                result = np.apply_along_axis(my_func, 0, result)

        if self.as_list:
            result = result.tolist()

        return result


    def process(self):
        outputs = self.outputs

        if outputs['Value'].is_linked:
            params = [self.inputs[i].sv_get()[0] for i in range(4)]
            out = [self.produce_range(*args) for args in zip(*match_long_repeat(params))]
            outputs['Value'].sv_set(out)

def register():
    bpy.utils.register_class(SvRndNumGen)


def unregister():
    bpy.utils.unregister_class(SvRndNumGen)
