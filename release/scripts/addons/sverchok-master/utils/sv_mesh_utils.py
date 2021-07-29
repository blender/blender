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

def mesh_join(vertices_s, edges_s, faces_s):
    '''Given list of meshes represented by lists of vertices, edges and faces,
    produce one joined mesh.'''

    offset = 0
    result_vertices = []
    result_edges = []
    result_faces = []
    if len(edges_s) == 0:
        edges_s = [[]] * len(faces_s)
    for vertices, edges, faces in zip(vertices_s, edges_s, faces_s):
        result_vertices.extend(vertices)
        new_edges = [tuple(i + offset for i in edge) for edge in edges]
        new_faces = [[i + offset for i in face] for face in faces]
        result_edges.extend(new_edges)
        result_faces.extend(new_faces)
        offset += len(vertices)
    return result_vertices, result_edges, result_faces
