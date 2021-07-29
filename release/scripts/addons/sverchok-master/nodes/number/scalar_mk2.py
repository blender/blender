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

from math import *
from itertools import zip_longest

import bpy
from bpy.props import EnumProperty, FloatProperty, IntProperty

from sverchok.ui.sv_icons import custom_icon
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.sv_itertools import (recurse_fx, recurse_fxy)
# pylint: disable=C0326

# Rules for modification:
#     1) Keep 4 items per column
#     2) only add new function with unique number

func_dict = {
    "--------------TRIG" : "#-------------------------------------------------#",
    "SINCOS":      (0,   lambda x: (sin(x), cos(x)),      ('s ss'), "Sin & Cos"),
    "SINE":        (1,   sin,                              ('s s'), "Sine"),
    "COSINE":      (2,   cos,                              ('s s'), "Cosine"),
    "TANGENT":     (3,   tan,                              ('s s'), "Tangent"),
    "ARCSINE":     (4,   asin,                             ('s s'), "Arcsine"),
    "ARCCOSINE":   (5,   acos,                             ('s s'), "Arccosine"),
    "ARCTANGENT":  (6,   atan,                             ('s s'), "Arctangent"),
    "ACOSH":       (7,   acosh,                            ('s s'), "acosh"),
    "ASINH":       (8,   asinh,                            ('s s'), "asinh"),
    "ATANH":       (9,   atanh,                            ('s s'), "atanh"),
    "COSH":        (10,  cosh,                             ('s s'), "cosh"),
    "SINH":        (11,  sinh,                             ('s s'), "sinh"),
    "TANH":        (12,  tanh,                             ('s s'), "tanh"),
    "DEGREES":     (20,  degrees,                          ('s s'), "Degrees"),
    "RADIANS":     (22,  radians,                          ('s s'), "Radians"),
    "---------------OPS" : "#---------------------------------------------------#",
    "ADD":         (30,  lambda x, y: x+y,                ('ss s'), "Add"),
    "SUB":         (31,  lambda x, y: x-y,                ('ss s'), "Sub"),
    "MUL":         (32,  lambda x, y: x*y,                ('ss s'), "Multiply"),
    "DIV":         (33,  lambda x, y: x/y,                ('ss s'), "Divide"),
    "INTDIV":      (34,  lambda x, y: x//y,               ('ss s'), "Int Division"),
    "SQRT":        (40,  lambda x: sqrt(fabs(x)),          ('s s'), "Squareroot"),
    "EXP":         (41,  exp,                              ('s s'), "Exponent"),
    "POW":         (42,  lambda x, y: x**y,               ('ss s'), "Power y"),
    "POW2":        (43,  lambda x: x*x,                    ('s s'), "Power 2"),
    "LN":          (44,  log,                              ('s s'), "log"),
    "LOG10":       (50,  log10,                            ('s s'), "log10"),
    "LOG1P":       (51,  log1p,                            ('s s'), "log1p"),
    "ABS":         (60,  fabs,                             ('s s'), "Absolute"),
    "NEG":         (61,  lambda x: -x,                     ('s s'), "Negate"),
    "CEIL":        (62,  ceil,                             ('s s'), "Ceiling"),
    "FLOOR":       (63,  floor,                            ('s s'), "floor"),
    "MIN":         (70,  min,                             ('ss s'), "min"),
    "MAX":         (72,  max,                             ('ss s'), "max"),
    "ROUND":       (80,  round,                            ('s s'), "Round"),
    "ROUND-N":     (81,  lambda x, y: round(x, int(y)),   ('ss s'), "Round N",),
    "FMOD":        (82,  fmod,                            ('ss s'), "Fmod"),
    "MODULO":      (83,  lambda x, y: (x % y),            ('ss s'), "modulo"),
    "-------------CONST" : "#---------------------------------------------------#",
    "PI":          (90,  lambda x: pi * x,                 ('s s'), "pi * x"),
    "TAU":         (100, lambda x: pi * 2 * x,             ('s s'), "tau * x"),
    "E":           (110, lambda x: e * x,                  ('s s'), "e * x"),
    "PHI":         (120, lambda x: 1.61803398875 * x,      ('s s'), "phi * x"),
    "+1":          (130, lambda x: x + 1,                  ('s s'), "x + 1"),
    "-1":          (131, lambda x: x - 1,                  ('s s'), "x - 1"),
    "*2":          (132, lambda x: x * 2,                  ('s s'), "x * 2"),
    "/2":          (133, lambda x: x / 2,                  ('s s'), "x / 2"),
    "RECIP":       (135, lambda x: 1 / x,                  ('s s'), "1 / x"),
    "THETA TAU":   (140, lambda x: pi * 2 * ((x-1) / x),   ('s s'), "tau * (x-1 / x)")
}

def func_from_mode(mode):
    return func_dict[mode][1]

def generate_node_items():
    prefilter = {k: v for k, v in func_dict.items() if not k.startswith('---')}
    return [(k, descr, '', ident) for k, (ident, _, _, descr) in sorted(prefilter.items(), key=lambda k: k[1][0])]

mode_items = generate_node_items()


def property_change(node, context, origin):
    if origin == 'input_mode_one':
        node.inputs[0].prop_name = {'Float': 'x_', 'Int': 'xi_'}.get(getattr(node, origin))
    elif origin == 'input_mode_two' and len(node.inputs) == 2:
        node.inputs[1].prop_name = {'Float': 'y_', 'Int': 'yi_'}.get(getattr(node, origin))
    else:
        pass
    updateNode(node, context)


class SvScalarMathNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' SvScalarMathNodeMK2 '''
    bl_idname = 'SvScalarMathNodeMK2'
    bl_label = 'Math MK2'
    sv_icon = 'SV_FUNCTION'

    def mode_change(self, context):
        self.update_sockets()
        updateNode(self, context)


    current_op = EnumProperty(
        name="Function", description="Function choice", default="MUL",
        items=mode_items, update=mode_change)

    x_ = FloatProperty(default=1.0, name='x', update=updateNode)
    y_ = FloatProperty(default=1.0, name='y', update=updateNode)
    xi_ = IntProperty(default=1, name='x', update=updateNode)
    yi_ = IntProperty(default=1, name='y', update=updateNode)

    mode_options = [(k, k, '', i) for i, k in enumerate(["Float", "Int"])]
    
    input_mode_one = EnumProperty(
        items=mode_options, description="offers int / float selection for socket 1",
        default="Float", update=lambda s, c: property_change(s, c, 'input_mode_one'))

    input_mode_two = EnumProperty(
        items=mode_options, description="offers int / float selection for socket 2",
        default="Float", update=lambda s, c: property_change(s, c, 'input_mode_two'))


    def draw_label(self):
        return self.current_op


    def draw_buttons(self, ctx, layout):
        layout.row().prop(self, "current_op", text="", icon_value=custom_icon("SV_FUNCTION"))


    def draw_buttons_ext(self, ctx, layout):
        layout.row().prop(self, 'input_mode_one', text="input 1")
        if len(self.inputs) == 2:
            layout.row().prop(self, 'input_mode_two', text="input 2")


    def sv_init(self, context):
        self.inputs.new('StringsSocket', "x").prop_name = 'x_'
        self.inputs.new('StringsSocket', "y").prop_name = 'y_'
        self.outputs.new('StringsSocket', "Out")


    def update_sockets(self):
        socket_info = func_dict.get(self.current_op)[2]
        t_inputs, t_outputs = socket_info.split(' ')

        if len(t_inputs) > len(self.inputs):
            new_second_input = self.inputs.new('StringsSocket', "y").prop_name = 'y_'
            if self.input_mode_two == 'Int':
                new_second_input.prop_name = 'yi_'
        elif len(t_inputs) < len(self.inputs):
            self.input_mode_two = 'Float'
            self.inputs.remove(self.inputs[-1])

        if len(t_outputs) > len(self.outputs):
            self.outputs.new('StringsSocket', "cos( x )")
        elif len(t_outputs) < len(self.outputs):
            self.outputs.remove(self.outputs[-1])

        if len(self.outputs) == 1:
            if not "Out" in self.outputs:
                self.outputs[0].replace_socket("StringsSocket", "Out")
        elif len(self.outputs) == 2:
            self.outputs[0].replace_socket("StringsSocket", "sin( x )")


    def process(self):
        signature = (len(self.inputs), len(self.outputs))

        x = self.inputs['x'].sv_get(deepcopy=False)
        if signature == (2, 1):
            y = self.inputs['y'].sv_get(deepcopy=False)

        if self.outputs[0].is_linked:
            result = []
            current_func = func_from_mode(self.current_op)
            if signature == (1, 1):
                result = recurse_fx(x, current_func)
            elif signature == (2, 1):
                result = recurse_fxy(x, y, current_func)
            elif signature == (1, 2):
                # special case at the moment
                result = recurse_fx(x, sin)
                result2 = recurse_fx(x, cos)
                self.outputs[1].sv_set(result2)

            self.outputs[0].sv_set(result)



def register():
    bpy.utils.register_class(SvScalarMathNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvScalarMathNodeMK2)
