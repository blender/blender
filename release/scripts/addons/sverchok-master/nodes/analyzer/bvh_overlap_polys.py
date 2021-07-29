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
import numpy as np
from mathutils.bvhtree import BVHTree
from bpy.props import FloatProperty, BoolProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode)


class SvBvhOverlapNodeNew(bpy.types.Node, SverchCustomTreeNode):
    ''' BVH Tree Overlap New '''
    bl_idname = 'SvBvhOverlapNodeNew'
    bl_label = 'overlap_polygons'
    bl_icon = 'OUTLINER_OB_EMPTY'

    triangles = BoolProperty(name="all triangles",
                             description="all triangles", default=False,
                             update=updateNode)

    epsilon = FloatProperty(name="epsilon",
                            default=0.0, min=0.0, max=10.0,
                            update=updateNode)

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, "triangles")
        layout.prop(self, "epsilon")

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vert(A)')
        self.inputs.new('StringsSocket', 'Poly(A)')
        self.inputs.new('VerticesSocket', 'Vert(B)')
        self.inputs.new('StringsSocket', 'Poly(B)')
        self.outputs.new('StringsSocket', 'PolyIndex(A)')
        self.outputs.new('StringsSocket', 'PolyIndex(B)')
        self.outputs.new('StringsSocket', 'OverlapPoly(A)')
        self.outputs.new('StringsSocket', 'OverlapPoly(B)')

    def process(self):
        btr = BVHTree.FromPolygons
        V1, P1, V2, P2 = [i.sv_get()[0] for i in self.inputs]
        outIndA, outIndB, Pover1, Pover2 = self.outputs
        Tri, epsi = self.triangles, self.epsilon
        T1 = btr(V1, P1, all_triangles = Tri, epsilon = epsi)
        T2 = btr(V2, P2, all_triangles = Tri, epsilon = epsi)
        ind1 = np.unique([i[0] for i in T1.overlap(T2)]).tolist()
        ind2 = np.unique([i[0] for i in T2.overlap(T1)]).tolist()
        if outIndA.is_linked:
            outIndA.sv_set([ind1])
        if outIndB.is_linked:
            outIndB.sv_set([ind2])
        if Pover1.is_linked:
            Pover1.sv_set([[P1[i] for i in ind1]])
        if Pover2.is_linked:
            Pover2.sv_set([[P2[i] for i in ind2]])


def register():
    bpy.utils.register_class(SvBvhOverlapNodeNew)


def unregister():
    bpy.utils.unregister_class(SvBvhOverlapNodeNew)
