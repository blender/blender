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
from . import cad_module as cm


def add_line_to_bisection(self):

    obj = bpy.context.object
    me = obj.data
    bm = bmesh.from_edit_mesh(me)

    bm.verts.ensure_lookup_table()
    bm.edges.ensure_lookup_table()

    edges = [e for e in bm.edges if e.select and not e.hide]

    if not len(edges) == 2:
        msg = "select two coplanar non parallel edges"
        self.report({"WARNING"}, msg)
        return

    [[v1, v2], [v3, v4]] = [[v.co for v in e.verts] for e in edges]
    print('vectors found:\n', v1, '\n', v2, '\n', v3, '\n', v4)

    dist1 = (v1 - v2).length
    dist2 = (v3 - v4).length
    bdist = min([dist1, dist2])
    edge1 = (v1, v2)
    edge2 = (v3, v4)

    if not cm.test_coplanar(edge1, edge2):
        msg = "edges must be coplanar non parallel edges"
        self.report({"WARNING"}, msg)
        return

    # get pt and pick fartest vertex from (projected) intersections
    pt = cm.get_intersection(edge1, edge2)
    far1 = v2 if (v1 - pt).length < (v2 - pt).length else v1
    far2 = v4 if (v3 - pt).length < (v4 - pt).length else v3
    # print('intersection: ', pt)

    dex1 = far1 - pt
    dex2 = far2 - pt
    dex1 = dex1 * (bdist / dex1.length)
    dex2 = dex2 * (bdist / dex2.length)
    pt2 = pt + (dex1).lerp(dex2, 0.5)
    # print('bisector point:', pt2)

    pt3 = pt2.lerp(pt, 2.0)

    vec1 = bm.verts.new(pt2)
    vec2 = bm.verts.new(pt)
    vec3 = bm.verts.new(pt3)
    bm.edges.new((vec1, vec2))
    bm.edges.new((vec2, vec3))
    bmesh.update_edit_mesh(me)
    # print("done")


class TCLineOnBisection(bpy.types.Operator):
    '''Generate the bisector of two selected edges'''
    bl_idname = 'tinycad.linetobisect'
    bl_label = 'BIX line to bisector'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return all([obj is not None, obj.type == 'MESH', obj.mode == 'EDIT'])

    def execute(self, context):
        add_line_to_bisection(self)
        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)
