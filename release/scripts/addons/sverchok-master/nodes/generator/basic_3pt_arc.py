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

import math

import bpy
import mathutils
from bpy.props import IntProperty, FloatProperty, FloatVectorProperty
from mathutils import Vector, Euler, geometry

from sverchok.node_tree import SverchCustomTreeNode, VerticesSocket, StringsSocket
from sverchok.data_structure import updateNode


def generate_3PT_mode_1(pts=None, num_verts=20, make_edges=False):
    '''
    Arc from start - throught - Eend
    - call this function only if you have 3 pts,
    - do your error checking before passing to it.
    '''
    num_verts -= 1
    verts, edges = [], []
    V = Vector

    # construction
    v1, v2, v3, v4 = V(pts[0]), V(pts[1]), V(pts[1]), V(pts[2])
    edge1_mid = v1.lerp(v2, 0.5)
    edge2_mid = v3.lerp(v4, 0.5)
    axis = geometry.normal(v1, v2, v4)
    mat_rot = mathutils.Matrix.Rotation(math.radians(90.0), 4, axis)

    # triangle edges
    v1_ = ((v1 - edge1_mid) * mat_rot) + edge1_mid
    v2_ = ((v2 - edge1_mid) * mat_rot) + edge1_mid
    v3_ = ((v3 - edge2_mid) * mat_rot) + edge2_mid
    v4_ = ((v4 - edge2_mid) * mat_rot) + edge2_mid

    r = geometry.intersect_line_line(v1_, v2_, v3_, v4_)
    if r:
        # do arc
        p1, _ = r

        # find arc angle.
        a = (v1 - p1).angle((v4 - p1), 0)
        s = (2 * math.pi) - a

        interior_angle = (v1 - v2).angle(v4 - v3, 0)
        if interior_angle > 0.5 * math.pi:
            s = math.pi + 2 * (0.5 * math.pi - interior_angle)

        for i in range(num_verts + 1):
            mat_rot = mathutils.Matrix.Rotation(((s / num_verts) * i), 4, axis)
            vec = ((v4 - p1) * mat_rot) + p1
            verts.append(vec[:])
    else:
        # do straight line
        step_size = 1 / num_verts
        verts = [v1_.lerp(v4_, i * step_size)[:] for i in range(num_verts + 1)]

    if make_edges:
        edges = [(n, n + 1) for n in range(len(verts) - 1)]

    return verts, edges


def make_all_arcs(v, nv, make_edges):
    verts_out = []
    edges_out = []

    collected = [v[i:i+3] for i in range(0, (len(v)-2), 3)]
    for idx, pts in enumerate(collected):

        # to force a minimum of verts per arc.
        num_verts = max(3, nv[idx])
        v, e = generate_3PT_mode_1(pts, num_verts, make_edges)
        verts_out.append(v)

        if make_edges:
            edges_out.append(e)

    return verts_out, edges_out


class svBasicArcNode(bpy.types.Node, SverchCustomTreeNode):

    ''' Arc from 3 points '''
    bl_idname = 'svBasicArcNode'
    bl_label = '3pt Arc'
    bl_icon = 'SPHERECURVE'

    num_verts = IntProperty(
        name='num_verts',
        description='Num Vertices',
        default=20, min=3,
        update=updateNode)

    arc_pts = FloatVectorProperty(
        name='atc_pts',
        description="3 points, Start-Through-End",
        update=updateNode,
        size=3)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "num_verts").prop_name = 'num_verts'
        self.inputs.new('VerticesSocket', "arc_pts").prop_name = 'arc_pts'

        self.outputs.new('VerticesSocket', "Verts", "Verts")
        self.outputs.new('StringsSocket', "Edges", "Edges")

    def draw_buttons(self, context, layout):
        pass

    def process(self):
        outputs = self.outputs
        inputs = self.inputs

        '''
        - is Edges socket created, means all sockets exist.
        - is anything connected to the Verts socket?
        '''
        if not all([outputs['Verts'].is_linked, inputs['arc_pts'].is_linked]):
            return

        '''
        operational scheme:
        - input:
            :   each 3 points are 'a' 'b' and 'c', then input must be a
                flat list of [v1a, v1b, v1c, v2a, v2b, v2c, ....]
            :   len(arc_pts) % 3 === 0, else no processing.

        - satisfied by input, the output will be n lists of verts+edges
        - [n = (len(arc_pts) / 3)]

        '''

        # assume they all match, reduce cycles used for checking.
        v = inputs['arc_pts'].sv_get(deepcopy=False)[0]

        if not (len(v) % 3 == 0):
            print('number of vertices input to 3pt arc must be divisible by 3')
            return

        num_arcs = len(v) // 3
        nv = []
        # get vert_nums, or pad till matching quantity
        if inputs['num_verts'].is_linked:
            nv = inputs['num_verts'].sv_get(deepcopy=False)[0]

            if nv and (len(nv) < num_arcs):
                pad_num = num_arcs - len(nv)
                for i in range(pad_num):
                    nv.append(nv[-1])
        else:
            for i in range(num_arcs):
                nv.append(self.num_verts)

        # will generate nested lists of arcs(verts+edges)
        make_edges = outputs['Edges'].is_linked
        verts_out, edges_out = make_all_arcs(v, nv, make_edges)

        # reaches here if we got usable data.
        outputs['Verts'].sv_set(verts_out)
        if make_edges:
            outputs['Edges'].sv_set(edges_out)


def register():
    bpy.utils.register_class(svBasicArcNode)


def unregister():
    bpy.utils.unregister_class(svBasicArcNode)
