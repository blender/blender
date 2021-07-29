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
import numpy as np
from math import sin, cos, pi, atan2, asin, ceil
import bpy
from bpy.props import IntProperty, FloatProperty, EnumProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (
    match_long_repeat,
    updateNode, match_long_cycle)
from sverchok.utils.modules.geom_utils import (
    pt_in_triangle,
    length_v2)
from sverchok.utils.sv_mesh_utils import mesh_join
from sverchok.nodes.modifier_change.edges_intersect_mk2 import (
    remove_doubles_from_edgenet,
    intersect_edges_2d)


mode_items = [
    ("Constant", "Constant", "Many contours on many distances", 0),
    ("Weighted", "Weighted", "One distance per each vertex  ", 1)]

list_match_Items = [
    ("Long Repeat", "Long Repeat", "Repeat last item until match the longest list", 0),
    ("Long Cycle", "Long Cycle", "Cycle through the shorter lists until match the longest list", 1)]

intersec_mode_items = [
    ("Circular", "Circular", "Intersecction based on distance (Slower)", 0),
    ("Poligonal", "Poligonal", "Intersecction dependent from num. of vertices (Faster)", 1)]


def check_dist_to_verts(v, or_verts, or_radius, net, poli_ang_list, modulo, mask_t):
    d = 0
    min_dist = 2.0e-5
    for j in range(modulo):
        rad = or_radius[j]
        if rad < min_dist:
            continue
        vo = or_verts[j]
        vfx = v[0]-vo[0]
        vfy = v[1]-vo[1]
        # dist_to_point = Vector((vfx, vfy)).length
        dist_to_point = length_v2((vfx, vfy))
        if dist_to_point > rad:
            continue

        polig_ang = poli_ang_list[j]
        offset = net[j][1][0]
        if len(net[j][0]) == 3:
            offset += net[j][0][0] - net[j][0][1]
        v_vo_ang = (normal_angle(atan2(vfy, vfx) - offset) % (2 * polig_ang)) - polig_ang
        rad_r = rad * cos(polig_ang)
        dist_lim = (rad_r / cos(v_vo_ang)) * (1 - mask_t)

        if dist_to_point < dist_lim:
            d = 1
            break
    return d


def check_dist_to_edges(v, or_verts, or_radius, edges, sides_space):
    d = 0
    min_dist = 2.0e-5
    for ed, s in zip(edges, sides_space):
        v1 = or_verts[ed[0]]
        v2 = or_verts[ed[1]]
        r1 = or_radius[ed[0]]
        r2 = or_radius[ed[1]]
        if v1 == v and r1 < min_dist or v2 == v and r2 < min_dist or r1 < min_dist and r2 < min_dist:
            continue

        in_tirangle_a = pt_in_triangle(v, s[2], s[0], s[1])
        in_tirangle_b = pt_in_triangle(v, s[2], s[1], s[3])

        if in_tirangle_a or in_tirangle_b:
            d = 1
            break
    return d


def sides_space_limits(or_verts, or_radius, net, edges, mask_t, modulo):
    net_offset_count = [0 for i in range(modulo)]
    sides_space = []
    for ed in edges:
        v1 = or_verts[ed[0]]
        v2 = or_verts[ed[1]]
        r1 = or_radius[ed[0]]
        r2 = or_radius[ed[1]]
        beta = net[ed[0]][0]

        dist_lim1 = r1 * (1 - mask_t)
        dist_lim2 = r2 * (1 - mask_t)

        net_offset = net_offset_count[ed[0]]
        net_offset_count[ed[0]] += 3
        net_offset_count[ed[1]] += 3
        vect_lim1a = [v1[0] + dist_lim1 * cos(beta[1 + net_offset]), v1[1] + dist_lim1 * sin(beta[1 + net_offset]), v1[2]]
        vect_lim1b = [v1[0] + dist_lim1 * cos(beta[2 + net_offset]), v1[1] + dist_lim1 * sin(beta[2 + net_offset]), v1[2]]
        vect_lim2a = [v2[0] + dist_lim2 * cos(beta[1 + net_offset]), v2[1] + dist_lim2 * sin(beta[1 + net_offset]), v2[2]]
        vect_lim2b = [v2[0] + dist_lim2 * cos(beta[2 + net_offset]), v2[1] + dist_lim2 * sin(beta[2 + net_offset]), v2[2]]
        sides_space.append([vect_lim1a, vect_lim1b, vect_lim2a, vect_lim2b])
    return sides_space


def mask_by_distance(verts, parameters, modulo, edges, mask_t):
    or_verts = parameters[0]
    or_radius = parameters[2]
    or_vert_num = parameters[1]
    net = parameters[3]
    mask = []

    sides_space = sides_space_limits(or_verts, or_radius, net, edges, mask_t, modulo)
    poli_ang_list = [pi/v for v in or_vert_num]

    for i in range(len(verts)):
        v = verts[i]
        d = check_dist_to_verts(v, or_verts, or_radius, net, poli_ang_list, modulo, mask_t)
        if d == 0:
            d = check_dist_to_edges(v, or_verts, or_radius, edges, sides_space)

        mask.append(0 if d > 0 else 1)

    return mask


def mask_vertices(verts, edges, mask):
    # function taken from vertices_mask.py
    verts_out = []
    edges_out = []
    ve, pe, ma = verts, edges, mask

    current_mask = itertools.islice(itertools.cycle(ma), len(ve))
    vert_index = [i for i, m in enumerate(current_mask) if m]
    if len(vert_index) < len(ve):
        index_set = set(vert_index)
        vert_dict = {j: i for i, j in enumerate(vert_index)}
        new_vert = [ve[i] for i in vert_index]
        is_ss = index_set.issuperset
        new_pe = [[vert_dict[n] for n in fe]
                  for fe in pe if is_ss(fe)]
        verts_out.append(new_vert)
        edges_out.append(new_pe)

    else:  # no reprocessing needed
        verts_out.append(ve)
        edges_out.append(pe)
    return verts_out, edges_out


def calculate_mid_points(verts, edges):

    np_verts = np.array(verts)
    np_ed_vert = np_verts[edges, :]
    np_sum = np_ed_vert.sum(axis=1)*0.5
    return np_sum.tolist()


def mask_edges(edges, mask):

    return [ed for m, ed in zip(mask, edges) if m]


def sort_verts_by_connexions(verts_in, edges_in):
    verts_out = []
    edges_out = []

    edges_length = len(edges_in)
    edges_index = [j for j in range(edges_length)]
    edges0 = [j[0] for j in edges_in]
    edges1 = [j[1] for j in edges_in]
    edge_index = 0
    edge_direction = 0
    edges_copy = [edges0, edges1, edges_index]

    for co in edges_copy:
        co.pop(0)

    for j in range(edges_length):
        e = edges_in[edge_index]
        ed = []
        if edge_direction == 1:
            e = [e[1], e[0]]

        for side in e:
            if verts_in[side] in verts_out:
                ed.append(verts_out.index(verts_in[side]))
            else:
                verts_out.append(verts_in[side])
                ed.append(verts_out.index(verts_in[side]))

        edges_out.append(ed)

        edges_index_old = edge_index
        v_index = e[1]
        if v_index in edges_copy[0]:
            k = edges_copy[0].index(v_index)
            edge_index = edges_copy[2][k]
            edge_direction = 0

            for co in edges_copy:
                co.pop(k)

        elif v_index in edges_copy[1]:
            k = edges_copy[1].index(v_index)
            edge_index = edges_copy[2][k]
            edge_direction = 1
            for co in edges_copy:
                co.pop(k)

        if edge_index == edges_index_old and len(edges_copy[0]) > 0:
            edge_index = edges_copy[2][0]
            edge_direction = 0

            for co in edges_copy:
                co.pop(0)

    return verts_out, edges_out


def orientation_angle(net2):
    for net_v in net2:
        connex = len(net_v[0])/3
        v_angle = 0
        for i in range(0, int(len(net_v[0])), 3):
            v_angle += net_v[0][i] / connex
        net_v[1].append(v_angle if connex != 1 else normal_angle(v_angle - pi))


def side_edges_angles(net2, verts_in, edges_in, radius):
    for ed in edges_in:
        s0 = ed[0]
        s1 = ed[1]
        if s0 > len(verts_in) - 1 or s1 > len(verts_in) - 1:
            break
        v0 = verts_in[s0]
        v1 = verts_in[s1]
        vfx = v1[0]-v0[0]
        vfy = v1[1]-v0[1]
        an = atan2(vfy, vfx) + pi
        an2 = an + pi
        r1 = radius[s0]
        r2 = radius[s1]
        dist = length_v2((vfx, vfy))
        beta = asin(min(max((r1-r2)/dist, -1), 1))

        net2[s0][0].append(normal_angle(an + 0.5*pi))
        net2[s0][0].append(normal_angle(an + 0.5*pi + beta))
        net2[s0][0].append(normal_angle(an + 1.5*pi - beta))
        net2[s1][0].append(normal_angle(an2 + 0.5*pi))
        net2[s1][0].append(normal_angle(an2 + 0.5*pi - beta))
        net2[s1][0].append(normal_angle(an2 + 1.5*pi + beta))


def cross_indices(n, edges_in):
    '''create crossed indices and subtract existing edges'''
    index = []
    for i in range(n-1):
        for j in range(i+1, n):
            index.append([i, j])
    if edges_in:
        for e in edges_in:
            if e in index:
                index.remove(e)
            elif [e[1], e[0]] in index:
                index.remove([e[1], e[0]])

    return index


def ciruclar_intersections(net2, verts, edges_in, v_len, radius):

    np_rad = np.array(radius)
    np_verts = np.array(verts)
    indexes = np.array(cross_indices(v_len, edges_in))

    p_rads = np_rad[indexes]
    pairs = np_verts[indexes, :]
    dif_v = pairs[:, 0, :] - pairs[:, 1, :]
    sum_rad = p_rads[:, 0] + p_rads[:, 1]
    dif_rad = abs(p_rads[:, 0] - p_rads[:, 1])
    dist = np.linalg.norm(dif_v, axis=1)

    mask = sum_rad > dist
    mask *= dif_rad < dist
    mask *= 0 < dist

    p_rads = p_rads[mask, :]
    index_inter = indexes[mask]

    dist_v = dist[mask]
    vec = dif_v[mask]
    rad = p_rads[:, 0]
    rad2 = p_rads[:, 1]
    ang_base = (np.arctan2(vec[:, 1], vec[:, 0]) + 2*pi) % (2*pi)

    a = rad*rad - rad2*rad2 + dist_v*dist_v
    a /= 2*dist_v
    h = np.sqrt(rad*rad - a*a)
    ang = np.arcsin(h / rad)

    a2 = rad2*rad2 - rad*rad + dist_v*dist_v
    a2 /= 2*dist_v
    h2 = np.sqrt(rad2*rad2 - a2*a2)
    ang2 = np.arcsin(h2 / rad2)

    p1 = ang_base - ang - pi
    p1b = ang_base + ang - pi
    p2 = ang_base - ang2
    p2b = ang_base + ang2

    p1 = p1.tolist()
    p1b = p1b.tolist()
    p2 = p2.tolist()
    p2b = p2b.tolist()
    index_inter = index_inter.tolist()
    p_rads = p_rads.tolist()
    for e, an1, an1b, an2, an2b, r in zip(index_inter, p1, p1b, p2, p2b, p_rads):
        e0 = e[0]
        e1 = e[1]
        net2[e0][2].append([normal_angle(an1), r[0], 1])
        net2[e0][2].append([normal_angle(an1b), r[0], 1])
        net2[e1][2].append([normal_angle(an2), r[1], 1])
        net2[e1][2].append([normal_angle(an2b), r[1], 1])
    return net2


def list_matcher(a, list_match):

    if list_match == "Long Cycle":
        return match_long_cycle(a)
    else:
        return match_long_repeat(a)


def normal_angle(a):
    return (a + 8*pi) % (2*pi)


def outside_angles(ang, ang_o, ang_f):

    if (ang_o > ang_f):
        return ang >= ang_f and ang <= ang_o
    else:
        return ang <= ang_o or ang >= ang_f


def on_valid_angle_inter(ang, intersecctions):
    out_side = True
    for i in range(int(len(intersecctions)/2)):
        out_side = out_side and outside_angles(ang, intersecctions[2*i][0], intersecctions[2*i+1][0])
        if not out_side:
            break

    return out_side


def on_valid_angle_connex(ang, net, connex):
    out_side = True
    for i in range(int(connex)):
        out_side = out_side and outside_angles(ang, net[3*i+1], net[3*i+2])
        if not out_side:
            break
    return out_side


def create_valid_vert_edges(x, y, z, new_angs, intersecctions, net, connex):
    list_vert_x, list_vert_y, list_vert_z = [], [], []
    edg_list = []
    ed_ind = 0
    inter_list = []
    last_is_inter = 0
    last_ang = 0

    for ang_local, r, inter in new_angs:
        out_side = on_valid_angle_inter(ang_local, intersecctions)
        if out_side and connex > 1:
            out_side = on_valid_angle_connex(ang_local, net, connex)

        if out_side:
            if last_is_inter and inter:
                mid_ang = normal_angle(last_ang + (ang_local - last_ang)*0.5)
                if not on_valid_angle_inter(mid_ang, intersecctions)or not on_valid_angle_connex(mid_ang, net, connex):
                    edg_list.pop()

            last_is_inter = inter
            last_ang = ang_local
            edg_list.append((ed_ind, ed_ind+1))
            inter_list.append([inter, ang_local])
            ed_ind += 1
            list_vert_x.append(x + r * cos(ang_local))
            list_vert_y.append(y + r * sin(ang_local))
            list_vert_z.append(z)

    if len(inter_list) > 0:
        if inter_list[-1][0] and inter_list[0][0]:
            mid_ang = normal_angle(inter_list[-1][1] + (inter_list[0][1] + 2*pi - inter_list[-1][1]) * 0.5)
            if not on_valid_angle_inter(mid_ang, intersecctions)or not on_valid_angle_connex(mid_ang, net, connex):
                edg_list.pop()
            else:
                edg_list[-1] = (edg_list[-1][0], 0)
        else:
            edg_list[-1] = (edg_list[-1][0], int(0))

    return list_vert_x, list_vert_y, list_vert_z, edg_list


class SvContourNode(bpy.types.Node, SverchCustomTreeNode):
    '''C2 Offset vert_line'''
    bl_idname = 'SvContourNode'
    bl_label = 'Contour 2D'
    bl_icon = 'FORCE_FORCE'

    modeI = EnumProperty(
        name="modeI",
        description="Type of contour when multiple distances are given",
        items=mode_items, default="Constant",
        update=updateNode)

    list_match = EnumProperty(
        name="list_match",
        description="Behaviour on diffent list lengths",
        items=list_match_Items, default="Long Repeat",
        update=updateNode)

    rm_doubles = FloatProperty(
        name='R. Doubles',
        description="Remove Doubles Distance",
        min=0.0, default=0.0001,
        step=0.1, update=updateNode)

    mask_t = FloatProperty(
        name='Mask tolerance',
        description="Mask tolerance",
        min=-1.0, default=1.0e-5,
        step=0.02, update=updateNode)

    intersecction_handle = EnumProperty(
        name="intersecction_handle",
        description="Intersecction mode",
        items=intersec_mode_items, default="Circular",
        update=updateNode)

    rad_ = FloatProperty(
        name='Distance', description='Contour distance',
        default=1.0, min=1.0e-5, update=updateNode)

    vert_ = IntProperty(
        name='N Vertices', description='Nº of Vertices per input vector',
        default=24, min=4, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Distance").prop_name = 'rad_'
        self.inputs.new('StringsSocket', "Nº Vertices").prop_name = 'vert_'
        self.inputs.new('VerticesSocket', "Verts_in")
        self.inputs.new('StringsSocket', "Edges_in")

        self.outputs.new('VerticesSocket', "Vertices", "Vertices")
        self.outputs.new('StringsSocket', "Edges", "Edges")

    def draw_buttons(self, context, layout):
        layout.prop(self, "modeI", expand=True)
        layout.prop(self, 'rm_doubles')

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, "modeI", expand=True)
        layout.prop(self, 'rm_doubles')
        layout.prop(self, 'mask_t')
        layout.prop(self, "intersecction_handle", expand=True)
        layout.prop(self, "list_match", expand=True)


    def build_net(self, verts_in, edges_in, v_len, radius, poligonal_inter):
        '''calculate radial intersections and connexion angles and orientations'''
        net2 = []
        for j in range(v_len):
            connect2 = [[], [], [], []]
            net2.append(connect2)

        # calculate side_edges angles
        if edges_in:
            side_edges_angles(net2, verts_in, edges_in, radius)
        # calculate orientation
        orientation_angle(net2)
        if not poligonal_inter and v_len > 1:
            # calculate circular intersections
            ciruclar_intersections(net2, verts_in, edges_in, v_len, radius)

        return net2

    def make_verts(self, center, vertices, radius, net_full):
        list_vert_x = []
        list_vert_y = []
        list_vert_z = []
        edg_list = []
        x = center[0]
        y = center[1]
        z = center[2]
        intersec = []
        v_angle = net_full[1][0]
        net = net_full[0]
        connex = len(net)/3
        intersec = net_full[2]
        theta = 2*pi/vertices
        vert = vertices

        if connex > 1:
            net_all = []
            for j in range(0, len(net)):
                ind = j % 3
                if ind != 0:
                    beta = net[j]
                    net_all.append([beta, radius, 1])

            new_angs = [[normal_angle((theta * i) + v_angle), radius, 0] for i in range(vert)]
            new_angs = sorted(new_angs + net_all + intersec)

        elif connex == 1:
            beta = (net[1] - net[2])
            if beta <= 0:
                beta = (net[1] - net[2] + 2*pi)
            v_angle += net[0] - net[1]
            vert = int((beta) / theta) + 1
            new_angs = [[normal_angle((theta*i) + v_angle), radius, 0] for i in range(vert)]
            closing_ang = [net[1], radius, 0]
            if beta / theta - int(beta / theta) != 0:
                new_angs.append(closing_ang)
            new_angs[0][2] = 1
            new_angs[-1][2] = 1
            new_angs = sorted(new_angs + intersec)

        else:
            new_angs = [[normal_angle((theta*i) + v_angle), radius, 0] for i in range(vert)]
            new_angs = sorted(new_angs + intersec)

        list_vert_x, list_vert_y, list_vert_z, edg_list = create_valid_vert_edges(x, y, z, new_angs, intersec, net, connex)
        points = list((x, y, z) for x, y, z in zip(list_vert_x, list_vert_y, list_vert_z))

        return points, edg_list

    def side_edges(self, v, edges, radius, net_full):
        net = net_full
        verts_out = []
        edges_out = []
        n = 0
        net_offset = [0 for i in range(len(net))]

        for ed in edges:
            for s in ed:

                net_of = net_offset[s]
                net_offset[s] += 3
                x1 = v[s][0] + radius[s] * cos(net[s][0][1 + net_of])
                y1 = v[s][1] + radius[s] * sin(net[s][0][1 + net_of])
                x2 = v[s][0] + radius[s] * cos(net[s][0][2 + net_of])
                y2 = v[s][1] + radius[s] * sin(net[s][0][2 + net_of])

                verts_out.append((x1, y1, v[s][2]))
                verts_out.append((x2, y2, v[s][2]))

            edges_out.append((n, n+3))
            edges_out.append((n+1, n+2))
            n += 4

        return verts_out, edges_out

    def get_inputs(self):
        inputs = self.inputs

        verts_all = inputs['Verts_in'].sv_get(deepcopy=False, default=[[(0.0, 0.0, 0.0)]])

        radius_all = inputs['Distance'].sv_get(deepcopy=False, default=[[abs(self.rad_)]])
        radius_all = [list(map(lambda x: abs(x), radius)) for radius in radius_all]

        vertices_all = inputs['Nº Vertices'].sv_get(deepcopy=False, default=[[self.vert_]])
        vertices_all = [list(map(lambda x: max(2, int(x)), Vertices)) for Vertices in vertices_all]

        edges_all = inputs['Edges_in'].sv_get(deepcopy=False, default=[[]])

        family = list_matcher([verts_all, radius_all, vertices_all, edges_all], self.list_match)

        return family

    def adjust_parameters(self, params, v_len, actual_radius, poligonal_inter, edges_in):
        verts_in, _, vertices, _ = params
        parameters = list_matcher([verts_in, vertices], self.list_match)
        net = self.build_net(verts_in, edges_in, v_len, actual_radius, poligonal_inter)
        parameters = list_matcher([verts_in, vertices, actual_radius, net], self.list_match)
        parameters = [data[0:v_len] for data in parameters]

        return net, parameters

    def mask_edges_by_mid_points(self, verts_out, edges_out, parameters, v_len, edges_in):
        mid_points = calculate_mid_points(verts_out, edges_out)
        mask_edg = mask_by_distance(mid_points, parameters, v_len, edges_in, self.mask_t)
        return mask_edges(edges_out, mask_edg)

    def get_perimeter_and_radius(self, params):
        verts_in, radius, _, _ = params
        v_len = len(verts_in)
        actual_radius = []

        if self.modeI == "Weighted":
            perimeter_number = ceil(len(radius) / v_len)
            for i in range(perimeter_number):
                if self.list_match == "Long Repeat":
                    actual_radius.append([radius[min((i*v_len + j), len(radius) - 1)] for j in range(v_len)])
                else:
                    actual_radius.append([radius[(i*v_len + j) % len(radius)] for j in range(v_len)])

        else:
            perimeter_number = len(radius)
            for i in range(perimeter_number):
                actual_radius.append([radius[i % len(radius)] for j in range(v_len)])

        return perimeter_number, actual_radius

    def generate_outlines(self, output_lists, params):
        verts_in, _, _, edges_in = params
        is_edges_in_linked = self.inputs['Edges_in'].is_linked
        poligonal_inter = (0 if self.intersecction_handle == "Circular" else 1)

        v_len = len(verts_in)
        edges_in = [i for i in edges_in if i[0] < v_len and i[1] < v_len]

        perimeter_number, actual_radius = self.get_perimeter_and_radius(params)

        for i in range(perimeter_number):

            net, parameters = self.adjust_parameters(params, v_len, actual_radius[i], poligonal_inter, edges_in)
            verts_in, _, actual_radius[i], net = parameters
            start_geometry = [self.make_verts(vi, v, r, n) for vi, v, r, n in zip(*parameters)]
            edg = [p[1] for p in start_geometry]
            points = [p[0] for p in start_geometry]

            if is_edges_in_linked:
                verts_sides_out, edge_side_out = self.side_edges(verts_in, edges_in, actual_radius[i], net)
                points.append(verts_sides_out)
                edg.append(edge_side_out)

            verts_out, _, edges_out = mesh_join(points, [], edg)

            verts_out, edges_out = intersect_edges_2d(verts_out, edges_out)

            edges_out = self.mask_edges_by_mid_points(verts_out, edges_out, parameters, v_len, edges_in)

            verts_out, edges_out = remove_doubles_from_edgenet(verts_out, edges_out, self.rm_doubles)

            verts_out, edges_out = sort_verts_by_connexions(verts_out, edges_out)

            output_lists[0].append(verts_out)
            output_lists[1].append(edges_out)

    def process(self):

        inputs, outputs = self.inputs, self.outputs
        if not outputs['Vertices'].is_linked:
            return


        output_lists = [[], []]

        _ = [self.generate_outlines(output_lists, params) for params in zip(*self.get_inputs())]

        vertices_out, edges_out = output_lists
        outputs['Vertices'].sv_set(vertices_out)

        if outputs['Edges'].is_linked:
            outputs['Edges'].sv_set(edges_out)



def register():
    bpy.utils.register_class(SvContourNode)


def unregister():
    bpy.utils.unregister_class(SvContourNode)
