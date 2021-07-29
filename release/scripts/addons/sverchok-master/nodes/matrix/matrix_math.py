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
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty

from mathutils import Matrix
from functools import reduce

from sverchok.node_tree import SverchCustomTreeNode, MatrixSocket, StringsSocket
from sverchok.data_structure import (updateNode, match_long_repeat,
                                     Matrix_listing, Matrix_generate)

operationItems = [
    ("MULTIPLY", "Multiply", "Multiply two matrices", 0),
    ("INVERT", "Invert", "Invert matrix", 1),
    ("FILTER", "Filter", "Filter matrix components", 2),
    ("BASIS", "Basis", "Extract Basis vectors", 3)
]

prePostItems = [
    ("PRE", "Pre", "Calculate A op B", 0),
    ("POST", "Post", "Calculate B op A", 1)
]

id_mat = Matrix_listing([Matrix.Identity(4)])
ABC = tuple('ABCDEFGHIJKLMNOPQRSTUVWXYZ')


class SvMatrixMathNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Math operation on matrices '''
    bl_idname = 'SvMatrixMathNode'
    bl_label = 'Matrix Math'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def update_operation(self, context):
        self.label = "Matrix " + self.operation.title()
        self.update_sockets()
        updateNode(self, context)

    prePost = EnumProperty(
        name='Pre Post',
        description='Order of operations PRE = A op B vs POST = B op A)',
        items=prePostItems, default="PRE", update=updateNode)

    operation = EnumProperty(
        name="Operation",
        description="Operation to apply on the given matrices",
        items=operationItems, default="MULTIPLY", update=update_operation)

    filter_t = BoolProperty(
        name="Filter Translation",
        description="Filter out the translation component of the matrix",
        default=False, update=updateNode)

    filter_r = BoolProperty(
        name="Filter Rotation",
        description="Filter out the rotation component of the matrix",
        default=False, update=updateNode)

    filter_s = BoolProperty(
        name="Filter Scale",
        description="Filter out the scale component of the matrix",
        default=False, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('MatrixSocket', "A", "A")
        self.inputs.new('MatrixSocket', "B", "B")

        self.outputs.new('MatrixSocket', "C", "C")

        self.outputs.new('VerticesSocket', "X", "X")
        self.outputs.new('VerticesSocket', "Y", "Y")
        self.outputs.new('VerticesSocket', "Z", "Z")

        self.operation = "MULTIPLY"

    def update_sockets(self):
        # update inputs
        inputs = self.inputs
        if self.operation in {"MULTIPLY"}:  # multiple input operations
            if len(inputs) < 2:  # at least two matrix inputs are available
                if not "B" in inputs:
                    inputs.new("MatrixSocket", "B")
        else:  # single input operations (remove all inputs except the first one)
            ss = [s for s in inputs]
            for s in ss:
                if s != inputs["A"]:
                    inputs.remove(s)

        # update outputs
        outputs = self.outputs
        if self.operation == "BASIS":
            for name in list("XYZ"):
                if name not in outputs:
                    outputs.new("VerticesSocket", name)
        else:  # remove basis output sockets for all other operations
            for name in list("XYZ"):
                if name in outputs:
                    outputs.remove(outputs[name])

    def draw_buttons(self, context, layout):
        layout.prop(self, "operation", text="")
        if self.operation == "MULTIPLY":
            layout.prop(self, "prePost", expand=True)
        elif self.operation == "FILTER":
            row = layout.row(align=True)
            row.prop(self, "filter_t", toggle=True, text="T")
            row.prop(self, "filter_r", toggle=True, text="R")
            row.prop(self, "filter_s", toggle=True, text="S")

    def operation_filter(self, a):
        T, R, S = a.decompose()

        if self.filter_t:
            mat_t = Matrix().Identity(4)
        else:
            mat_t = Matrix().Translation(T)

        if self.filter_r:
            mat_r = Matrix().Identity(4)
        else:
            mat_r = R.to_matrix().to_4x4()

        if self.filter_s:
            mat_s = Matrix().Identity(4)
        else:
            mat_s = Matrix().Identity(4)
            mat_s[0][0] = S[0]
            mat_s[1][1] = S[1]
            mat_s[2][2] = S[2]

        m = mat_t * mat_r * mat_s

        return m

    def operation_basis(self, a):
        T, R, S = a.decompose()

        rot = R.to_matrix().to_4x4()
        Rx = (rot[0][0], rot[1][0], rot[2][0])
        Ry = (rot[0][1], rot[1][1], rot[2][1])
        Rz = (rot[0][2], rot[1][2], rot[2][2])

        return Rx, Ry, Rz

    def get_operation(self):
        if self.operation == "MULTIPLY":
            return lambda l: reduce((lambda a, b: a * b), l)
        elif self.operation == "FILTER":
            return self.operation_filter
        elif self.operation == "INVERT":
            return lambda a: a.inverted()
        elif self.operation == "BASIS":
            return self.operation_basis

    def update(self):
        # sigle input operation ? => no need to update sockets
        if self.operation not in {"MULTIPLY"}:
            return

        # multiple input operation ? => add an empty last socket
        inputs = self.inputs
        if inputs[-1].links:
            name = ABC[len(inputs)]  # pick the next letter A to Z
            inputs.new("MatrixSocket", name)
        else:  # last input disconnected ? => remove all but last unconnected extra inputs
            while len(inputs) > 2 and not inputs[-2].links:
                inputs.remove(inputs[-1])

    def process(self):
        outputs = self.outputs
        if not any(s.is_linked for s in outputs):
            return

        I = []  # collect the inputs from the connected sockets
        for s in filter(lambda s: s.is_linked, self.inputs):
            I.append([Matrix(m) for m in s.sv_get(default=id_mat)])

        operation = self.get_operation()

        if self.operation in {"MULTIPLY"}:  # multiple input operations
            if self.prePost == "PRE":  # A op B : keep input order
                parameters = match_long_repeat(I)
            else:  # B op A : reverse input order
                parameters = match_long_repeat(I[::-1])

            matrixList = [operation(params) for params in zip(*parameters)]

            matrices = Matrix_listing(matrixList)
            outputs['C'].sv_set(matrices)

        else:  # single input operations
            parameters = I[0]
            print("parameters=", parameters)

            if self.operation == "BASIS":
                xList = []
                yList = []
                zList = []
                for a in parameters:
                    Rx, Ry, Rz = operation(a)
                    xList.append(Rx)
                    yList.append(Ry)
                    zList.append(Rz)
                outputs['X'].sv_set(xList)
                outputs['Y'].sv_set(yList)
                outputs['Z'].sv_set(zList)

                matrices = Matrix_listing(parameters)
                outputs['C'].sv_set(matrices)

            else:  # INVERSE / FILTER
                matrixList = [operation(a) for a in parameters]

                matrices = Matrix_listing(matrixList)
                outputs['C'].sv_set(matrices)


def register():
    bpy.utils.register_class(SvMatrixMathNode)


def unregister():
    bpy.utils.unregister_class(SvMatrixMathNode)
