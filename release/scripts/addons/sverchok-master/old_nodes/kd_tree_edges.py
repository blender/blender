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
from bpy.props import IntProperty, FloatProperty
import mathutils
from mathutils import Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, SvSetSocketAnyType, SvGetSocketAnyType


# documentation/blender_python_api_2_70_release/mathutils.kdtree.html
class SvKDTreeEdgesNode(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvKDTreeEdgesNode'
    bl_label = 'KDT Closest Edges'
    bl_icon = 'OUTLINER_OB_EMPTY'

    mindist = FloatProperty(
        name='mindist', description='Minimum dist',
        default=0.1, update=updateNode)

    maxdist = FloatProperty(
        name='maxdist', description='Maximum dist',
        default=2.0, update=updateNode)

    maxNum = IntProperty(
        name='maxNum', description='max edge count',
        default=4, min=1, update=updateNode)

    skip = IntProperty(
        name='skip', description='skip first n',
        default=0, min=0, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Verts', 'Verts')
        self.inputs.new('StringsSocket', 'mindist', 'mindist').prop_name = 'mindist'
        self.inputs.new('StringsSocket', 'maxdist', 'maxdist').prop_name = 'maxdist'
        self.inputs.new('StringsSocket', 'maxNum', 'maxNum').prop_name = 'maxNum'
        self.inputs.new('StringsSocket', 'skip', 'skip').prop_name = 'skip'

        self.outputs.new('StringsSocket', 'Edges', 'Edges')

    def process(self):
        inputs = self.inputs
        outputs = self.outputs

        try:
            verts = inputs['Verts'].sv_get()[0]
            linked = outputs['Edges'].is_linked
        except (IndexError, KeyError) as e:
            return

        optional_sockets = [
            ['mindist', self.mindist, float],
            ['maxdist', self.maxdist, float],
            ['maxNum', self.maxNum, int],
            ['skip', self.skip, int]]

        socket_inputs = []
        for s, s_default_value, dtype in optional_sockets:
            if s in inputs and inputs[s].is_linked:
                sock_input = dtype(inputs[s].sv_get()[0][0])
            else:
                sock_input = s_default_value
            socket_inputs.append(sock_input)

        self.run_kdtree(verts, socket_inputs)

    def run_kdtree(self, verts, socket_inputs):
        mindist, maxdist, maxNum, skip = socket_inputs

        # make kdtree
        # documentation/blender_python_api_2_70_release/mathutils.kdtree.html
        size = len(verts)
        kd = mathutils.kdtree.KDTree(size)

        for i, xyz in enumerate(verts):
            kd.insert(xyz, i)
        kd.balance()

        # set minimum values
        maxNum = max(maxNum, 1)
        skip = max(skip, 0)

        # makes edges
        e = set()
        mcount = 0
        for i, vtx in enumerate(verts):
            num_edges = 0
            for (co, index, dist) in kd.find_range(vtx, maxdist):
                if (dist <= mindist) or (i == index):
                    continue
                if (num_edges > maxNum):
                    break
                if num_edges <= skip:
                    num_edges += 1
                    continue

                e.add(tuple(sorted([i, index])))
                mcount += 1
                num_edges += 1

        # print(len(e), 'vs', mcount)
        self.outputs['Edges'].sv_set([list(e)])


def register():
    bpy.utils.register_class(SvKDTreeEdgesNode)


def unregister():
    bpy.utils.unregister_class(SvKDTreeEdgesNode)
