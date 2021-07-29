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

from math import sin, cos, pi, sqrt, radians
from random import random
import time

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat


class SvSplitEdgesNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Split Edges '''
    bl_idname = 'SvSplitEdgesNode'
    bl_label = 'Split Edges'
    # sv_icon = 'SV_EDGE_SPLIT'

    factor = FloatProperty(
        name="Factor", description="Split Factor",
        default=0.5, min=0.0, soft_min=0.0, max=1.0,
        update=updateNode)

    mirror = BoolProperty(
        name="Mirror", description="Mirror split",
        default=False,
        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Edges")
        self.inputs.new('StringsSocket', "Factor").prop_name = 'factor'

        self.outputs.new('VerticesSocket',  "Vertices")
        self.outputs.new('StringsSocket',  "Edges")

    def draw_buttons(self, context, layout):
        layout.prop(self, 'mirror')

    def process(self):
        # return if no outputs are connected
        if not any(s.is_linked for s in self.outputs):
            return

        vertList = self.inputs['Vertices'].sv_get()[0]
        edgeList = self.inputs['Edges'].sv_get()[0]
        input_f = self.inputs['Factor'].sv_get()[0]

        # sanitize the input
        input_f = list(map(lambda f: min(1, max(0, f)), input_f))

        params = match_long_repeat([edgeList, input_f])

        offset = len(vertList)
        newVerts = list(vertList)
        newEdges = []
        i = 0
        for edge, f in zip(*params):
            i0 = edge[0]
            i1 = edge[1]
            v0 = vertList[i0]
            v1 = vertList[i1]

            if self.mirror:
                f = f / 2

                vx = v0[0] * (1 - f) + v1[0] * f
                vy = v0[1] * (1 - f) + v1[1] * f
                vz = v0[2] * (1 - f) + v1[2] * f
                va = [vx, vy, vz]
                newVerts.append(va)

                vx = v0[0] * f + v1[0] * (1 - f)
                vy = v0[1] * f + v1[1] * (1 - f)
                vz = v0[2] * f + v1[2] * (1 - f)
                vb = [vx, vy, vz]
                newVerts.append(vb)

                newEdges.append([i0, offset + i])  # v0 - va
                newEdges.append([offset + i, offset + i + 1])  # va - vb
                newEdges.append([offset + i + 1, i1])  # vb - v1
                i = i + 2

            else:
                vx = v0[0] * (1 - f) + v1[0] * f
                vy = v0[1] * (1 - f) + v1[1] * f
                vz = v0[2] * (1 - f) + v1[2] * f
                va = [vx, vy, vz]
                newVerts.append(va)

                newEdges.append([i0, offset + i])  # v0 - va
                newEdges.append([offset + i, i1])  # va - v1
                i = i + 1

        self.outputs['Vertices'].sv_set([newVerts])
        self.outputs['Edges'].sv_set([newEdges])


def register():
    bpy.utils.register_class(SvSplitEdgesNode)


def unregister():
    bpy.utils.unregister_class(SvSplitEdgesNode)

if __name__ == '__main__':
    register()
