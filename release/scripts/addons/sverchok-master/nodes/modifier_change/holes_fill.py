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
from bpy.props import IntProperty
import bmesh

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, repeat_last, dataCorrect
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh


def fill_holes(vertices, edges, s):

    if not edges and not vertices:
        return False

    if len(edges[0]) != 2:
        return False
    
    bm = bmesh_from_pydata(vertices, edges, [])

    bmesh.ops.holes_fill(bm, edges=bm.edges[:], sides=s)
    verts, edges, faces = pydata_from_bmesh(bm)
    return (verts, edges, faces)


class SvFillHolesNode(bpy.types.Node, SverchCustomTreeNode):
    '''Fills holes'''
    bl_idname = 'SvFillsHoleNode'
    bl_label = 'Fill Holes'
    bl_icon = 'OUTLINER_OB_EMPTY'

    sides = IntProperty(
        name='Sides', description='Side to fill',
        default=4, min=3,
        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vertices')
        self.inputs.new('StringsSocket', 'edges')
        self.inputs.new('StringsSocket', 'Sides').prop_name = 'sides'

        self.outputs.new('VerticesSocket', 'vertices')
        self.outputs.new('StringsSocket', 'edges')
        self.outputs.new('StringsSocket', 'polygons')

    def process(self):

        if not (self.inputs['vertices'].is_linked and self.inputs['edges'].is_linked):
            return

        verts = dataCorrect(self.inputs['vertices'].sv_get())
        edges = dataCorrect(self.inputs['edges'].sv_get())
        sides = repeat_last(self.inputs['Sides'].sv_get()[0])
        verts_out = []
        edges_out = []
        polys_out = []

        for v, e, s in zip(verts, edges, sides):
            res = fill_holes(v, e, int(s))
            if not res:
                return
            verts_out.append(res[0])
            edges_out.append(res[1])
            polys_out.append(res[2])

        self.outputs['vertices'].sv_set(verts_out)
        self.outputs['edges'].sv_set(edges_out)
        self.outputs['polygons'].sv_set(polys_out)



def register():
    bpy.utils.register_class(SvFillHolesNode)


def unregister():
    bpy.utils.unregister_class(SvFillHolesNode)
