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
from bpy.props import EnumProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, enum_item as e)


class SvBMVertsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' BMesh Verts '''
    bl_idname = 'SvBMVertsNode'
    bl_label = 'BMesh props'
    bl_icon = 'OUTLINER_OB_EMPTY'

    Modes = ['verts','faces','edges']
    Mod = EnumProperty(name="getmodes", default="verts", items=e(Modes), update=updateNode)
    a = ['hide','select']
    PV = a + ['is_manifold','is_wire','is_boundary','calc_shell_factor()','calc_edge_angle(-1)']
    PF = a + ['calc_area()','calc_perimeter()','material_index','smooth']
    PE = a + ['calc_face_angle(0)','calc_face_angle_signed(0)','calc_length()','is_boundary','is_contiguous','is_convex','is_manifold','is_wire','seam']
    verts = EnumProperty(name="Vprop", default="is_manifold", items=e(PV), update=updateNode)
    faces = EnumProperty(name="Fprop", default="select", items=e(PF), update=updateNode)
    edges = EnumProperty(name="Eprop", default="select", items=e(PE), update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'bmesh_list')
        self.outputs.new('StringsSocket', 'Value')

    def draw_buttons(self, context, layout):
        layout.prop(self, "Mod", "Get")
        layout.prop(self, self.Mod, "")

    def process(self):
        V = []
        bm = self.inputs['bmesh_list'].sv_get()
        elem = getattr(self, self.Mod)
        exec("for b in bm:\n    bv = getattr(b, self.Mod)\n    V.append([i."+elem+" for i in bv])")
        self.outputs['Value'].sv_set(V)

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvBMVertsNode)


def unregister():
    bpy.utils.unregister_class(SvBMVertsNode)
