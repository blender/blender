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

import bpy
import bmesh
import mathutils

from bpy.props import EnumProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.sv_bmesh_utils import pydata_from_bmesh, bmesh_from_pydata


def get2d(plane, vertices):
    if plane == 'Z':
        return [(v[0], v[1]) for v in vertices]
    elif plane == 'Y':
        return [(v[0], v[2]) for v in vertices]
    else:
        return [(v[1], v[2]) for v in vertices]


def make_hull(vertices, params):
    if not vertices:
        return False

    verts, faces = [], [[]]

    # invoke the right convex hull function
    if params.hull_mode == '3D':
        bm = bmesh_from_pydata(vertices, [], [])
        res = bmesh.ops.convex_hull(bm, input=bm.verts[:], use_existing_faces=False)
        unused_v_indices = [v.index for v in res["geom_unused"]]

        if params.inside and params.outside:
            verts, _, faces = pydata_from_bmesh(bm)

        elif not params.inside and params.outside:
            bmesh.ops.delete(bm, geom=[bm.verts[i] for i in unused_v_indices], context=1)
            verts, _, faces = pydata_from_bmesh(bm)

        elif not params.outside and params.inside:
            used_v_indices = set(range(len(vertices))) - set(unused_v_indices)
            bmesh.ops.delete(bm, geom=[bm.verts[i] for i in used_v_indices], context=1)
            verts = [v[:] for idx, v in enumerate(vertices) if idx in unused_v_indices]


    elif params.hull_mode == '2D':
        vertices_2d = get2d(params.plane, vertices)
        used_v_indices = mathutils.geometry.convex_hull_2d(vertices_2d)
        unused_v_indices = set(range(len(vertices))) - set(used_v_indices)

        bm = bmesh_from_pydata(vertices, [], [used_v_indices])

        if params.inside and params.outside:
            verts, _, faces = pydata_from_bmesh(bm)

        elif not params.inside and params.outside:
            bmesh.ops.delete(bm, geom=[bm.verts[i] for i in unused_v_indices], context=1)
            if params.sort_edges:
                bm.faces.ensure_lookup_table()
                addv = verts.append
                _ = [addv(v.co[:]) for v in bm.faces[0].verts[:]]
                faces = [list(range(len(verts)))]
            else:
                verts, _, faces = pydata_from_bmesh(bm)

        elif not params.outside and params.inside:
            bmesh.ops.delete(bm, geom=[bm.verts[i] for i in used_v_indices], context=1)
            verts, _, _ = pydata_from_bmesh(bm)


    bm.clear()
    bm.free()
    return (verts, faces)



class SvConvexHullNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' cvh 2D/3D conv.hull'''
    bl_idname = 'SvConvexHullNodeMK2'
    bl_label = 'Convex Hull MK2'
    # bl_icon = 'OUTLINER_OB_EMPTY'

    hull_mode_options = [(k, k, '', i) for i, k in enumerate(["3D", "2D"])]
    hull_mode = EnumProperty(
        description=" 3d or 2d?", default="3D", items=hull_mode_options, update=updateNode
    )

    plane_choices = [(k, k, '', i) for i, k in enumerate(["X", "Y", "Z"])]
    plane = EnumProperty(
        description="track 2D plane", default="X", items=plane_choices, update=updateNode
    )

    outside = BoolProperty(default=True, update=updateNode)
    inside = BoolProperty(default=False, update=updateNode)
    sort_edges = BoolProperty(default=True, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices')

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Polygons')

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        col.row().prop(self, 'hull_mode', expand=True)
        
        row = col.row(align=True)
        row.prop(self, 'inside', toggle=True)
        row.prop(self, 'outside', toggle=True)

        show_me = (self.hull_mode == '2D')
        col.separator()

        frow = col.row()
        frow.enabled = show_me
        frow.prop(self, 'plane', expand=True)

        frow2 = col.row()
        frow2.enabled = show_me and not self.inside
        frow2.prop(self, 'sort_edges', text='Topo Sort', toggle=True)

    def process(self):

        if self.inputs['Vertices'].is_linked:

            verts = self.inputs['Vertices'].sv_get()
            verts_out = []
            polys_out = []

            for v_obj in verts:
                res = make_hull(v_obj, self)
                if not res:
                    return

                verts_out.append(res[0])
                polys_out.append(res[1])

            self.outputs['Vertices'].sv_set(verts_out)
            self.outputs['Polygons'].sv_set(polys_out)


def register():
    bpy.utils.register_class(SvConvexHullNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvConvexHullNodeMK2)
