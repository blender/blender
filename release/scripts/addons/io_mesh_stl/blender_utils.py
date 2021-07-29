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
import array
from itertools import chain


def create_and_link_mesh(name, faces, face_nors, points, global_matrix):
    """
    Create a blender mesh and object called name from a list of
    *points* and *faces* and link it in the current scene.
    """

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(points, [], faces)

    if face_nors:
        # Note: we store 'temp' normals in loops, since validate() may alter final mesh,
        #       we can only set custom lnors *after* calling it.
        mesh.create_normals_split()
        lnors = tuple(chain(*chain(*zip(face_nors, face_nors, face_nors))))
        mesh.loops.foreach_set("normal", lnors)

    mesh.transform(global_matrix)

    # update mesh to allow proper display
    mesh.validate(clean_customdata=False)  # *Very* important to not remove lnors here!

    if face_nors:
        clnors = array.array('f', [0.0] * (len(mesh.loops) * 3))
        mesh.loops.foreach_get("normal", clnors)

        mesh.polygons.foreach_set("use_smooth", [True] * len(mesh.polygons))

        mesh.normals_split_custom_set(tuple(zip(*(iter(clnors),) * 3)))
        mesh.use_auto_smooth = True
        mesh.show_edge_sharp = True
        mesh.free_normals_split()

    mesh.update()

    scene = bpy.context.scene

    obj = bpy.data.objects.new(name, mesh)
    scene.objects.link(obj)
    scene.objects.active = obj
    obj.select = True


def faces_from_mesh(ob, global_matrix, use_mesh_modifiers=False, triangulate=True):
    """
    From an object, return a generator over a list of faces.

    Each faces is a list of his vertexes. Each vertex is a tuple of
    his coordinate.

    use_mesh_modifiers
        Apply the preview modifier to the returned liste

    triangulate
        Split the quad into two triangles
    """

    # get the editmode data
    ob.update_from_editmode()

    # get the modifiers
    try:
        mesh = ob.to_mesh(bpy.context.scene, use_mesh_modifiers, "PREVIEW")
    except RuntimeError:
        raise StopIteration

    mat = global_matrix * ob.matrix_world
    mesh.transform(mat)
    if mat.is_negative:
        mesh.flip_normals()
        mesh.calc_tessface()

    if triangulate:
        # From a list of faces, return the face triangulated if needed.
        def iter_face_index():
            for face in mesh.tessfaces:
                vertices = face.vertices[:]
                if len(vertices) == 4:
                    yield vertices[0], vertices[1], vertices[2]
                    yield vertices[2], vertices[3], vertices[0]
                else:
                    yield vertices
    else:
        def iter_face_index():
            for face in mesh.tessfaces:
                yield face.vertices[:]

    vertices = mesh.vertices

    for indexes in iter_face_index():
        yield [vertices[index].co.copy() for index in indexes]

    bpy.data.meshes.remove(mesh)
