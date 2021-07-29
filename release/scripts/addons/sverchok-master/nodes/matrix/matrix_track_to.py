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
from bpy.props import BoolProperty, EnumProperty, FloatVectorProperty
from mathutils import Vector
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, match_long_repeat, enum_item as e)


class SvMatrixTrackToNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Construct a Matrix from arbitrary Track and Up vectors '''
    bl_idname = 'SvMatrixTrackToNode'
    bl_label = 'Matrix Track To'
    bl_icon = 'OUTLINER_OB_EMPTY'

    TUA = ["X Y", "X Z", "Y X", "Y Z", "Z X", "Z Y"]
    tu_axes = EnumProperty(
        name="Track/Up Axes",
        description="Select which two of the XYZ axes to be the Track and Up axes",
        items=e(TUA), default=TUA[0], update=updateNode)

    normalize = BoolProperty(
        name="Normalize Vectors", description="Normalize the output X,Y,Z vectors",
        default=True, update=updateNode)

    origin = FloatVectorProperty(
        name='Location', description="The location component of the output matrix",
        default=(0, 0, 0), update=updateNode)

    scale = FloatVectorProperty(
        name='Scale', description="The scale component of the output matrix",
        default=(1, 1, 1), update=updateNode)

    vA = FloatVectorProperty(
        name='A', description="A direction",
        default=(1, 0, 0), update=updateNode)

    vB = FloatVectorProperty(
        name='B', description='B direction',
        default=(0, 1, 0), update=updateNode)

    TUM = ["A B", "A -B", "-A B", "-A -B", "B A", "B -A", "-B A", "-B -A"]
    tu_mapping = EnumProperty(
        name="Track/Up Mapping",
        description="Map the Track and Up vectors to one of the two inputs or their negatives",
        items=e(TUM), default=TUM[0], update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Location").prop_name = "origin"  # L
        self.inputs.new('VerticesSocket', "Scale").prop_name = "scale"  # S
        self.inputs.new('VerticesSocket', "A").prop_name = "vA"  # A
        self.inputs.new('VerticesSocket', "B").prop_name = "vB"  # B
        self.outputs.new('MatrixSocket', "Matrix")
        self.outputs.new('VerticesSocket', "X")
        self.outputs.new('VerticesSocket', "Y")
        self.outputs.new('VerticesSocket', "Z")

    def split_columns(self, panel, ratios, aligns):
        """
        Splits the given panel into columns based on the given set of ratios.
        e.g ratios = [1, 2, 1] or [.2, .3, .2] etc
        Note: The sum of all ratio numbers doesn't need to be normalized
        """
        col2 = panel
        cols = []
        ns = len(ratios) - 1  # number of splits
        for n in range(ns):
            n1 = ratios[n]  # size of the current column
            n2 = sum(ratios[n + 1:])  # size of all remaining columns
            p = n1 / (n1 + n2)  # percentage split of current vs remaning columns
            # print("n = ", n, " n1 = ", n1, " n2 = ", n2, " p = ", p)
            split = col2.split(percentage=p, align=aligns[n])
            col1 = split.column(align=True)
            col2 = split.column(align=True)
            cols.append(col1)
        cols.append(col2)

        return cols

    def draw_buttons(self, context, layout):
        row = layout.column().row()
        cols = self.split_columns(row, [1, 1], [True, True])

        cols[0].prop(self, "tu_axes", "")
        cols[1].prop(self, "tu_mapping", "")

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, "normalize")

    def orthogonalizeXY(self, X, Y):  # keep X, recalculate Z form X&Y then Y
        Z = X.cross(Y)
        Y = Z.cross(X)
        return X, Y, Z

    def orthogonalizeXZ(self, X, Z):  # keep X, recalculate Y form Z&X then Z
        Y = Z.cross(X)
        Z = X.cross(Y)
        return X, Y, Z

    def orthogonalizeYX(self, Y, X):  # keep Y, recalculate Z form X&Y then X
        Z = X.cross(Y)
        X = Y.cross(Z)
        return X, Y, Z

    def orthogonalizeYZ(self, Y, Z):  # keep Y, recalculate X form Y&Z then Z
        X = Y.cross(Z)
        Z = X.cross(Y)
        return X, Y, Z

    def orthogonalizeZX(self, Z, X):  # keep Z, recalculate Y form Z&X then X
        Y = Z.cross(X)
        X = Y.cross(Z)
        return X, Y, Z

    def orthogonalizeZY(self, Z, Y):  # keep Z, recalculate X form Y&Z then Y
        X = Y.cross(Z)
        Y = Z.cross(X)
        return X, Y, Z

    def orthogonalizer(self):
        order = self.tu_axes.replace(" ", "")
        orthogonalizer = eval("self.orthogonalize" + order)
        return lambda T, U: orthogonalizer(T, U)

    def process(self):
        outputs = self.outputs

        # return if no outputs are connected
        if not any(s.is_linked for s in outputs):
            return

        # input values lists
        inputs = self.inputs
        input_locations = inputs["Location"].sv_get()[0]
        input_scales = inputs["Scale"].sv_get()[0]
        input_vAs = inputs["A"].sv_get()[0]
        input_vBs = inputs["B"].sv_get()[0]

        locations = [Vector(i) for i in input_locations]
        scales = [Vector(i) for i in input_scales]
        vAs = [Vector(i) for i in input_vAs]
        vBs = [Vector(i) for i in input_vBs]

        params = match_long_repeat([locations, scales, vAs, vBs])

        orthogonalize = self.orthogonalizer()

        mT, mU = self.tu_mapping.split(" ")

        xList = []  # ortho-normal X vector list
        yList = []  # ortho-normal Y vector list
        zList = []  # ortho-normal Z vector list
        matrixList = []
        for L, S, A, B in zip(*params):
            T = eval(mT)  # map T to one of A, B or its negative
            U = eval(mU)  # map U to one of A, B or its negative

            X, Y, Z = orthogonalize(T, U)

            if self.normalize:
                X.normalize()
                Y.normalize()
                Z.normalize()

            # prepare the Ortho-Normalized outputs
            if outputs["X"].is_linked:
                xList.append([X.x, X.y, X.z])
            if outputs["Y"].is_linked:
                yList.append([Y.x, Y.y, Y.z])
            if outputs["Z"].is_linked:
                zList.append([Z.x, Z.y, Z.z])

            # composite matrix: M = T * R * S (Tanslation x Rotation x Scale)
            m = [[X.x * S.x, Y.x * S.y, Z.x * S.z, L.x],
                 [X.y * S.x, Y.y * S.y, Z.y * S.z, L.y],
                 [X.z * S.x, Y.z * S.y, Z.z * S.z, L.z],
                 [0, 0, 0, 1]]

            matrixList.append(m)

        outputs["Matrix"].sv_set(matrixList)
        outputs["X"].sv_set([xList])
        outputs["Y"].sv_set([yList])
        outputs["Z"].sv_set([zList])


def register():
    bpy.utils.register_class(SvMatrixTrackToNode)


def unregister():
    bpy.utils.unregister_class(SvMatrixTrackToNode)
