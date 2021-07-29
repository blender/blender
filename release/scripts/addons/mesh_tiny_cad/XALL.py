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

# <pep8 compliant>


import bpy
import bmesh
from mathutils.geometry import intersect_line_line as LineIntersect

import itertools
from collections import defaultdict
from . import cad_module as cm


def order_points(edge, point_list):
    ''' order these edges from distance to v1, then
    sandwich the sorted list with v1, v2 '''
    v1, v2 = edge

    def dist(co):
        return (v1 - co).length
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
    return (cpa - cpb).length > cm.CAD_prefs.VTX_PRECISION


def get_intersection_dictionary(bm, edge_indices):

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
    ''' Make new geometry (delete old first) '''

    oe = bm.edges
    ov = bm.verts

    new_verts = []
    collect = new_verts.extend
    for old_edge, point_list in d.items():
        num_edges_to_add = len(point_list)-1
        for i in range(num_edges_to_add):
            a = ov.new(point_list[i])
            b = ov.new(point_list[i+1])
            oe.new((a, b))
            bm.normal_update()
            collect([a, b])

    bmesh.ops.delete(bm, geom=[edge for edge in bm.edges if edge.select], context=2) # 2 = edges

    #bpy.ops.mesh.remove_doubles(
    #    threshold=cm.CAD_prefs.VTX_DOUBLES_THRSHLD,
    #    use_unselected=False)

    bmesh.ops.remove_doubles(bm, verts=new_verts, dist=cm.CAD_prefs.VTX_DOUBLES_THRSHLD)


def unselect_nonintersecting(bm, d_edges, edge_indices):
    if len(edge_indices) > len(d_edges):
        reserved_edges = set(edge_indices) - set(d_edges)
        for edge in reserved_edges:
            bm.edges[edge].select = False
        print("unselected {}, non intersecting edges".format(reserved_edges))


class TCIntersectAllEdges(bpy.types.Operator):
    '''Adds a vertex at the intersections of all selected edges'''
    bl_idname = 'tinycad.intersectall'
    bl_label = 'XALL intersect all edges'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.type == 'MESH' and obj.mode == 'EDIT'

    def execute(self, context):
        # must force edge selection mode here
        bpy.context.tool_settings.mesh_select_mode = (False, True, False)

        obj = context.active_object
        if obj.mode == "EDIT":
            bm = bmesh.from_edit_mesh(obj.data)

            selected_edges = [edge for edge in bm.edges if edge.select]
            edge_indices = [i.index for i in selected_edges]

            d = get_intersection_dictionary(bm, edge_indices)

            unselect_nonintersecting(bm, d.keys(), edge_indices)
            update_mesh(bm, d)

            bmesh.update_edit_mesh(obj.data)
        else:
            print('must be in edit mode')

        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)
