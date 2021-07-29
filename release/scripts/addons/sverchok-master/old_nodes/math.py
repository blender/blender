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

from math import degrees
from itertools import zip_longest

import bpy
from bpy.props import EnumProperty, BoolProperty, StringProperty
from mathutils import Vector
from mathutils.noise import noise_vector, cell_vector, noise, cell

from sverchok.node_tree import SverchCustomTreeNode, VerticesSocket, StringsSocket
from sverchok.data_structure import (fullList, levelsOflist, updateNode,
                            SvSetSocketAnyType, SvGetSocketAnyType)

'''
using slice [:] to generate 3-tuple instead of .to_tuple()
because it tests slightly faster on larger data.
'''


scalar_out = {
    "DOT":          (lambda u, v: Vector(u).dot(v), 2),
    "DISTANCE":     (lambda u, v: (Vector(u) - Vector(v)).length, 2),
    "ANGLE RAD":    (lambda u, v: Vector(u).angle(v, 0), 2),
    "ANGLE DEG":    (lambda u, v: degrees(Vector(u).angle(v, 0)), 2),

    "LEN":          (lambda u: Vector(u).length, 1),
    "NOISE-S":      (lambda u: noise(Vector(u)), 1),
    "CELL-S":       (lambda u: cell(Vector(u)), 1)
}

vector_out = {
    "CROSS":        (lambda u, v: Vector(u).cross(v)[:], 2),
    "ADD":          (lambda u, v: (u[0]+v[0], u[1]+v[1], u[2]+v[2]), 2),
    "SUB":          (lambda u, v: (u[0]-v[0], u[1]-v[1], u[2]-v[2]), 2),
    "REFLECT":      (lambda u, v: Vector(u).reflect(v)[:], 2),
    "PROJECT":      (lambda u, v: Vector(u).project(v)[:], 2),
    "SCALAR":       (lambda u, s: (Vector(u) * s)[:], 2),
    "1/SCALAR":     (lambda u, s: (Vector(u) * (1 / s))[:], 2),
    "ROUND":        (lambda u, s: Vector(u).to_tuple(s), 2),

    "NORMALIZE":    (lambda u: Vector(u).normalized()[:], 1),
    "NEG":          (lambda u: (-Vector(u))[:], 1),
    "NOISE-V":      (lambda u: noise_vector(Vector(u))[:], 1),
    "CELL-V":       (lambda u: cell_vector(Vector(u))[:], 1),

    "COMPONENT-WISE":  (lambda u, v: (u[0]*v[0], u[1]*v[1], u[2]*v[2]), 2)
}


class VectorMathNode(bpy.types.Node, SverchCustomTreeNode):

    ''' VectorMath Node '''
    bl_idname = 'VectorMathNode'
    bl_label = 'Vector Math'
    bl_icon = 'OUTLINER_OB_EMPTY'

    # vector math functions
    mode_items = [
        ("CROSS",       "Cross product",        "", 0),
        ("DOT",         "Dot product",          "", 1),
        ("ADD",         "Add",                  "", 2),
        ("SUB",         "Sub",                  "", 3),
        ("LEN",         "Length",               "", 4),
        ("DISTANCE",    "Distance",             "", 5),
        ("NORMALIZE",   "Normalize",            "", 6),
        ("NEG",         "Negate",               "", 7),

        ("NOISE-V",     "Noise Vector",         "", 8),
        ("NOISE-S",     "Noise Scalar",         "", 9),
        ("CELL-V",      "Vector Cell noise",    "", 10),
        ("CELL-S",      "Scalar Cell noise",    "", 11),

        ("ANGLE DEG",   "Angle Degrees",        "", 12),
        ("PROJECT",     "Project",              "", 13),
        ("REFLECT",     "Reflect",              "", 14),
        ("SCALAR",      "Multiply Scalar",      "", 15),
        ("1/SCALAR",    "Multiply 1/Scalar",    "", 16),

        ("ANGLE RAD",   "Angle Radians",        "", 17),
        ("ROUND",       "Round s digits",       "", 18),

        ("COMPONENT-WISE", "Component-wise U*V", "", 19)
    ]

    def mode_change(self, context):

        if not (self.items_ == self.current_op):
            self.label = self.items_
            self.update_outputs_and_inputs()
            self.current_op = self.items_
            updateNode(self, context)

    items_ = EnumProperty(
        items=mode_items,
        name="Function",
        description="Function choice",
        default="CROSS",
        update=mode_change)

    # matches default of CROSS product, defaults to False at init time.
    scalar_output_socket = BoolProperty()
    current_op = StringProperty(default="CROSS")

    def draw_buttons(self, context, layout):
        layout.prop(self, "items_", "Functions:")

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "U", "u")
        self.inputs.new('VerticesSocket', "V", "v")
        self.outputs.new('VerticesSocket', "W", "W")

    def update_outputs_and_inputs(self):
        '''
        Reaches here only if new operation is different from current op
        '''
        inputs = self.inputs
        outputs = self.outputs
        new_op = self.items_
        current_op = self.current_op
        scalars = ["SCALAR", "1/SCALAR", "ROUND"]

        if (new_op in scalars) and (current_op in scalars):
            return  # it's OK nothing to change

        '''
        reaches this point, means it needs to rewire
        '''

        # check and adjust outputs and input size
        if new_op in scalar_out:
            self.scalar_output_socket = True
            nrInputs = scalar_out[new_op][1]
            if 'W' in outputs:
                outputs.remove(outputs['W'])
                outputs.new('StringsSocket', "out", "out")

        else:
            self.scalar_output_socket = False
            nrInputs = vector_out[new_op][1]
            if 'out' in outputs:
                outputs.remove(outputs['out'])
                outputs.new('VerticesSocket', "W", "W")

        '''
        this monster removes and adds sockets depending on what kind of switch
        between new_ops is made, some new_op changes require addition of sockets
        others require deletion or replacement.
        '''

        add_scalar_input = lambda: inputs.new('StringsSocket', "S", "s")
        add_vector_input = lambda: inputs.new('VerticesSocket', "V", "v")
        remove_last_input = lambda: inputs.remove(inputs[-1])

        if nrInputs < len(inputs):
            remove_last_input()

        elif nrInputs > len(inputs):
            if (new_op in scalars):
                add_scalar_input()
            else:
                add_vector_input()

        else:
            if nrInputs == 1:
                # is only ever a vector u
                return

            if new_op in scalars:
                remove_last_input()
                add_scalar_input()
            elif (current_op in scalars):
                remove_last_input()
                add_vector_input()

    def process(self):
        inputs = self.inputs
        outputs = self.outputs
        operation = self.items_
        self.label = self.items_

        if not outputs[0].is_linked:
            return

        # this input is shared over both.
        vector1 = []
        if inputs['U'].is_linked:
            if isinstance(inputs['U'].links[0].from_socket, VerticesSocket):
                vector1 = SvGetSocketAnyType(self, inputs['U'], deepcopy=False)

        if not vector1:
            return

        # reaches here only if we have vector1
        u = vector1
        leve = levelsOflist(u)
        scalars = ["SCALAR", "1/SCALAR", "ROUND"]
        result = []

        # vector-output
        if 'W' in outputs and outputs['W'].is_linked:

            func = vector_out[operation][0]
            if len(inputs) == 1:
                try:
                    result = self.recurse_fx(u, func, leve - 1)
                except:
                    print('one input only, failed')
                    return

            elif len(inputs) == 2:

                '''
                get second input sockets content, depending on mode
                '''
                b = []
                if operation in scalars:
                    socket = ['S', StringsSocket]
                    msg = "two inputs, 1 scalar, "
                else:
                    socket = ['V', VerticesSocket]
                    msg = "two inputs, both vector, "

                name, _type = socket
                if name in inputs and inputs[name].links:
                    if isinstance(inputs[name].links[0].from_socket, _type):
                        b = SvGetSocketAnyType(self, inputs[name], deepcopy=False)

                # this means one of the necessary sockets is not connected
                if not b:
                    return

                try:
                    result = self.recurse_fxy(u, b, func, leve - 1)
                except:
                    print(self.name, msg, 'failed')
                    return

            else:
                return  # fail!

            SvSetSocketAnyType(self, 'W', result)

        # scalar-output
        if 'out' in outputs and outputs['out'].is_linked:

            vector2, result = [], []
            func = scalar_out[operation][0]
            num_inputs = len(inputs)

            try:
                if num_inputs == 1:
                    result = self.recurse_fx(u, func, leve - 1)

                elif all([num_inputs == 2, ('V' in inputs), (inputs['V'].links)]):

                    if isinstance(inputs['V'].links[0].from_socket, VerticesSocket):
                        vector2 = SvGetSocketAnyType(self, inputs['V'], deepcopy=False)
                        result = self.recurse_fxy(u, vector2, func, leve - 1)
                    else:
                        print('socket connected to V is not a vertices socket')
                else:
                    return

            except:
                print('failed scalar out, {} inputs'.format(num_inputs))
                return

            if result:
                SvSetSocketAnyType(self, 'out', result)

    '''
    apply f to all values recursively
    - fx and fxy do full list matching by length
    '''

    # vector -> scalar | vector
    def recurse_fx(self, l, f, leve):
        if not leve:
            return f(l)
        else:
            rfx = self.recurse_fx
            t = [rfx(i, f, leve-1) for i in l]
        return t

    def recurse_fxy(self, l1, l2, f, leve):
        res = []
        res_append = res.append
        # will only be used if lists are of unequal length
        fl = l2[-1] if len(l1) > len(l2) else l1[-1]
        if leve == 1:
            for u, v in zip_longest(l1, l2, fillvalue=fl):
                res_append(f(u, v))
        else:
            for u, v in zip_longest(l1, l2, fillvalue=fl):
                res_append(self.recurse_fxy(u, v, f, leve-1))
        return res


def register():
    bpy.utils.register_class(VectorMathNode)


def unregister():
    bpy.utils.unregister_class(VectorMathNode)
