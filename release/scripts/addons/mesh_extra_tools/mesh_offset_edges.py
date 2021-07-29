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

bl_info = {
    "name": "Offset Edges",
    "author": "Hidesato Ikeya",
    "version": (0, 2, 6),
    "blender": (2, 70, 0),
    "location": "VIEW3D > Edge menu(CTRL-E) > Offset Edges",
    "description": "Offset Edges",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Modeling/offset_edges",
    "category": "Mesh"}

import bpy
import bmesh
from bpy.types import Operator
from math import sin, cos, pi, radians
from mathutils import Vector
from time import perf_counter

from bpy.props import (
        BoolProperty,
        FloatProperty,
        EnumProperty,
        )

# Globals
X_UP = Vector((1.0, .0, .0))
Y_UP = Vector((.0, 1.0, .0))
Z_UP = Vector((.0, .0, 1.0))
ZERO_VEC = Vector((.0, .0, .0))
ANGLE_90 = pi / 2
ANGLE_180 = pi
ANGLE_360 = 2 * pi

# switch performance logging
ENABLE_DEBUG = False


def calc_loop_normal(verts, fallback=Z_UP):
    # Calculate normal from verts using Newell's method
    normal = ZERO_VEC.copy()

    if verts[0] is verts[-1]:
        # Perfect loop
        range_verts = range(1, len(verts))
    else:
        # Half loop
        range_verts = range(0, len(verts))

    for i in range_verts:
        v1co, v2co = verts[i - 1].co, verts[i].co
        normal.x += (v1co.y - v2co.y) * (v1co.z + v2co.z)
        normal.y += (v1co.z - v2co.z) * (v1co.x + v2co.x)
        normal.z += (v1co.x - v2co.x) * (v1co.y + v2co.y)

    if normal != ZERO_VEC:
        normal.normalize()
    else:
        normal = fallback

    return normal


def collect_edges(bm):
    set_edges_orig = set()
    for e in bm.edges:
        if e.select:
            co_faces_selected = 0
            for f in e.link_faces:
                if f.select:
                    co_faces_selected += 1
                    if co_faces_selected == 2:
                        break
            else:
                set_edges_orig.add(e)

    if not set_edges_orig:
        return None

    return set_edges_orig


def collect_loops(set_edges_orig):
    set_edges_copy = set_edges_orig.copy()

    loops = []  # [v, e, v, e, ... , e, v]
    while set_edges_copy:
        edge_start = set_edges_copy.pop()
        v_left, v_right = edge_start.verts
        lp = [v_left, edge_start, v_right]
        reverse = False
        while True:
            edge = None
            for e in v_right.link_edges:
                if e in set_edges_copy:
                    if edge:
                        # Overlap detected.
                        return None
                    edge = e
                    set_edges_copy.remove(e)
            if edge:
                v_right = edge.other_vert(v_right)
                lp.extend((edge, v_right))
                continue
            else:
                if v_right is v_left:
                    # Real loop.
                    loops.append(lp)
                    break
                elif reverse is False:
                    # Right side of half loop
                    # Reversing the loop to operate same procedure on the left side
                    lp.reverse()
                    v_right, v_left = v_left, v_right
                    reverse = True
                    continue
                else:
                    # Half loop, completed
                    loops.append(lp)
                    break
    return loops


def get_adj_ix(ix_start, vec_edges, half_loop):
    # Get adjacent edge index, skipping zero length edges
    len_edges = len(vec_edges)
    if half_loop:
        range_right = range(ix_start, len_edges)
        range_left = range(ix_start - 1, -1, -1)
    else:
        range_right = range(ix_start, ix_start + len_edges)
        range_left = range(ix_start - 1, ix_start - 1 - len_edges, -1)

    ix_right = ix_left = None
    for i in range_right:
        # Right
        i %= len_edges
        if vec_edges[i] != ZERO_VEC:
            ix_right = i
            break
    for i in range_left:
        # Left
        i %= len_edges
        if vec_edges[i] != ZERO_VEC:
            ix_left = i
            break
    if half_loop:
        # If index of one side is None, assign another index
        if ix_right is None:
            ix_right = ix_left
        if ix_left is None:
            ix_left = ix_right

    return ix_right, ix_left


def get_adj_faces(edges):
    adj_faces = []
    for e in edges:
        adj_f = None
        co_adj = 0
        for f in e.link_faces:
            # Search an adjacent face
            # Selected face has precedence
            if not f.hide and f.normal != ZERO_VEC:
                adj_f = f
                co_adj += 1
                if f.select:
                    adj_faces.append(adj_f)
                    break
        else:
            if co_adj == 1:
                adj_faces.append(adj_f)
            else:
                adj_faces.append(None)
    return adj_faces


def get_edge_rail(vert, set_edges_orig):
    co_edges = co_edges_selected = 0
    vec_inner = None
    for e in vert.link_edges:
        if (e not in set_edges_orig and
                (e.select or (co_edges_selected == 0 and not e.hide))):
            v_other = e.other_vert(vert)
            vec = v_other.co - vert.co
            if vec != ZERO_VEC:
                vec_inner = vec
                if e.select:
                    co_edges_selected += 1
                    if co_edges_selected == 2:
                        return None
                else:
                    co_edges += 1
    if co_edges_selected == 1:
        vec_inner.normalize()
        return vec_inner
    elif co_edges == 1:
        # No selected edges, one unselected edge
        vec_inner.normalize()
        return vec_inner
    else:
        return None


def get_cross_rail(vec_tan, vec_edge_r, vec_edge_l, normal_r, normal_l):
    # Cross rail is a cross vector between normal_r and normal_l
    vec_cross = normal_r.cross(normal_l)
    if vec_cross.dot(vec_tan) < .0:
        vec_cross *= -1
    cos_min = min(vec_tan.dot(vec_edge_r), vec_tan.dot(-vec_edge_l))
    cos = vec_tan.dot(vec_cross)
    if cos >= cos_min:
        vec_cross.normalize()
        return vec_cross
    else:
        return None


def move_verts(width, depth, verts, directions, geom_ex):
    if geom_ex:
        geom_s = geom_ex['side']
        verts_ex = []
        for v in verts:
            for e in v.link_edges:
                if e in geom_s:
                    verts_ex.append(e.other_vert(v))
                    break
        verts = verts_ex

    for v, (vec_width, vec_depth) in zip(verts, directions):
        v.co += width * vec_width + depth * vec_depth


def extrude_edges(bm, edges_orig):
    extruded = bmesh.ops.extrude_edge_only(bm, edges=edges_orig)['geom']
    n_edges = n_faces = len(edges_orig)
    n_verts = len(extruded) - n_edges - n_faces

    geom = dict()
    geom['verts'] = verts = set(extruded[:n_verts])
    geom['edges'] = edges = set(extruded[n_verts:n_verts + n_edges])
    geom['faces'] = set(extruded[n_verts + n_edges:])
    geom['side'] = set(e for v in verts for e in v.link_edges if e not in edges)

    return geom


def clean(bm, mode, edges_orig, geom_ex=None):
    for f in bm.faces:
        f.select = False
    if geom_ex:
        for e in geom_ex['edges']:
            e.select = True
        if mode == 'offset':
            lis_geom = list(geom_ex['side']) + list(geom_ex['faces'])
            bmesh.ops.delete(bm, geom=lis_geom, context=2)
    else:
        for e in edges_orig:
            e.select = True


def collect_mirror_planes(edit_object):
    mirror_planes = []
    eob_mat_inv = edit_object.matrix_world.inverted()
    for m in edit_object.modifiers:
        if (m.type == 'MIRROR' and m.use_mirror_merge):
            merge_limit = m.merge_threshold
            if not m.mirror_object:
                loc = ZERO_VEC
                norm_x, norm_y, norm_z = X_UP, Y_UP, Z_UP
            else:
                mirror_mat_local = eob_mat_inv * m.mirror_object.matrix_world
                loc = mirror_mat_local.to_translation()
                norm_x, norm_y, norm_z, _ = mirror_mat_local.adjugated()
                norm_x = norm_x.to_3d().normalized()
                norm_y = norm_y.to_3d().normalized()
                norm_z = norm_z.to_3d().normalized()
            if m.use_x:
                mirror_planes.append((loc, norm_x, merge_limit))
            if m.use_y:
                mirror_planes.append((loc, norm_y, merge_limit))
            if m.use_z:
                mirror_planes.append((loc, norm_z, merge_limit))
    return mirror_planes


def get_vert_mirror_pairs(set_edges_orig, mirror_planes):
    if mirror_planes:
        set_edges_copy = set_edges_orig.copy()
        vert_mirror_pairs = dict()
        for e in set_edges_orig:
            v1, v2 = e.verts
            for mp in mirror_planes:
                p_co, p_norm, mlimit = mp
                v1_dist = abs(p_norm.dot(v1.co - p_co))
                v2_dist = abs(p_norm.dot(v2.co - p_co))
                if v1_dist <= mlimit:
                    # v1 is on a mirror plane
                    vert_mirror_pairs[v1] = mp
                if v2_dist <= mlimit:
                    # v2 is on a mirror plane
                    vert_mirror_pairs[v2] = mp
                if v1_dist <= mlimit and v2_dist <= mlimit:
                    # This edge is on a mirror_plane, so should not be offsetted
                    set_edges_copy.remove(e)
        return vert_mirror_pairs, set_edges_copy
    else:
        return None, set_edges_orig


def get_mirror_rail(mirror_plane, vec_up):
    p_norm = mirror_plane[1]
    mirror_rail = vec_up.cross(p_norm)
    if mirror_rail != ZERO_VEC:
        mirror_rail.normalize()
        # Project vec_up to mirror_plane
        vec_up = vec_up - vec_up.project(p_norm)
        vec_up.normalize()
        return mirror_rail, vec_up
    else:
        return None, vec_up


def reorder_loop(verts, edges, lp_normal, adj_faces):
    for i, adj_f in enumerate(adj_faces):
        if adj_f is None:
            continue

        v1, v2 = verts[i], verts[i + 1]
        fv = tuple(adj_f.verts)
        if fv[fv.index(v1) - 1] is v2:
            # Align loop direction
            verts.reverse()
            edges.reverse()
            adj_faces.reverse()

        if lp_normal.dot(adj_f.normal) < .0:
            lp_normal *= -1
        break
    else:
        # All elements in adj_faces are None
        for v in verts:
            if v.normal != ZERO_VEC:
                if lp_normal.dot(v.normal) < .0:
                    verts.reverse()
                    edges.reverse()
                    lp_normal *= -1
                break

    return verts, edges, lp_normal, adj_faces


def get_directions(lp, vec_upward, normal_fallback, vert_mirror_pairs, **options):
    opt_follow_face = options['follow_face']
    opt_edge_rail = options['edge_rail']
    opt_er_only_end = options['edge_rail_only_end']
    opt_threshold = options['threshold']

    verts, edges = lp[::2], lp[1::2]
    set_edges = set(edges)
    lp_normal = calc_loop_normal(verts, fallback=normal_fallback)

    # Loop order might be changed below
    if lp_normal.dot(vec_upward) < .0:
        # Make this loop's normal towards vec_upward
        verts.reverse()
        edges.reverse()
        lp_normal *= -1

    if opt_follow_face:
        adj_faces = get_adj_faces(edges)
        verts, edges, lp_normal, adj_faces = \
            reorder_loop(verts, edges, lp_normal, adj_faces)
    else:
        adj_faces = (None, ) * len(edges)
    # Loop order might be changed above

    vec_edges = tuple((e.other_vert(v).co - v.co).normalized()
                      for v, e in zip(verts, edges))

    if verts[0] is verts[-1]:
        # Real loop. Popping last vertex
        verts.pop()
        HALF_LOOP = False
    else:
        # Half loop
        HALF_LOOP = True

    len_verts = len(verts)
    directions = []
    for i in range(len_verts):
        vert = verts[i]
        ix_right, ix_left = i, i - 1

        VERT_END = False
        if HALF_LOOP:
            if i == 0:
                # First vert
                ix_left = ix_right
                VERT_END = True
            elif i == len_verts - 1:
                # Last vert
                ix_right = ix_left
                VERT_END = True

        edge_right, edge_left = vec_edges[ix_right], vec_edges[ix_left]
        face_right, face_left = adj_faces[ix_right], adj_faces[ix_left]

        norm_right = face_right.normal if face_right else lp_normal
        norm_left = face_left.normal if face_left else lp_normal
        if norm_right.angle(norm_left) > opt_threshold:
            # Two faces are not flat
            two_normals = True
        else:
            two_normals = False

        tan_right = edge_right.cross(norm_right).normalized()
        tan_left = edge_left.cross(norm_left).normalized()
        tan_avr = (tan_right + tan_left).normalized()
        norm_avr = (norm_right + norm_left).normalized()

        rail = None
        if two_normals or opt_edge_rail:
            # Get edge rail
            # edge rail is a vector of an inner edge
            if two_normals or (not opt_er_only_end) or VERT_END:
                rail = get_edge_rail(vert, set_edges)
        if vert_mirror_pairs and VERT_END:
            if vert in vert_mirror_pairs:
                rail, norm_avr = get_mirror_rail(vert_mirror_pairs[vert], norm_avr)
        if (not rail) and two_normals:
            # Get cross rail
            # Cross rail is a cross vector between norm_right and norm_left
            rail = get_cross_rail(
                tan_avr, edge_right, edge_left, norm_right, norm_left)
        if rail:
            dot = tan_avr.dot(rail)
            if dot > .0:
                tan_avr = rail
            elif dot < .0:
                tan_avr = -rail

        vec_plane = norm_avr.cross(tan_avr)
        e_dot_p_r = edge_right.dot(vec_plane)
        e_dot_p_l = edge_left.dot(vec_plane)
        if e_dot_p_r or e_dot_p_l:
            if e_dot_p_r > e_dot_p_l:
                vec_edge, e_dot_p = edge_right, e_dot_p_r
            else:
                vec_edge, e_dot_p = edge_left, e_dot_p_l

            vec_tan = (tan_avr - tan_avr.project(vec_edge)).normalized()
            # Make vec_tan perpendicular to vec_edge
            vec_up = vec_tan.cross(vec_edge)

            vec_width = vec_tan - (vec_tan.dot(vec_plane) / e_dot_p) * vec_edge
            vec_depth = vec_up - (vec_up.dot(vec_plane) / e_dot_p) * vec_edge
        else:
            vec_width = tan_avr
            vec_depth = norm_avr

        directions.append((vec_width, vec_depth))

    return verts, directions


angle_presets = {'0°': 0,
                 '15°': radians(15),
                 '30°': radians(30),
                 '45°': radians(45),
                 '60°': radians(60),
                 '75°': radians(75),
                 '90°': radians(90),
                }


def use_cashes(self, context):
    self.caches_valid = True


def assign_angle_presets(self, context):
    use_cashes(self, context)
    self.angle = angle_presets[self.angle_presets]


class OffsetEdges(Operator):
    bl_idname = "mesh.offset_edges"
    bl_label = "Offset Edges"
    bl_description = ("Extrude, Move or Offset the selected Edges\n"
                      "Operates only on separate Edge loops selections")
    bl_options = {'REGISTER', 'UNDO'}

    geometry_mode = EnumProperty(
            items=[('offset', "Offset", "Offset edges"),
                   ('extrude', "Extrude", "Extrude edges"),
                   ('move', "Move", "Move selected edges")],
            name="Geometry mode",
            default='offset',
            update=use_cashes
            )
    width = FloatProperty(
            name="Width",
            default=.2,
            precision=4, step=1,
            update=use_cashes
            )
    flip_width = BoolProperty(
            name="Flip Width",
            default=False,
            description="Flip width direction",
            update=use_cashes
            )
    depth = FloatProperty(
            name="Depth",
            default=.0,
            precision=4, step=1,
            update=use_cashes
            )
    flip_depth = BoolProperty(
            name="Flip Depth",
            default=False,
            description="Flip depth direction",
            update=use_cashes
            )
    depth_mode = EnumProperty(
            items=[('angle', "Angle", "Angle"),
                   ('depth', "Depth", "Depth")],
            name="Depth mode",
            default='angle',
            update=use_cashes
            )
    angle = FloatProperty(
            name="Angle", default=0,
            precision=3, step=.1,
            min=-2 * pi, max=2 * pi,
            subtype='ANGLE',
            description="Angle",
            update=use_cashes
            )
    flip_angle = BoolProperty(
            name="Flip Angle",
            default=False,
            description="Flip Angle",
            update=use_cashes
            )
    follow_face = BoolProperty(
            name="Follow Face",
            default=False,
            description="Offset along faces around"
            )
    mirror_modifier = BoolProperty(
            name="Mirror Modifier",
            default=False,
            description="Take into account of Mirror modifier"
            )
    edge_rail = BoolProperty(
            name="Edge Rail",
            default=False,
            description="Align vertices along inner edges"
            )
    edge_rail_only_end = BoolProperty(
            name="Edge Rail Only End",
            default=False,
            description="Apply edge rail to end verts only"
            )
    threshold = FloatProperty(
            name="Flat Face Threshold",
            default=radians(0.05), precision=5,
            step=1.0e-4, subtype='ANGLE',
            description="If difference of angle between two adjacent faces is "
                        "below this value, those faces are regarded as flat",
            options={'HIDDEN'}
            )
    caches_valid = BoolProperty(
            name="Caches Valid",
            default=False,
            options={'HIDDEN'}
            )
    angle_presets = EnumProperty(
            items=[('0°', "0°", "0°"),
                   ('15°', "15°", "15°"),
                   ('30°', "30°", "30°"),
                   ('45°', "45°", "45°"),
                   ('60°', "60°", "60°"),
                   ('75°', "75°", "75°"),
                   ('90°', "90°", "90°"), ],
            name="Angle Presets",
            default='0°',
            update=assign_angle_presets
            )

    _cache_offset_infos = None
    _cache_edges_orig_ixs = None

    @classmethod
    def poll(self, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        layout = self.layout
        layout.prop(self, 'geometry_mode', text="")

        row = layout.row(align=True)
        row.prop(self, 'width')
        row.prop(self, 'flip_width', icon='ARROW_LEFTRIGHT', icon_only=True)
        layout.prop(self, 'depth_mode', expand=True)

        if self.depth_mode == 'angle':
            d_mode = 'angle'
            flip = 'flip_angle'
        else:
            d_mode = 'depth'
            flip = 'flip_depth'
        row = layout.row(align=True)
        row.prop(self, d_mode)
        row.prop(self, flip, icon='ARROW_LEFTRIGHT', icon_only=True)
        if self.depth_mode == 'angle':
            layout.prop(self, 'angle_presets', text="Presets", expand=True)

        layout.separator()

        layout.prop(self, 'follow_face')

        row = layout.row()
        row.prop(self, 'edge_rail')
        if self.edge_rail:
            row.prop(self, 'edge_rail_only_end', text="OnlyEnd", toggle=True)

        layout.prop(self, 'mirror_modifier')
        # layout.operator('mesh.offset_edges', text="Repeat")  # for 2.79a, Crashing...

        if self.follow_face:
            layout.separator()
            layout.prop(self, 'threshold', text="Threshold")

    def get_offset_infos(self, bm, edit_object):
        if self.caches_valid and self._cache_offset_infos is not None:
            # Return None, indicating to use cache
            return None, None

        if ENABLE_DEBUG:
            time = perf_counter()

        set_edges_orig = collect_edges(bm)
        if set_edges_orig is None:
            self.report({'WARNING'},
                        "No edges selected or edge loops could not be determined")
            return False, False

        if self.mirror_modifier:
            mirror_planes = collect_mirror_planes(edit_object)
            vert_mirror_pairs, set_edges = \
                get_vert_mirror_pairs(set_edges_orig, mirror_planes)

            if set_edges:
                set_edges_orig = set_edges
            else:
                vert_mirror_pairs = None
        else:
            vert_mirror_pairs = None

        loops = collect_loops(set_edges_orig)
        if loops is None:
            self.report({'WARNING'},
                        "Overlap detected. Select non-overlapping edge loops")
            return False, False

        vec_upward = (X_UP + Y_UP + Z_UP).normalized()
        # vec_upward is used to unify loop normals when follow_face is off
        normal_fallback = Z_UP
        # normal_fallback = Vector(context.region_data.view_matrix[2][:3])
        # normal_fallback is used when loop normal cannot be calculated

        follow_face = self.follow_face
        edge_rail = self.edge_rail
        er_only_end = self.edge_rail_only_end
        threshold = self.threshold

        offset_infos = []
        for lp in loops:
            verts, directions = get_directions(
                lp, vec_upward, normal_fallback, vert_mirror_pairs,
                follow_face=follow_face, edge_rail=edge_rail,
                edge_rail_only_end=er_only_end,
                threshold=threshold)
            if verts:
                offset_infos.append((verts, directions))

        # Saving caches
        self._cache_offset_infos = _cache_offset_infos = []
        for verts, directions in offset_infos:
            v_ixs = tuple(v.index for v in verts)
            _cache_offset_infos.append((v_ixs, directions))
        self._cache_edges_orig_ixs = tuple(e.index for e in set_edges_orig)

        if ENABLE_DEBUG:
            print("Preparing OffsetEdges: ", perf_counter() - time)

        return offset_infos, set_edges_orig

    def do_offset_and_free(self, bm, me, offset_infos=None, set_edges_orig=None):
        # If offset_infos is None, use caches
        # Makes caches invalid after offset

        if ENABLE_DEBUG:
            time = perf_counter()

        if offset_infos is None:
            # using cache
            bmverts = tuple(bm.verts)
            bmedges = tuple(bm.edges)
            edges_orig = [bmedges[ix] for ix in self._cache_edges_orig_ixs]
            verts_directions = []
            for ix_vs, directions in self._cache_offset_infos:
                verts = tuple(bmverts[ix] for ix in ix_vs)
                verts_directions.append((verts, directions))
        else:
            verts_directions = offset_infos
            edges_orig = list(set_edges_orig)

        if self.depth_mode == 'angle':
            w = self.width if not self.flip_width else -self.width
            angle = self.angle if not self.flip_angle else -self.angle
            width = w * cos(angle)
            depth = w * sin(angle)
        else:
            width = self.width if not self.flip_width else -self.width
            depth = self.depth if not self.flip_depth else -self.depth

        # Extrude
        if self.geometry_mode == 'move':
            geom_ex = None
        else:
            geom_ex = extrude_edges(bm, edges_orig)

        for verts, directions in verts_directions:
            move_verts(width, depth, verts, directions, geom_ex)

        clean(bm, self.geometry_mode, edges_orig, geom_ex)

        bpy.ops.object.mode_set(mode="OBJECT")
        bm.to_mesh(me)
        bpy.ops.object.mode_set(mode="EDIT")
        bm.free()
        self.caches_valid = False  # Make caches invalid

        if ENABLE_DEBUG:
            print("OffsetEdges offset: ", perf_counter() - time)

    def execute(self, context):
        # In edit mode
        edit_object = context.edit_object
        bpy.ops.object.mode_set(mode="OBJECT")

        me = edit_object.data
        bm = bmesh.new()
        bm.from_mesh(me)

        offset_infos, edges_orig = self.get_offset_infos(bm, edit_object)
        if offset_infos is False:
            bpy.ops.object.mode_set(mode="EDIT")
            return {'CANCELLED'}

        self.do_offset_and_free(bm, me, offset_infos, edges_orig)

        return {'FINISHED'}

    def restore_original_and_free(self, context):
        self.caches_valid = False  # Make caches invalid
        context.area.header_text_set()

        me = context.edit_object.data
        bpy.ops.object.mode_set(mode="OBJECT")
        self._bm_orig.to_mesh(me)
        bpy.ops.object.mode_set(mode="EDIT")

        self._bm_orig.free()
        context.area.header_text_set()

    def invoke(self, context, event):
        # In edit mode
        edit_object = context.edit_object
        me = edit_object.data
        bpy.ops.object.mode_set(mode="OBJECT")
        for p in me.polygons:
            if p.select:
                self.follow_face = True
                break

        self.caches_valid = False
        bpy.ops.object.mode_set(mode="EDIT")
        return self.execute(context)


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == '__main__':
    register()
