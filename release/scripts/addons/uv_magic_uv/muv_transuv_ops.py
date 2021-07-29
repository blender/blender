# <pep8-80 compliant>

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

__author__ = "Nutti <nutti.metro@gmail.com>, Mifth, MaxRobinot"
__status__ = "production"
__version__ = "4.4"
__date__ = "2 Aug 2017"

from collections import OrderedDict

import bpy
import bmesh
from bpy.props import BoolProperty

from . import muv_props
from . import muv_common


class MUV_TransUVCopy(bpy.types.Operator):
    """
        Operation class: Transfer UV copy
        Topological based copy
    """

    bl_idname = "uv.muv_transuv_copy"
    bl_label = "Transfer UV Copy"
    bl_description = "Transfer UV Copy (Topological based copy)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.muv_props.transuv
        active_obj = context.scene.objects.active
        bm = bmesh.from_edit_mesh(active_obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()

        # get UV layer
        if not bm.loops.layers.uv:
            self.report({'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}
        uv_layer = bm.loops.layers.uv.verify()

        props.topology_copied.clear()

        # get selected faces
        active_face = bm.faces.active
        sel_faces = [face for face in bm.faces if face.select]
        if len(sel_faces) != 2:
            self.report({'WARNING'}, "Two faces must be selected")
            return {'CANCELLED'}
        if not active_face or active_face not in sel_faces:
            self.report({'WARNING'}, "Two faces must be active")
            return {'CANCELLED'}

        # parse all faces according to selection
        active_face_nor = active_face.normal.copy()
        all_sorted_faces = main_parse(
            self, uv_layer, sel_faces, active_face,
            active_face_nor)

        if all_sorted_faces:
            for face_data in all_sorted_faces.values():
                edges = face_data[1]
                uv_loops = face_data[2]
                uvs = [l.uv.copy() for l in uv_loops]
                pin_uvs = [l.pin_uv for l in uv_loops]
                seams = [e.seam for e in edges]
                props.topology_copied.append([uvs, pin_uvs, seams])

        bmesh.update_edit_mesh(active_obj.data)

        return {'FINISHED'}


class MUV_TransUVPaste(bpy.types.Operator):
    """
        Operation class: Transfer UV paste
        Topological based paste
    """

    bl_idname = "uv.muv_transuv_paste"
    bl_label = "Transfer UV Paste"
    bl_description = "Transfer UV Paste (Topological based paste)"
    bl_options = {'REGISTER', 'UNDO'}

    invert_normals = BoolProperty(
        name="Invert Normals",
        description="Invert Normals",
        default=False
    )
    copy_seams = BoolProperty(
        name="Copy Seams",
        description="Copy Seams",
        default=True
    )

    def execute(self, context):
        props = context.scene.muv_props.transuv
        active_obj = context.scene.objects.active
        bm = bmesh.from_edit_mesh(active_obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()

        # get UV layer
        if not bm.loops.layers.uv:
            self.report({'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}
        uv_layer = bm.loops.layers.uv.verify()

        # get selection history
        all_sel_faces = [
            e for e in bm.select_history
            if isinstance(e, bmesh.types.BMFace) and e.select]
        if len(all_sel_faces) % 2 != 0:
            self.report({'WARNING'}, "Two faces must be selected")
            return {'CANCELLED'}

        # parse selection history
        for i, _ in enumerate(all_sel_faces):
            if i > 0 and i % 2 != 0:
                sel_faces = [all_sel_faces[i - 1], all_sel_faces[i]]
                active_face = all_sel_faces[i]

                # parse all faces according to selection history
                active_face_nor = active_face.normal.copy()
                if self.invert_normals:
                    active_face_nor.negate()
                all_sorted_faces = main_parse(
                    self, uv_layer, sel_faces, active_face,
                    active_face_nor)

                if all_sorted_faces:
                    # check amount of copied/pasted faces
                    if len(all_sorted_faces) != len(props.topology_copied):
                        self.report(
                            {'WARNING'},
                            "Mesh has different amount of faces"
                        )
                        return {'FINISHED'}

                    for i, face_data in enumerate(all_sorted_faces.values()):
                        copied_data = props.topology_copied[i]

                        # check amount of copied/pasted verts
                        if len(copied_data[0]) != len(face_data[2]):
                            bpy.ops.mesh.select_all(action='DESELECT')
                            # select problematic face
                            list(all_sorted_faces.keys())[i].select = True
                            self.report(
                                {'WARNING'},
                                "Face have different amount of vertices"
                            )
                            return {'FINISHED'}

                        for j, (edge, uvloop) in enumerate(zip(face_data[1], face_data[2])):
                            uvloop.uv = copied_data[0][j]
                            uvloop.pin_uv = copied_data[1][j]
                            if self.copy_seams:
                                edge.seam = copied_data[2][j]

        bmesh.update_edit_mesh(active_obj.data)
        if self.copy_seams:
            active_obj.data.show_edge_seams = True

        return {'FINISHED'}


def main_parse(
        self, uv_layer, sel_faces,
        active_face, active_face_nor):
    all_sorted_faces = OrderedDict()  # This is the main stuff

    used_verts = set()
    used_edges = set()

    faces_to_parse = []

    # get shared edge of two faces
    cross_edges = []
    for edge in active_face.edges:
        if edge in sel_faces[0].edges and edge in sel_faces[1].edges:
            cross_edges.append(edge)

    # parse two selected faces
    if cross_edges and len(cross_edges) == 1:
        shared_edge = cross_edges[0]
        vert1 = None
        vert2 = None

        dot_n = active_face_nor.normalized()
        edge_vec_1 = (shared_edge.verts[1].co - shared_edge.verts[0].co)
        edge_vec_len = edge_vec_1.length
        edge_vec_1 = edge_vec_1.normalized()

        af_center = active_face.calc_center_median()
        af_vec = shared_edge.verts[0].co + (edge_vec_1 * (edge_vec_len * 0.5))
        af_vec = (af_vec - af_center).normalized()

        if af_vec.cross(edge_vec_1).dot(dot_n) > 0:
            vert1 = shared_edge.verts[0]
            vert2 = shared_edge.verts[1]
        else:
            vert1 = shared_edge.verts[1]
            vert2 = shared_edge.verts[0]

        # get active face stuff and uvs
        face_stuff = get_other_verts_edges(
            active_face, vert1, vert2, shared_edge, uv_layer)
        all_sorted_faces[active_face] = face_stuff
        used_verts.update(active_face.verts)
        used_edges.update(active_face.edges)

        # get first selected face stuff and uvs as they share shared_edge
        second_face = sel_faces[0]
        if second_face is active_face:
            second_face = sel_faces[1]
        face_stuff = get_other_verts_edges(
            second_face, vert1, vert2, shared_edge, uv_layer)
        all_sorted_faces[second_face] = face_stuff
        used_verts.update(second_face.verts)
        used_edges.update(second_face.edges)

        # first Grow
        faces_to_parse.append(active_face)
        faces_to_parse.append(second_face)

    else:
        self.report({'WARNING'}, "Two faces should share one edge")
        return None

    # parse all faces
    while True:
        new_parsed_faces = []
        if not faces_to_parse:
            break
        for face in faces_to_parse:
            face_stuff = all_sorted_faces.get(face)
            new_faces = parse_faces(
                face, face_stuff, used_verts, used_edges, all_sorted_faces,
                uv_layer)
            if new_faces == 'CANCELLED':
                self.report({'WARNING'}, "More than 2 faces share edge")
                return None

            new_parsed_faces += new_faces
        faces_to_parse = new_parsed_faces

    return all_sorted_faces


def parse_faces(
        check_face, face_stuff, used_verts, used_edges, all_sorted_faces,
        uv_layer):
    """recurse faces around the new_grow only"""

    new_shared_faces = []
    for sorted_edge in face_stuff[1]:
        shared_faces = sorted_edge.link_faces
        if shared_faces:
            if len(shared_faces) > 2:
                bpy.ops.mesh.select_all(action='DESELECT')
                for face_sel in shared_faces:
                    face_sel.select = True
                shared_faces = []
                return 'CANCELLED'

            clear_shared_faces = get_new_shared_faces(
                check_face, sorted_edge, shared_faces, all_sorted_faces.keys())
            if clear_shared_faces:
                shared_face = clear_shared_faces[0]
                # get vertices of the edge
                vert1 = sorted_edge.verts[0]
                vert2 = sorted_edge.verts[1]

                muv_common.debug_print(face_stuff[0], vert1, vert2)
                if face_stuff[0].index(vert1) > face_stuff[0].index(vert2):
                    vert1 = sorted_edge.verts[1]
                    vert2 = sorted_edge.verts[0]

                muv_common.debug_print(shared_face.verts, vert1, vert2)
                new_face_stuff = get_other_verts_edges(
                    shared_face, vert1, vert2, sorted_edge, uv_layer)
                all_sorted_faces[shared_face] = new_face_stuff
                used_verts.update(shared_face.verts)
                used_edges.update(shared_face.edges)

                if muv_props.DEBUG:
                    shared_face.select = True  # test which faces are parsed

                new_shared_faces.append(shared_face)

    return new_shared_faces


def get_new_shared_faces(orig_face, shared_edge, check_faces, used_faces):
    shared_faces = []

    for face in check_faces:
        is_shared_edge = shared_edge in face.edges
        not_used = face not in used_faces
        not_orig = face is not orig_face
        not_hide = face.hide is False
        if is_shared_edge and not_used and not_orig and not_hide:
            shared_faces.append(face)

    return shared_faces


def get_other_verts_edges(face, vert1, vert2, first_edge, uv_layer):
    face_edges = [first_edge]
    face_verts = [vert1, vert2]
    face_loops = []

    other_edges = [edge for edge in face.edges if edge not in face_edges]

    for _ in range(len(other_edges)):
        found_edge = None
        # get sorted verts and edges
        for edge in other_edges:
            if face_verts[-1] in edge.verts:
                other_vert = edge.other_vert(face_verts[-1])

                if other_vert not in face_verts:
                    face_verts.append(other_vert)

                found_edge = edge
                if found_edge not in face_edges:
                    face_edges.append(edge)
                break

        other_edges.remove(found_edge)

    # get sorted uvs
    for vert in face_verts:
        for loop in face.loops:
            if loop.vert is vert:
                face_loops.append(loop[uv_layer])
                break

    return [face_verts, face_edges, face_loops]
