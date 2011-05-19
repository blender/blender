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


def mesh_linked_faces(mesh):
    '''
    Splits the mesh into connected parts,
    these parts are returned as lists of faces.
    used for seperating cubes from other mesh elements in the 1 mesh
    '''

    # Build vert face connectivity
    vert_faces = [[] for i in range(len(mesh.vertices))]
    for f in mesh.faces:
        for v in f.vertices:
            vert_faces[v].append(f)

    # sort faces into connectivity groups
    face_groups = [[f] for f in mesh.faces]
    face_mapping = list(range(len(mesh.faces)))  # map old, new face location

    # Now clump faces iterativly
    ok = True
    while ok:
        ok = False

        for i, f in enumerate(mesh.faces):
            mapped_index = face_mapping[f.index]
            mapped_group = face_groups[mapped_index]

            for v in f.vertices:
                for nxt_f in vert_faces[v]:
                    if nxt_f != f:
                        nxt_mapped_index = face_mapping[nxt_f.index]

                        # We are not a part of the same group
                        if mapped_index != nxt_mapped_index:
                            ok = True

                            # Assign mapping to this group so they all map to this group
                            for grp_f in face_groups[nxt_mapped_index]:
                                face_mapping[grp_f.index] = mapped_index

                            # Move faces into this group
                            mapped_group.extend(face_groups[nxt_mapped_index])

                            # remove reference to the list
                            face_groups[nxt_mapped_index] = None

    # return all face groups that are not null
    # this is all the faces that are connected in their own lists.
    return [fg for fg in face_groups if fg]
