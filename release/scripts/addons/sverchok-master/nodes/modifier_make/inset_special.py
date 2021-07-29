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
import mathutils

from mathutils import Vector
from bpy.props import FloatProperty, FloatVectorProperty, IntProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (
    updateNode, Vector_generate,
    repeat_last, fullList)


''' very non optimal routines. beware. I know this '''


def inset_special(vertices, faces, inset_rates, distances, ignores, make_inners):

    new_faces = []
    new_ignores = []
    new_insets = []

    def get_average_vector(verts, n):
        dummy_vec = Vector()
        for v in verts:
            dummy_vec = dummy_vec + v
        return dummy_vec * 1/n

    def do_tri(face, lv_idx, make_inner):
        a, b, c = face
        d, e, f = lv_idx-2, lv_idx-1, lv_idx
        out_faces = []
        out_faces.append([a, b, e, d])
        out_faces.append([b, c, f, e])
        out_faces.append([c, a, d, f])
        if make_inner:
            out_faces.append([d, e, f])
            new_insets.append([d, e, f])
        return out_faces

    def do_quad(face, lv_idx, make_inner):
        a, b, c, d = face
        e, f, g, h = lv_idx-3, lv_idx-2, lv_idx-1, lv_idx
        out_faces = []
        out_faces.append([a, b, f, e])
        out_faces.append([b, c, g, f])
        out_faces.append([c, d, h, g])
        out_faces.append([d, a, e, h])
        if make_inner:
            out_faces.append([e, f, g, h])
            new_insets.append([e, f, g, h])
        return out_faces

    def do_ngon(face, lv_idx, make_inner):
        '''
        setting up the forloop only makes sense for ngons
        '''
        num_elements = len(face)
        face_elements = list(face)
        inner_elements = [lv_idx-n for n in range(num_elements-1, -1, -1)]
        # padding, wrap-around
        face_elements.append(face_elements[0])
        inner_elements.append(inner_elements[0])

        out_faces = []
        add_face = out_faces.append
        for j in range(num_elements):
            add_face([face_elements[j], face_elements[j+1], inner_elements[j+1], inner_elements[j]])

        if make_inner:
            add_face([idx[-1] for idx in out_faces])
            new_insets.append([idx[-1] for idx in out_faces])

        return out_faces

    def new_inner_from(face, inset_by, distance, make_inner):
        '''
        face:       (idx list) face to work on
        inset_by:   (scalar) amount to open the face
        axis:       (vector) axis relative to face normal
        distance:   (scalar) push new verts on axis by this amount
        make_inner: create extra internal face

        # dumb implementation first. should only loop over the verts of face 1 time
        to get
         - new faces
         - avg vertex location
         - but can't lerp until avg is known. so each input face is looped at least twice.
        '''
        current_verts_idx = len(vertices)
        n = len(face)
        verts = [vertices[i] for i in face]
        avg_vec = get_average_vector(verts, n)

        # lerp and add to vertices immediately
        new_verts_prime = [avg_vec.lerp(v, inset_by) for v in verts]

        if distance:
            local_normal = mathutils.geometry.normal(*new_verts_prime[:3])
            new_verts_prime = [v.lerp(v+local_normal, distance) for v in new_verts_prime]

        vertices.extend(new_verts_prime)

        tail_idx = (current_verts_idx + n) - 1

        get_faces_prime = {3: do_tri, 4: do_quad}.get(n, do_ngon)
        new_faces_prime = get_faces_prime(face, tail_idx, make_inner)
        new_faces.extend(new_faces_prime)

    for idx, face in enumerate(faces):
        inset_by = inset_rates[idx]

        if (inset_by > 0) and (not ignores[idx]):
            new_inner_from(face, inset_by, distances[idx], make_inners[idx])
        else:
            new_faces.append(face)
            new_ignores.append(face)

    new_verts = [v[:] for v in vertices]
    return new_verts, new_faces, new_ignores, new_insets


class SvInsetSpecial(bpy.types.Node, SverchCustomTreeNode):
    '''
    Insets geometry, optional remove and/or translate
    Don't think of this as a realtime effect.
    '''

    bl_idname = 'SvInsetSpecial'
    bl_label = 'Inset Special'
    bl_icon = 'OUTLINER_OB_EMPTY'

    inset = FloatProperty(
        name='Inset',
        description='inset amount',
        default=0.1, update=updateNode)
    distance = FloatProperty(
        name='Distance',
        description='Distance',
        default=0.0, update=updateNode)

    ignore = IntProperty(name='Ignore', description='skip polygons', default=0, update=updateNode)
    make_inner = IntProperty(name='Make Inner', description='Make inner polygon', default=1, update=updateNode)

    # axis = FloatVectorProperty(
    #   name='axis', description='axis relative to normal',
    #   default=(0,0,1), update=updateNode)

    def sv_init(self, context):
        i = self.inputs
        i.new('StringsSocket', 'inset').prop_name = 'inset'
        i.new('StringsSocket', 'distance').prop_name = 'distance'
        i.new('VerticesSocket', 'vertices')
        i.new('StringsSocket', 'polygons')
        i.new('StringsSocket', 'ignore').prop_name = 'ignore'
        i.new('StringsSocket', 'make_inner').prop_name = 'make_inner'

        o = self.outputs
        o.new('VerticesSocket', 'vertices')
        o.new('StringsSocket', 'polygons')
        o.new('StringsSocket', 'ignored')
        o.new('StringsSocket', 'inset')


    def process(self):
        i = self.inputs
        o = self.outputs

        if not o['vertices'].is_linked:
            return

        all_verts = Vector_generate(i['vertices'].sv_get())
        all_polys = i['polygons'].sv_get()

        all_inset_rates = i['inset'].sv_get()
        all_distance_vals = i['distance'].sv_get()

        # silly auto ugrade.
        if not i['ignore'].prop_name:
            i['ignore'].prop_name = 'ignore'
            i['make_inner'].prop_name = 'make_inner'

        all_ignores = i['ignore'].sv_get()
        all_make_inners = i['make_inner'].sv_get()

        data = all_verts, all_polys, all_inset_rates, all_distance_vals, all_ignores, all_make_inners

        verts_out = []
        polys_out = []
        ignored_out = []
        inset_out = []

        for v, p, inset_rates, distance_vals, ignores, make_inners in zip(*data):
            fullList(inset_rates, len(p))
            fullList(distance_vals, len(p))
            fullList(ignores, len(p))
            fullList(make_inners, len(p))

            func_args = {
                'vertices': v,
                'faces': p,
                'inset_rates': inset_rates,
                'distances': distance_vals,
                'make_inners': make_inners,
                'ignores': ignores
            }

            res = inset_special(**func_args)

            if not res:
                res = v, p, [], []

            verts_out.append(res[0])
            polys_out.append(res[1])
            ignored_out.append(res[2])
            inset_out.append(res[3])

        # deal  with hooking up the processed data to the outputs
        o['vertices'].sv_set(verts_out)
        o['polygons'].sv_set(polys_out)
        o['ignored'].sv_set(ignored_out)
        o['inset'].sv_set(inset_out)



def register():
    bpy.utils.register_class(SvInsetSpecial)


def unregister():
    bpy.utils.unregister_class(SvInsetSpecial)
