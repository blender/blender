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
import sys

from bpy.props import FloatProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata

from sverchok.utils.csg_core import CSG

def Boolean(VA, PA, VB, PB, operation):
    if not all([VA, PA, VB, PB]):
        return False, False

    a = CSG.Obj_from_pydata(VA, PA)
    b = CSG.Obj_from_pydata(VB, PB)

    faces = []
    vertices = []

    recursionlimit = sys.getrecursionlimit()
    sys.setrecursionlimit(10000)
    try:
        if operation == 'DIFF':
            polygons = a.subtract(b).toPolygons()
        elif operation == 'JOIN':
            polygons = a.union(b).toPolygons()
        elif operation == 'ITX':
            polygons = a.intersect(b).toPolygons()
    except RuntimeError as e:
        raise RuntimeError(e)

    sys.setrecursionlimit(recursionlimit)

    for polygon in polygons:
        indices = []
        for v in polygon.vertices:
            pos = [v.pos.x, v.pos.y, v.pos.z]
            if not pos in vertices:
                vertices.append(pos)
            index = vertices.index(pos)
            indices.append(index)

        faces.append(indices)

    return [vertices], [faces]


class SvCSGBooleanNode(bpy.types.Node, SverchCustomTreeNode):
    '''CSG Boolean Node'''
    bl_idname = 'SvCSGBooleanNode'
    bl_label = 'CSG Boolean'
    bl_icon = 'OUTLINER_OB_EMPTY'

    mode_options = [
        ("ITX", "Intersect", "", 0),
        ("JOIN", "Join", "", 1),
        ("DIFF", "Diff", "", 2)
    ]

    selected_mode = EnumProperty(
        items=mode_options,
        description="offers basic booleans using CSG",
        default="ITX",
        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Verts A')
        self.inputs.new('StringsSocket',  'Polys A')
        self.inputs.new('VerticesSocket', 'Verts B')
        self.inputs.new('StringsSocket',  'Polys B')

        self.outputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.outputs.new('StringsSocket', 'Polygons', 'Polygons')

    def draw_buttons(self, context, layout):
        row = layout.row()
        row.prop(self, 'selected_mode', expand=True)

    def process(self):
        for i in range(4):
            if not self.inputs[i].is_linked:
                return

        if not self.outputs['Vertices'].is_linked:
            return

        VA = self.inputs['Verts A'].sv_get()[0]
        PA = self.inputs['Polys A'].sv_get()[0]
        VB = self.inputs['Verts B'].sv_get()[0]
        PB = self.inputs['Polys B'].sv_get()[0]

        verts_out, polys_out = Boolean(VA, PA, VB, PB, self.selected_mode)

        self.outputs['Vertices'].sv_set(verts_out)
        self.outputs['Polygons'].sv_set(polys_out)


def register():
    bpy.utils.register_class(SvCSGBooleanNode)


def unregister():
    bpy.utils.unregister_class(SvCSGBooleanNode)
