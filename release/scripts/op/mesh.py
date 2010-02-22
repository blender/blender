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


class MeshSelectInteriorFaces(bpy.types.Operator):
    '''Select faces where all edges have more then 2 face users.'''

    bl_idname = "mesh.faces_select_interior"
    bl_label = "Select Interior Faces"
    bl_register = True
    bl_undo = True

    def poll(self, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH')

    def execute(self, context):
        ob = context.active_object
        bpy.ops.mesh.selection_type(type='FACE')
        is_editmode = (ob.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        mesh = ob.data

        face_list = [face for face in mesh.faces]
        face_edge_keys = [face.edge_keys for face in face_list]

        edge_face_count = mesh.edge_face_count_dict

        def test_interior(index):
            for key in face_edge_keys[index]:
                if edge_face_count[key] < 3:
                    return False
            return True

        for index, face in enumerate(face_list):
            if(test_interior(index)):
                face.selected = True
            else:
                face.selected = False

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)
        return {'FINISHED'}


class MeshMirrorUV(bpy.types.Operator):
    '''Copy mirror UV coordinates on the X axis based on a mirrored mesh'''
    bl_idname = "mesh.faces_miror_uv"
    bl_label = "Copy Mirrored UV coords"
    bl_register = True
    bl_undo = True

    def poll(self, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH')

    def execute(self, context):
        DIR = 1 # TODO, make an option

        from Mathutils import Vector

        ob = context.active_object
        is_editmode = (ob.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        mesh = ob.data

        # mirror lookups
        mirror_gt = {}
        mirror_lt = {}

        vcos = [v.co.to_tuple(5) for v in mesh.verts]

        for i, co in enumerate(vcos):
            if co[0] > 0.0:
                mirror_gt[co] = i
            elif co[0] < 0.0:
                mirror_lt[co] = i
            else:
                mirror_gt[co] = i
                mirror_lt[co] = i

        #for i, v in enumerate(mesh.verts):
        vmap = {}
        for mirror_a, mirror_b in (mirror_gt, mirror_lt), (mirror_lt, mirror_gt):
            for co, i in mirror_a.items():
                nco = (-co[0], co[1], co[2])
                j = mirror_b.get(nco)
                if j is not None:
                    vmap[i] = j


        active_uv_layer = None
        for lay in mesh.uv_textures:
            if lay.active:
                active_uv_layer = lay.data
                break

        fuvs = [(uv.uv1, uv.uv2, uv.uv3, uv.uv4) for uv in active_uv_layer]
        fuvs_cpy = [(uv[0].copy(), uv[1].copy(), uv[2].copy(), uv[3].copy()) for uv in fuvs]

        # as a list
        faces = mesh.faces[:]

        fuvsel = [(False not in uv.uv_selected) for uv in active_uv_layer]
        fcents = [f.center for f in faces]

        # find mirror faces
        mirror_fm = {}
        for i, f in enumerate(faces):
            verts = f.verts[:]
            verts.sort()
            verts = tuple(verts)
            mirror_fm[verts] = i

        fmap = {}
        for i, f in enumerate(faces):
            verts = [vmap.get(j) for j in f.verts]
            if None not in verts:
                verts.sort()
                j = mirror_fm.get(tuple(verts))
                if j is not None:
                    fmap[i] = j

        done = [False] * len(faces)
        for i, j in fmap.items():

            if not fuvsel[i] or not fuvsel[j]:
                continue
            elif DIR == 0 and fcents[i][0] < 0.0:
                continue
            elif DIR == 1 and fcents[i][0] > 0.0:
                continue

            # copy UVs
            uv1 = fuvs[i]
            uv2 = fuvs_cpy[j]

            # get the correct rotation
            v1 = faces[j].verts[:]
            v2 = [vmap[k] for k in faces[i].verts[:]]


            for k in range(len(uv1)):
                k_map = v1.index(v2[k])
                uv1[k].x = - (uv2[k_map].x - 0.5) + 0.5
                uv1[k].y = uv2[k_map].y

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)

        return {'FINISHED'}


# Register the operator
classes = [
    MeshSelectInteriorFaces,
    MeshMirrorUV]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
