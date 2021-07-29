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

from math import radians

import bpy
from bpy.props import EnumProperty, FloatProperty

from mathutils import Matrix, Euler

from sverchok.node_tree import SverchCustomTreeNode, StringsSocket
from sverchok.data_structure import (updateNode, Matrix_listing, match_long_repeat)


class SvMatrixEulerNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Construct a Matirx from Euler '''
    bl_idname = 'SvMatrixEulerNode'
    bl_label = 'Matrix Euler'
    bl_icon = 'OUTLINER_OB_EMPTY'


    X = FloatProperty(name='X', description='X rotation',
                             default=0.0,
                             options={'ANIMATABLE'}, update=updateNode)
    Y = FloatProperty(name='Y', description='Y rotation',
                             default=0.0,
                             options={'ANIMATABLE'}, update=updateNode)
    Z = FloatProperty(name='Z', description='Z rotation',
                             default=0.0,
                             options={'ANIMATABLE'}, update=updateNode)

    def change_prop(self, context):
        for i, name in enumerate(self.order):
            self.inputs[i].prop_name = name
        updateNode(self, context)

    orders = [
        ('XYZ', "XYZ",        "", 0),
        ('XZY', 'XZY',        "", 1),
        ('YXZ', 'YXZ',        "", 2),
        ('YZX', 'YZX',        "", 3),
        ('ZXY', 'ZXY',        "", 4),
        ('ZYX', 'ZYX',        "", 5),
    ]
    order = EnumProperty(name="Order", description="Order",
                          default="XYZ", items=orders,
                          update=change_prop)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "pos0").prop_name = 'X'
        self.inputs.new('StringsSocket', "pos1").prop_name = 'Y'
        self.inputs.new('StringsSocket', "pos1").prop_name = 'Z'
        self.outputs.new('MatrixSocket', "Matrix", "Matrix")

    def draw_buttons(self, context, layout):
        layout.prop(self, "order", text="Order:")

    def process(self):
        if not self.outputs['Matrix'].is_linked:
            return
        inputs = self.inputs
        param = [s.sv_get()[0] for s in inputs]
        mats = []
        for angles in zip(*match_long_repeat(param)):
            a_r = [radians(x) for x in angles]
            mat = Euler(a_r, self.order).to_matrix().to_4x4()
            mats.append(mat)
        self.outputs['Matrix'].sv_set(Matrix_listing(mats))


def register():
    bpy.utils.register_class(SvMatrixEulerNode)


def unregister():
    bpy.utils.unregister_class(SvMatrixEulerNode)
