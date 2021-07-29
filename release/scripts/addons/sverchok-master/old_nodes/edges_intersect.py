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

import itertools
from collections import defaultdict

import bpy
import bmesh
from mathutils.geometry import intersect_line_line as LineIntersect

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.utils import cad_module as cm
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata

''' helpers '''


def order_points(edge, point_list):
    ''' order these edges from distance to v1, then
    sandwich the sorted list with v1, v2 '''
    v1, v2 = edge
    dist = lambda co: (v1-co).length
    point_list = sorted(point_list, key=dist)
    return [v1] + point_list + [v2]


def remove_permutations_that_share_a_vertex(bm, permutations):
    ''' Get useful Permutations '''

    final_permutations = []
    for edges in permutations:
        raw_vert_indices = cm.vertex_indices_from_edges_tuple(bm, edges)
        if cm.duplicates(raw_vert_indices):
            continue

        # reaches this point if they do not share.
        final_permutations.append(edges)

    return final_permutations


def get_valid_permutations(bm, edge_indices):
    raw_permutations = itertools.permutations(edge_indices, 2)
    permutations = [r for r in raw_permutations if r[0] < r[1]]
    return remove_permutations_that_share_a_vertex(bm, permutations)


def can_skip(closest_points, vert_vectors):
    '''this checks if the intersection lies on both edges, returns True
    when criteria are not met, and thus this point can be skipped'''
    if not closest_points:
        return True
    if not isinstance(closest_points[0].x, float):
        return True
    if cm.num_edges_point_lies_on(closest_points[0], vert_vectors) < 2:
        return True

    # if this distance is larger than than VTX_PRECISION, we can skip it.
    cpa, cpb = closest_points
    return (cpa-cpb).length > cm.CAD_prefs.VTX_PRECISION


def get_intersection_dictionary(bm, edge_indices):

    if hasattr(bm.verts, "ensure_lookup_table"):
        bm.verts.ensure_lookup_table()
        bm.edges.ensure_lookup_table()

    permutations = get_valid_permutations(bm, edge_indices)

    k = defaultdict(list)
    d = defaultdict(list)

    for edges in permutations:
        raw_vert_indices = cm.vertex_indices_from_edges_tuple(bm, edges)
        vert_vectors = cm.vectors_from_indices(bm, raw_vert_indices)

        points = LineIntersect(*vert_vectors)

        # some can be skipped.    (NaN, None, not on both edges)
        if can_skip(points, vert_vectors):
            continue

        # reaches this point only when an intersection happens on both edges.
        [k[edge].append(points[0]) for edge in edges]

    # k will contain a dict of edge indices and points found on those edges.
    for edge_idx, unordered_points in k.items():
        tv1, tv2 = bm.edges[edge_idx].verts
        v1 = bm.verts[tv1.index].co
        v2 = bm.verts[tv2.index].co
        ordered_points = order_points((v1, v2), unordered_points)
        d[edge_idx].extend(ordered_points)

    return d


def update_mesh(bm, d):
    ''' Make new geometry '''

    oe = bm.edges
    ov = bm.verts

    vert_count = len(ov)
    edge_count = len(oe)

    for old_edge, point_list in d.items():
        num_points = len(point_list)
        num_edges_to_add = num_points-1

        for i in range(num_edges_to_add):
            ov.new(point_list[i])
            ov.new(point_list[i+1])

            bm.verts.ensure_lookup_table()
            bm.edges.ensure_lookup_table()

            vseq = ov[vert_count], ov[vert_count+1]
            oe.new(vseq)
            bm.normal_update()
            vert_count = len(ov)
            edge_count = len(oe)

    # offer a remove doubles pass here.
    # bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=node.dist)


def unselect_nonintersecting(bm, d_edges, edge_indices):
    # print(d_edges, edge_indices)
    if len(edge_indices) > len(d_edges):
        reserved_edges = set(edge_indices) - set(d_edges)
        for edge in reserved_edges:
            bm.edges[edge].select = False
        # print("unselected {}, non intersecting edges".format(reserved_edges))


class SvIntersectEdgesNode(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvIntersectEdgesNode'
    bl_label = 'Intersect Edges'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Verts_in', 'Verts_in')
        self.inputs.new('StringsSocket', 'Edges_in', 'Edges_in')

        self.outputs.new('VerticesSocket', 'Verts_out', 'Verts_out')
        self.outputs.new('StringsSocket', 'Edges_out', 'Edges_out')

    def draw_buttons(self, context, layout):
        pass

    def process(self):
        inputs = self.inputs
        outputs = self.outputs

        try:
            verts_in = inputs['Verts_in'].sv_get()[0]
            edges_in = inputs['Edges_in'].sv_get()[0]
            linked = outputs['Verts_out'].is_linked
        except (IndexError, KeyError) as e:
            return

        bm = bmesh_from_pydata(verts_in, edges_in, [])

        edge_indices = [e.index for e in bm.edges]
        trim_indices = len(edge_indices)
        for edge in bm.edges:
            edge.select = True

        d = get_intersection_dictionary(bm, edge_indices)

        unselect_nonintersecting(bm, d.keys(), edge_indices)

        # store non_intersecting edge sequencer
        add_back = [[i.index for i in edge.verts] for edge in bm.edges if not edge.select]

        update_mesh(bm, d)

        verts_out = [v.co.to_tuple() for v in bm.verts]
        edges_out = [[j.index for j in i.verts] for i in bm.edges]

        # optional correction, remove originals, add back those that are not intersecting.
        edges_out = edges_out[trim_indices:]
        edges_out.extend(add_back)

        outputs['Verts_out'].sv_set([verts_out])
        outputs['Edges_out'].sv_set([edges_out])


def register():
    bpy.utils.register_class(SvIntersectEdgesNode)


def unregister():
    bpy.utils.unregister_class(SvIntersectEdgesNode)
