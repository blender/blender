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
import bmesh
from mathutils.geometry import intersect_line_plane


def failure_message(self):
    self.report({"WARNING"}, 'select 1 face and 1 detached edge')

def failure_message_on_plane(self):
    msg2 = """\
Edge2Face expects the edge to intersect at one point on the plane of the selected face. You're  
seeing this warning because mathutils.geometry.intersect_line_plane is being called on an edge/face  
combination that has no clear intersection point ( both points of the edge either touch the same  
plane as the face or they lie in a plane that is offset along the face's normal )"""
    lines = msg2.split('\n')
    for line in lines:
        self.report({'INFO'}, line)
    self.report({"WARNING"}, 'No intersection found, see the info panel for details')


def extend_vertex(self):

    obj = bpy.context.edit_object
    me = obj.data
    bm = bmesh.from_edit_mesh(me)
    verts = bm.verts
    faces = bm.faces

    planes = [f for f in faces if f.select]
    if not (len(planes) == 1):
        failure_message(self)
        return

    plane = planes[0]
    plane_vert_indices = [v for v in plane.verts[:]]
    all_selected_vert_indices = [v for v in verts if v.select]

    M = set(plane_vert_indices)
    N = set(all_selected_vert_indices)
    O = N.difference(M)
    O = list(O)

    if not len(O) == 2:
        failure_message(self)
        return

    (v1_ref, v1), (v2_ref, v2) = [(i, i.co) for i in O]

    plane_co = plane.calc_center_median()
    plane_no = plane.normal

    new_co = intersect_line_plane(v1, v2, plane_co, plane_no, False)

    if new_co:
        new_vertex = verts.new(new_co)
        A_len = (v1 - new_co).length
        B_len = (v2 - new_co).length

        vertex_reference = v1_ref if (A_len < B_len) else v2_ref
        bm.edges.new([vertex_reference, new_vertex])
        bmesh.update_edit_mesh(me, True)

    else:
        failure_message_on_plane(self)



class TCEdgeToFace(bpy.types.Operator):
    '''Extend selected edge towards projected intersection with a selected face'''
    bl_idname = 'tinycad.edge_to_face'
    bl_label = 'E2F edge to face'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return all([bool(ob), ob.type == 'MESH', ob.mode == 'EDIT'])

    def execute(self, context):
        extend_vertex(self)
        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)
