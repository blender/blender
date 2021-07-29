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
from bpy.props import BoolProperty
from mathutils import Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (
    Matrix_generate, Matrix_location, Vector_generate
)


class DistancePPNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Distance Point to Point '''
    bl_idname = 'DistancePPNode'
    bl_label = 'Distance'
    bl_icon = 'OUTLINER_OB_EMPTY'
    sv_icon = 'SV_DISTANCE'

    Cross_dist = BoolProperty(name='Cross_dist', description='DANGEROUS! If crossover dimension calculation, be sure',
                              default=False)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vertices1', 'vertices1')
        self.inputs.new('MatrixSocket', 'matrix1', 'matrix1')
        self.inputs.new('VerticesSocket', 'vertices2', 'vertices2')
        self.inputs.new('MatrixSocket', 'matrix2', 'matrix2')
        self.outputs.new('StringsSocket', 'distances', 'distances')

    def draw_buttons(self, context, layout):
        layout.prop(self, "Cross_dist", text="CrossOver")

    def process(self):
        if self.inputs['vertices1'].is_linked and self.inputs['vertices2'].is_linked:
            prop1_ = self.inputs['vertices1'].sv_get()
            prop1 = Vector_generate(prop1_)
            prop2_ = self.inputs['vertices2'].sv_get()
            prop2 = Vector_generate(prop2_)

        elif self.inputs['matrix1'].is_linked and self.inputs['matrix2'].is_linked:
            propa = self.inputs['matrix1'].sv_get()
            prop1 = Matrix_location(Matrix_generate(propa))
            propb = self.inputs['matrix2'].sv_get()
            prop2 = Matrix_location(Matrix_generate(propb))
        else:
            prop1, prop2 = [], []

        if prop1 and prop2:
            if self.outputs['distances'].is_linked:
                # print ('distances input', str(prop1), str(prop2))
                if self.Cross_dist:
                    output = self.calcM(prop1, prop2)
                else:
                    output = self.calcV(prop1, prop2)
                self.outputs['distances'].sv_set(output)

                # print ('distances out' , str(output))
        else:
            self.outputs['distances'].sv_set([])

    def calcV(self, list1, list2):
        dists = []
        lenlis = min(len(list1), len(list2)) - 1
        for i, object1 in enumerate(list1):
            if i > lenlis:
                continue
            values = []
            lenlen = min(len(object1), len(list2[i])) - 1
            for k, vert1 in enumerate(object1):
                if k > lenlen:
                    continue
                values.append(self.distance(vert1, list2[i][k]))
            dists.append(values)
        # print(dists)
        return dists

    def calcM(self, list1, list2):
        ll1, ll2 = len(list1[0]), len(list2[0])
        if ll1 > ll2:
            short = list2
            long = list1
        else:
            short = list1
            long = list2
        dists = []
        for obsh in short:
            obshdis = []
            for vers in obsh:
                for obln in long:
                    oblndis = []
                    for verl in obln:
                        oblndis.append(self.distance(vers, verl))
                    obshdis.append(oblndis)
            dists.append(obshdis)
        # print(dists)
        return dists[0]

    def distance(self, x, y):
        vec = Vector((x[0] - y[0], x[1] - y[1], x[2] - y[2]))
        return vec.length


def register():
    bpy.utils.register_class(DistancePPNode)


def unregister():
    bpy.utils.unregister_class(DistancePPNode)
