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
from mathutils import geometry


def add_vertex_to_intersection():

    obj = bpy.context.object
    me = obj.data
    bm = bmesh.from_edit_mesh(me)

    edges = [e for e in bm.edges if e.select]

    if len(edges) == 2:
        [[v1, v2], [v3, v4]] = [[v.co for v in e.verts] for e in edges]

        iv = geometry.intersect_line_line(v1, v2, v3, v4)
        if iv:
            iv = (iv[0] + iv[1]) / 2
            bm.verts.new(iv)

            bm.verts.ensure_lookup_table()

            bm.verts[-1].select = True
            bmesh.update_edit_mesh(me)


class TCVert2Intersection(bpy.types.Operator):
    '''Add a vertex at the intersection (projected or real) of two selected edges'''
    bl_idname = 'tinycad.vertintersect'
    bl_label = 'V2X vertex to intersection'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.type == 'MESH' and obj.mode == 'EDIT'

    def execute(self, context):
        add_vertex_to_intersection()
        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)
