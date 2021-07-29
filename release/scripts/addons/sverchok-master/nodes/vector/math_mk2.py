# BEGIN GPL LICENSE BLOCK #####
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
# END GPL LICENSE BLOCK #####

from math import degrees, sqrt
from itertools import zip_longest

import bpy
from bpy.props import EnumProperty, FloatProperty, FloatVectorProperty
from mathutils import Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import levelsOflist, updateNode

# pylint: disable=C0326

socket_type = {'s': 'StringsSocket', 'v': 'VerticesSocket'}


func_dict = {
    "DOT":            (1,  lambda u, v: Vector(u).dot(v),                          ('vv s'),        "Dot product"),
    "DISTANCE":       (5,  lambda u, v: (Vector(u) - Vector(v)).length,            ('vv s'),           "Distance"),
    "ANGLE DEG":      (12, lambda u, v: degrees(Vector(u).angle(v, 0)),            ('vv s'),      "Angle Degrees"),
    "ANGLE RAD":      (17, lambda u, v: Vector(u).angle(v, 0),                     ('vv s'),      "Angle Radians"), 

    "LEN":            (4,  lambda u: sqrt((u[0]*u[0])+(u[1]*u[1])+(u[2]*u[2])),     ('v s'),             "Length"),
    "CROSS":          (0,  lambda u, v: Vector(u).cross(v)[:],                     ('vv v'),      "Cross product"),
    "ADD":            (2,  lambda u, v: (u[0]+v[0], u[1]+v[1], u[2]+v[2]),         ('vv v'),                "Add"),
    "SUB":            (3,  lambda u, v: (u[0]-v[0], u[1]-v[1], u[2]-v[2]),         ('vv v'),                "Sub"),
    "PROJECT":        (13, lambda u, v: Vector(u).project(v)[:],                   ('vv v'),            "Project"),
    "REFLECT":        (14, lambda u, v: Vector(u).reflect(v)[:],                   ('vv v'),            "Reflect"),
    "COMPONENT-WISE": (19, lambda u, v: (u[0]*v[0], u[1]*v[1], u[2]*v[2]),         ('vv v'), "Component-wise U*V"),

    "SCALAR":         (15, lambda u, s: (u[0]*s, u[1]*s, u[2]*s),                  ('vs v'),    "Multiply Scalar"),
    "1/SCALAR":       (16, lambda u, s: (u[0]/s, u[1]/s, u[2]/s),                  ('vs v'),  "Multiply 1/Scalar"),
    "ROUND":          (18, lambda u, s: Vector(u).to_tuple(abs(int(s))),           ('vs v'),     "Round s digits"),

    "NORMALIZE":      (6,  lambda u: Vector(u).normalized()[:],                     ('v v'),          "Normalize"),
    "NEG":            (7,  lambda u: (-Vector(u))[:],                               ('v v'),             "Negate"),

    "SCALE XY":       (30, lambda u, s: (u[0]*s, u[1]*s, u[2]),                    ('vs v'),          "Scale XY"),
    "SCALE XZ":       (31, lambda u, s: (u[0]*s, u[1],   u[2]*s),                  ('vs v'),          "Scale XZ"),
    "SCALE YZ":       (32, lambda u, s: (u[0],   u[1]*s, u[2]*s),                  ('vs v'),          "Scale YZ")

}


mode_items = [(k, descr, '', ident) for k, (ident, _, _, descr) in sorted(func_dict.items(), key=lambda k: k[1][0])]

 
# apply f to all values recursively
# - fx and fxy do full list matching by length

def recurse_fx(l, f, level):
    if not level:
        return f(l)
    else:
        rfx = recurse_fx
        t = [rfx(i, f, level-1) for i in l]
    return t

def recurse_fxy(l1, l2, f, level):
    res = []
    res_append = res.append
    # will only be used if lists are of unequal length
    fl = l2[-1] if len(l1) > len(l2) else l1[-1]
    if level == 1:
        for u, v in zip_longest(l1, l2, fillvalue=fl):
            res_append(f(u, v))
    else:
        for u, v in zip_longest(l1, l2, fillvalue=fl):
            res_append(recurse_fxy(u, v, f, level-1))
    return res




class SvVectorMathNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' VectorMath Node MK2'''
    bl_idname = 'SvVectorMathNodeMK2'
    bl_label = 'Vector Math'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def mode_change(self, context):
        self.update_sockets()
        updateNode(self, context)

    current_op = EnumProperty(
        items=mode_items,
        name="Function",
        description="Function choice",
        default="CROSS",
        update=mode_change)

    amount = FloatProperty(default=1.0, name='amount', update=updateNode)
    v3_input_0 = FloatVectorProperty(size=3, default=(0,0,0), name='input a', update=updateNode)
    v3_input_1 = FloatVectorProperty(size=3, default=(0,0,0), name='input b', update=updateNode)


    def draw_label(self):
        return self.current_op


    def draw_buttons(self, ctx, layout):
        layout.prop(self, "current_op", text="Functions:")


    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "A").prop_name = 'v3_input_0'
        self.inputs.new('VerticesSocket', "B").prop_name = 'v3_input_1'
        self.outputs.new('VerticesSocket', "Out")


    def update_sockets(self):
        socket_info = func_dict.get(self.current_op)[2]
        t_inputs, t_outputs = socket_info.split(' ')

        self.outputs[0].replace_socket(socket_type.get(t_outputs))

        if len(t_inputs) > len(self.inputs):
            self.inputs.new('VerticesSocket', "dummy")
        elif len(t_inputs) < len(self.inputs):
            self.inputs.remove(self.inputs[-1])

        renames = 'AB'
        for idx, t_in in enumerate(t_inputs):
            s = self.inputs[idx].replace_socket(socket_type.get(t_in), renames[idx])
            if t_in == 'v':
                s.prop_name = 'v3_input_' + str(idx)
            else:
                s.prop_name = 'amount'


    def process(self):
        inputs, outputs = self.inputs, self.outputs

        if not outputs[0].is_linked:
            return

        func = func_dict.get(self.current_op)[1]
        num_inputs = len(inputs)

        # get either input data, or socket default
        input_one = inputs[0].sv_get(deepcopy=False)
        
        level = levelsOflist(input_one) - 1
        if num_inputs == 1:
            result = recurse_fx(input_one, func, level)
        else:
            input_two = inputs[1].sv_get(deepcopy=False)
            result = recurse_fxy(input_one, input_two, func, level)

        outputs[0].sv_set(result)



def register():
    bpy.utils.register_class(SvVectorMathNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvVectorMathNodeMK2)
