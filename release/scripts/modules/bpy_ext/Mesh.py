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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

def ord_ind(i1,i2):
    if i1<i2: return i1,i2
    return i2,i1

def edge_key(ed):
    v1, v2 = tuple(ed.verts)
    return ord_ind(v1, v2)

def face_edge_keys(face):
    verts = tuple(face.verts)
    if len(verts)==3:
        return ord_ind(verts[0], verts[1]),  ord_ind(verts[1], verts[2]),  ord_ind(verts[2], verts[0])

    return ord_ind(verts[0], verts[1]),  ord_ind(verts[1], verts[2]),  ord_ind(verts[2], verts[3]),  ord_ind(verts[3], verts[0])

def mesh_edge_keys(mesh):
    return [edge_key for face in mesh.faces for edge_key in face.edge_keys()]

def mesh_edge_face_count_dict(mesh, face_edge_keys=None):

    # Optional speedup
    if face_edge_keys==None:
        face_edge_keys = [face.edge_keys() for face in face_list]

    face_edge_count = {}
    for face_keys in face_edge_keys:
        for key in face_keys:
            try:
                face_edge_count[key] += 1
            except:
                face_edge_count[key] = 1


    return face_edge_count

def mesh_edge_face_count(mesh, face_edge_keys=None):
    edge_face_count_dict = mesh.edge_face_count_dict(face_edge_keys)
    return [edge_face_count_dict.get(ed.key(), 0) for ed in mesh.edges]

import bpy

# * Edge *
class_obj = bpy.types.MeshEdge
class_obj.key = edge_key

# * Face *
class_obj = bpy.types.MeshFace
class_obj.edge_keys = face_edge_keys

# * Mesh *
class_obj = bpy.types.Mesh
class_obj.edge_keys = mesh_edge_keys
class_obj.edge_face_count = mesh_edge_face_count
class_obj.edge_face_count_dict = mesh_edge_face_count_dict
