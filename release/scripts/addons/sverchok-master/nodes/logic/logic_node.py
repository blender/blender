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
from bpy.props import (EnumProperty, FloatProperty,
                       IntProperty, BoolVectorProperty)

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode)
from sverchok.utils.sv_itertools import (recurse_fx, recurse_fxy)


class SvLogicNode(bpy.types.Node, SverchCustomTreeNode):
    ''' LogicNode '''
    bl_idname = 'SvLogicNode'
    bl_label = 'Logic functions'
    bl_icon = 'LOGIC'


# Math functions from http://docs.python.org/3.3/library/math.html
# maybe this should be distilled to most common with the others available via Formula2 Node
# And some constants etc.
# Keep 4, columns number unchanged and only add new with unique number


    def change_type(self, context):
        nrInputs = 1
        if self.items_ in self.constant:
            nrInputs = 0
        elif self.items_ in self.fx:
            nrInputs = 1
        elif self.items_ in self.fxy or self.items_ in self.fxy2:
            nrInputs = 2

        self.set_inputs(nrInputs)

        if self.items_ in self.fxy2:
            self.inputs[0].prop_name = 'i_x'
            self.inputs[1].prop_name = 'i_y'
        elif self.items_ in self.fxy:
            self.inputs[0].prop_name = 'x'
            self.inputs[1].prop_name = 'y'
        elif self.items_ in self.fx:
            self.inputs[0].prop_name = 'x'

    def set_inputs(self, n):
        if n == len(self.inputs):
            return
        if n < len(self.inputs):
            while n < len(self.inputs):
                self.inputs.remove(self.inputs[-1])
        if n > len(self.inputs):
            if 'X' not in self.inputs:
                self.inputs.new('StringsSocket', "X")
            if 'Y' not in self.inputs and n == 2:
                self.inputs.new('StringsSocket', "Y")
            self.change_prop_type(None)


    mode_items = [
        ("AND",             "And",          "", 1),
        ("OR",              "Or",           "", 2),
        ("IF",              "If",           "", 3),
        ("NOT",             "Not",          "", 4),
        ("NAND",            "Nand",         "", 5),
        ("NOR",             "Nor",          "", 6),
        ("XOR",             "Xor",          "", 7),
        ("XNOR",            "Xnor",         "", 8),
        ("LESS",             "<",           "", 9),
        ("BIG",              ">",           "", 10),
        ("EQUAL",            "==",          "", 11),
        ("NOT_EQ",           "!=",          "", 12),
        ("LESS_EQ",          "<=",          "", 13),
        ("BIG_EQ",           ">=",          "", 14),
        ("TRUE",             "True",        "", 15),
        ("FALSE",            "False",       "", 16),
        ]

    fx = {
        'IF':        lambda x: bool(x),
        'NOT':       lambda x: bool(abs(x-1)),
        }

    fxy = {
        'AND':      lambda x, y : bool(x) and bool(y),
        'OR':       lambda x, y : bool(x) or bool(y),
        'NAND':     lambda x, y : not (bool(x) and bool(y)),
        'NOR':      lambda x, y : not (bool(x) or bool(y)),
        'XOR':      lambda x, y : bool(x) ^ bool(y),
        'XNOR':     lambda x, y : (bool(x) and bool(y)) or (not (bool(x) or bool(y))),
        }

    fxy2 = {
        'LESS':     lambda x, y : x < y,
        'BIG':      lambda x, y : x > y,
        'EQUAL':    lambda x, y : x == y,
        'NOT_EQ':   lambda x, y : x != y,
        'LESS_EQ':  lambda x, y : x <= y,
        'BIG_EQ':   lambda x, y : x >= y,
        }

    constant = {
        'FALSE':     False,
        'TRUE':      True,
        }

    int_prop ={
        'ROUND-N':  ("x","i_y"),
        }

    # items_ is a really bad name but changing it breaks old layouts
    items_ = EnumProperty(name="Logic Gate", description="Logic Gate choice",
                          default="AND", items=mode_items,
                          update=change_type)
    x = IntProperty(default=1, name='x', max=1, min=0, update=updateNode)
    y = IntProperty(default=1, name='y', max=1, min=0, update=updateNode)

    i_x = FloatProperty(default=1, name='x', update=updateNode)
    i_y = FloatProperty(default=1, name='y', update=updateNode)

     # boolvector to control prop type
    def change_prop_type(self, context):
        inputs = self.inputs
        if inputs:
            inputs[0].prop_name = 'i_x' if self.prop_types[0] else 'x'
        if len(inputs)>1:
            if not self.items_ in self.int_prop:
                inputs[1].prop_name = 'i_y' if self.prop_types[1] else 'y'
            else:
                inputs[1].prop_name = 'i_y'

    prop_types = BoolVectorProperty(size=2, default=(False, False),
                                    update=change_prop_type)

    def draw_buttons(self, context, layout):
        layout.prop(self, "items_", "Functions:")

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, "items_", "Functions:")
        layout.label(text="Change property type")
        for i,s in enumerate(self.inputs):
            row = layout.row()
            row.label(text=s.name)
            t = "To int" if self.prop_types[i] else "To float"
            row.prop(self, "prop_types", index=i, text=t, toggle=True)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "X").prop_name = 'x'
        self.inputs.new('StringsSocket', "Y").prop_name = 'y'
        self.outputs.new('StringsSocket', "Gate")

    def draw_label(self):
        nrInputs = len(self.inputs)
        label = [self.items_]
        if nrInputs:
            x = self.i_x if self.prop_types[0] else self.x
            x_label = 'X' if self.inputs[0].links else str(round(x, 3))
            label.append(x_label)
        if nrInputs == 2:
            y = self.i_y if self.prop_types[1] else self.y
            y_label = 'Y' if self.inputs[1].links else str(round(y, 3))
            label.extend((", ", y_label))
        return " ".join(label)


    def process(self):
        # inputs
        if  not self.outputs['Gate'].is_linked:
            return

        if 'X' in self.inputs:
            x = self.inputs['X'].sv_get(deepcopy=False)

        if 'Y' in self.inputs:
            y = self.inputs['Y'].sv_get(deepcopy=False)

        # outputs
        out= []
        if self.items_ in self.constant:
            out = [[self.constant[self.items_]]]
        elif self.items_ in self.fx:
            out = recurse_fx(x, self.fx[self.items_])
        elif self.items_ in self.fxy:
            out = recurse_fxy(x, y, self.fxy[self.items_])
        elif self.items_ in self.fxy2:
            out = recurse_fxy(x, y, self.fxy2[self.items_])

        self.outputs['Gate'].sv_set(out)



def register():
    bpy.utils.register_class(SvLogicNode)


def unregister():
    bpy.utils.unregister_class(SvLogicNode)
