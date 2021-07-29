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

import random

import bpy
import bmesh
from mathutils import Vector
from mathutils.kdtree import KDTree
from mathutils.bvhtree import BVHTree
from mathutils.noise import seed_set, random_unit_vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata


def generate_random_unitvectors():
    # may need many more directions to increase accuracy
    # generate up to 6 directions, filter later
    seed_set(140230)
    return [random_unit_vector() for i in range(6)]

directions = generate_random_unitvectors()


def get_points_in_mesh(points, verts, faces, eps=0.0, num_samples=3):
    mask_inside = []

    bvh = BVHTree.FromPolygons(verts, faces, all_triangles=False, epsilon=eps)

    for direction in directions[:num_samples]:
        samples = []
        mask = samples.append
        
        for point in points:
            hit = bvh.ray_cast(point, direction)
            if hit[0]:
                v = hit[1].dot(direction)
                mask(not v < 0.0)
            else:
                mask(False)

        mask_inside.append(samples)
    
    if len(mask_inside) == 1:
        return mask_inside[0]
    else:
        mask_totals = []
        oversample = mask_totals.append
        num_points = len(points)

        # exactly what the criteria should be here is not clear, this seems enough.
        for i in range(num_points):
            fsum = sum(mask_inside[j][i] for j in range(num_samples))

            if num_samples == 2:
                oversample(fsum >= 1)
            elif num_samples == 3:
                oversample(fsum >= 2)
            elif num_samples == 4:
                oversample(fsum >= 3)
            elif num_samples == 5:
                oversample(fsum >= 4)
            elif num_samples == 6:
                oversample(fsum >= 4)
        return mask_totals       


def are_inside(points, bm):
    mask_inside = []
    mask = mask_inside.append
    bvh = BVHTree.FromBMesh(bm, epsilon=0.0001)
 
    # return points on polygons
    for point in points:
        fco, normal, _, _ = bvh.find_nearest(point)
        p2 = fco - Vector(point)
        v = p2.dot(normal)
        mask(not v < 0.0)  # addp(v >= 0.0) ?
    
    return mask_inside


class SvPointInside(bpy.types.Node, SverchCustomTreeNode):
    ''' pin get points inside mesh '''
    bl_idname = 'SvPointInside'
    bl_label = 'Points Inside Mesh'

    mode_options = [(k, k, '', i) for i, k in enumerate(["algo 1", "algo 2"])]
    
    selected_algo = bpy.props.EnumProperty(
        items=mode_options,
        description="offers different approaches to finding internal points",
        default="algo 1", update=updateNode
    )
    epsilon_bvh = bpy.props.FloatProperty(update=updateNode, default=0.0, min=0.0, max=1.0, description="fudge value")
    num_samples = bpy.props.IntProperty(min=1, max=6, default=3)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'verts')
        self.inputs.new('StringsSocket', 'faces')
        self.inputs.new('VerticesSocket', 'points')
        self.outputs.new('StringsSocket', 'mask')
        self.outputs.new('VerticesSocket', 'verts')

    def draw_buttons(self, context, layout):

        layout.prop(self, 'selected_algo', expand=True)
        if self.selected_algo == 'algo 2':
            layout.prop(self, 'epsilon_bvh', text='Epsilon')
            layout.prop(self, 'num_samples', text='Samples')


    def process(self):

        if not all(socket.is_linked for socket in self.inputs):
            return

        verts_in, faces_in, points = [s.sv_get() for s in self.inputs]
        mask = []

        for idx, (verts, faces, pts_in) in enumerate(zip(verts_in, faces_in, points)):
            if self.selected_algo == 'algo 1':
                bm = bmesh_from_pydata(verts, [], faces, normal_update=True)
                mask.append(are_inside(pts_in, bm))
            elif self.selected_algo == 'algo 2':
                mask.append(
                    get_points_in_mesh(pts_in, verts, faces, self.epsilon_bvh, self.num_samples)
                )

        self.outputs['mask'].sv_set(mask)
        if self.outputs['verts'].is_linked:
            out_verts = []
            for masked, pts_in in zip(mask, points):
                out_verts.append([p for m, p in zip(masked, pts_in) if m])
            self.outputs['verts'].sv_set(out_verts)


def register():
    bpy.utils.register_class(SvPointInside)


def unregister():
    bpy.utils.unregister_class(SvPointInside)
