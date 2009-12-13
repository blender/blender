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

# <pep8-80 compliant>

import bpy

def main(context):
    ob = context.active_object
    bpy.ops.mesh.selection_type(type='FACE')
    is_editmode = (ob.mode=='EDIT')
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
        main(context)
        return ('FINISHED',)


# Register the operator
bpy.ops.add(MeshSelectInteriorFaces)

if __name__ == "__main__":
    bpy.ops.mesh.faces_select_interior()
