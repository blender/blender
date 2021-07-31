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
from mathutils import Vector

from mathutils.geometry import (
    intersect_line_line,
    intersect_line_line_2d)

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.cad_module_class import CAD_ops
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata

modeItems = [
    ("2D", "2D", "", 0),
    ("3D", "3D", "", 1)]

''' helpers '''

def order_points(edge, point_list):
    ''' order these edges from distance to v1, then
    sandwich the sorted list with v1, v2 '''
    v1, v2 = edge
    dist = lambda co: (v1-co).length
    point_list = sorted(point_list, key=dist)
    return [v1] + point_list + [v2]


def remove_permutations_that_share_a_vertex(cm, bm, permutations):
    ''' Get useful Permutations '''

    final_permutations = []
    for edges in permutations:
        raw_vert_indices = cm.vertex_indices_from_edges_tuple(bm, edges)
        if cm.duplicates(raw_vert_indices):
            continue

        # reaches this point if they do not share.
        final_permutations.append(edges)

    return final_permutations


def get_valid_permutations(cm, bm, edge_indices):
    raw_permutations = itertools.permutations(edge_indices, 2)
    permutations = [r for r in raw_permutations if r[0] < r[1]]
    return remove_permutations_that_share_a_vertex(cm, bm, permutations)


def can_skip(cm, closest_points, vert_vectors):
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
    return (cpa-cpb).length > cm.VTX_PRECISION


def get_intersection_dictionary(cm, bm, edge_indices):

    bm.verts.ensure_lookup_table()
    bm.edges.ensure_lookup_table()

    permutations = get_valid_permutations(cm, bm, edge_indices)

    k = defaultdict(list)
    d = defaultdict(list)

    for edges in permutations:
        vert_vectors = cm.vectors_from_edges_tuple(bm, edges)
        v1, v2, v3, v4 = vert_vectors

        # Edges obviously can not intersect if their bounding
        # boxes do not intersect
        if (max(v1.x, v2.x) < min(v3.x, v4.x) or
            max(v1.y, v2.y) < min(v3.y, v4.y) or
            max(v1.z, v2.z) < min(v3.z, v4.z)):
                continue
        if (max(v3.x, v4.x) < min(v1.x, v2.x) or
            max(v3.y, v4.y) < min(v1.y, v2.y) or
            max(v3.z, v4.z) < min(v1.z, v2.z)):
                continue

        # Edges can not intersect if they do not lie in
        # the same plane
        if not cm.is_coplanar(vert_vectors):
            continue

        points = intersect_line_line(*vert_vectors)

        # some can be skipped.    (NaN, None, not on both edges)
        if can_skip(cm, points, vert_vectors):
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

    for old_edge, point_list in d.items():
        num_edges_to_add = len(point_list)-1
        for i in range(num_edges_to_add):
            a = ov.new(point_list[i])
            b = ov.new(point_list[i+1])
            oe.new((a, b))
            bm.normal_update()


def unselect_nonintersecting(bm, d_edges, edge_indices):
    # print(d_edges, edge_indices)
    if len(edge_indices) > len(d_edges):
        reserved_edges = set(edge_indices) - set(d_edges)
        for edge in reserved_edges:
            bm.edges[edge].select = False
        # print("unselected {}, non intersecting edges".format(reserved_edges))


def intersect_edges_3d(verts_in, edges_in, s_epsilon):

    bm = bmesh_from_pydata(verts_in, edges_in, [])

    edge_indices = [e.index for e in bm.edges]
    trim_indices = len(edge_indices)
    for edge in bm.edges:
        edge.select = True

    cm = CAD_ops(epsilon=s_epsilon)

    d = get_intersection_dictionary(cm, bm, edge_indices)
    unselect_nonintersecting(bm, d.keys(), edge_indices)

    # store non_intersecting edge sequencer
    add_back = [[i.index for i in edge.verts] for edge in bm.edges if not edge.select]

    update_mesh(bm, d)
    verts_out = [v.co.to_tuple() for v in bm.verts]
    edges_out = [[j.index for j in i.verts] for i in bm.edges]

    # optional correction, remove originals, add back those that are not intersecting.
    edges_out = edges_out[trim_indices:]
    edges_out.extend(add_back)
    bm.free()

    return verts_out, edges_out


def edges_from_ed_inter(ed_inter):
    '''create edges from intersecctions library'''
    edges_out = []
    for e in ed_inter:
        # sort by first element of tuple (distances)
        e_s = sorted(e)
        for i in range(1, len(e_s)):
            edges_out.append((e_s[i-1][1], e_s[i][1]))
    return edges_out


def intersect_edges_2d(verts, edges):
    '''Iterate through edges  and expose them to intersect_line_line_2d'''
    verts_in = [Vector(v) for v in verts]
    ed_lengths = [(verts_in[e[1]] - verts_in[e[0]]).length for e in edges]
    verts_out = verts
    edges_out = []
    ed_inter = [[] for e in edges]
    for e, d, i in zip(edges, ed_lengths, range(len(edges))):
        # if there is no intersections this will create a normal edge
        ed_inter[i].append([0.0, e[0]])
        ed_inter[i].append([d, e[1]])
        v1 = verts_in[e[0]]
        v2 = verts_in[e[1]]
        if d == 0:
            continue

        for e2, d2, j in zip(edges, ed_lengths, range(len(edges))):

            if i <= j or d2 == 0:
                continue
            if (e2[0] in e) or (e2[1] in e):
                continue

            v3 = verts_in[e2[0]]
            v4 = verts_in[e2[1]]
            vx = intersect_line_line_2d(v1, v2, v3, v4)
            if vx:
                d_to_1 = (vx - v1.to_2d()).length
                d_to_2 = (vx - v3.to_2d()).length

                new_id = len(verts_out)
                # first item stores distance to origin, second the vertex id
                ed_inter[i].append([d_to_1, new_id])
                ed_inter[j].append([d_to_2, new_id])
                verts_out.append((vx.x, vx.y, v1.z))

    edges_out = edges_from_ed_inter(ed_inter)

    return verts_out, edges_out


def remove_doubles_from_edgenet(verts_in, edges_in, distance):
    bm = bmesh_from_pydata(verts_in, edges_in, [])
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=distance)
    verts_out = [v.co.to_tuple() for v in bm.verts]
    edges_out = [[j.index for j in i.verts] for i in bm.edges]

    return verts_out, edges_out


class SvIntersectEdgesNodeMK2(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvIntersectEdgesNodeMK2'
    bl_label = 'Intersect Edges MK2'
    sv_icon = 'SV_XALL'

    mode = bpy.props.EnumProperty(items=modeItems, default="3D", update=updateNode)
    rm_switch = bpy.props.BoolProperty(update=updateNode)
    rm_doubles = bpy.props.FloatProperty(min=0.0, default=0.0001, step=0.1, update=updateNode)
    epsilon = bpy.props.FloatProperty(min=1.0e-5, default=1.0e-5, step=0.02, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Verts_in')
        self.inputs.new('StringsSocket', 'Edges_in')

        self.outputs.new('VerticesSocket', 'Verts_out')
        self.outputs.new('StringsSocket', 'Edges_out')

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode", expand=True)
        r = layout.row(align=True)
        r1 = r.split(0.32)
        r1.prop(self, 'rm_switch', text='doubles', toggle=True)
        r2 = r1.split()
        r2.enabled = self.rm_switch
        r2.prop(self, 'rm_doubles', text='delta')

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, 'epsilon')

    def process(self):
        inputs = self.inputs
        outputs = self.outputs

        try:
            verts_in = inputs['Verts_in'].sv_get(deepcopy=False)[0]
            edges_in = inputs['Edges_in'].sv_get(deepcopy=False)[0]
            linked = outputs['Verts_out'].is_linked
        except (IndexError, KeyError) as e:
            return

        if self.mode == "3D":
            verts_out, edges_out = intersect_edges_3d(verts_in, edges_in, self.epsilon)
        else:
            verts_out, edges_out = intersect_edges_2d(verts_in, edges_in)

        # post processing step to remove doubles
        if self.rm_switch:
            verts_out, edges_out = remove_doubles_from_edgenet(verts_out, edges_out, self.rm_doubles)

        outputs['Verts_out'].sv_set([verts_out])
        outputs['Edges_out'].sv_set([edges_out])


def register():
    bpy.utils.register_class(SvIntersectEdgesNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvIntersectEdgesNodeMK2)
