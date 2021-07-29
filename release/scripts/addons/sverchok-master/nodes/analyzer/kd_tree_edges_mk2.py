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

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


# documentation/blender_python_api_2_70_release/mathutils.kdtree.html
class SvKDTreeEdgesNodeMK2(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvKDTreeEdgesNodeMK2'
    bl_label = 'KDT Closest Edges MK2'
    bl_icon = 'OUTLINER_OB_EMPTY'

    mindist = FloatProperty(
        name='mindist', description='Minimum dist', min=0.0,
        default=0.1, update=updateNode)

    maxdist = FloatProperty(
        name='maxdist', description='Maximum dist', min=0.0,
        default=2.0, update=updateNode)

    maxNum = IntProperty(
        name='maxNum', description='max edge count',
        default=4, min=1, update=updateNode)

    skip = IntProperty(
        name='skip', description='skip first n',
        default=0, min=0, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Verts')
        self.inputs.new('StringsSocket', 'mindist').prop_name = 'mindist'
        self.inputs.new('StringsSocket', 'maxdist').prop_name = 'maxdist'
        self.inputs.new('StringsSocket', 'maxNum').prop_name = 'maxNum'
        self.inputs.new('StringsSocket', 'skip').prop_name = 'skip'

        self.outputs.new('StringsSocket', 'Edges')

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
        # documentation/blender_python_api_2_78_release/mathutils.kdtree.html
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

        for i, vtx in enumerate(verts):
            num_edges = 0

            # this always returns closest first followed by next closest, etc.
            #              co  index  dist
            for edge_idx, (_, index, dist) in enumerate(kd.find_range(vtx, abs(maxdist))):

                if skip > 0:
                    if edge_idx < skip:
                        continue

                if (dist <= abs(mindist)) or (i == index):
                    continue

                edge = tuple(sorted([i, index]))
                if not edge in e:
                    e.add(edge)
                    num_edges += 1

                if num_edges == maxNum:
                    break


        self.outputs['Edges'].sv_set([list(e)])


def register():
    bpy.utils.register_class(SvKDTreeEdgesNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvKDTreeEdgesNodeMK2)
