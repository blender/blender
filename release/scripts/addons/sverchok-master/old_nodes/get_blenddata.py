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
from bpy.props import EnumProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, enum_item as e)


class SvGetDataObjectNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Get Object Props '''
    bl_idname = 'SvGetDataObjectNode'
    bl_label = 'Object ID Get'
    bl_icon = 'OUTLINER_OB_EMPTY'

    M = ['actions','brushes','filepath','grease_pencil','groups',
         'images','libraries','linestyles','masks','materials',
         'movieclips','node_groups','particles','scenes','screens','shape_keys',
         'sounds','speakers','texts','textures','worlds','objects']
    T = ['MESH','CURVE','SURFACE','META','FONT','ARMATURE','LATTICE','EMPTY','CAMERA','LAMP','SPEAKER']

    Modes = EnumProperty(name="getmodes", default="objects", items=e(M), update=updateNode)
    Types = EnumProperty(name="getmodes", default="MESH", items=e(T), update=updateNode)

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        layout.prop(self, "Modes", text="mode")
        if self.Modes == 'objects':
            layout.prop(self, "Types", "type")

    def sv_init(self, context):
        self.outputs.new('StringsSocket', "Objects")

    def process(self):
        sob = self.outputs['Objects']
        if not sob.is_linked:
            return
        L = getattr(bpy.data,self.Modes)
        if self.Modes != 'objects':
            sob.sv_set(L[:])
        else:
            sob.sv_set([i for i in L if i.type == self.Types])


def register():
    bpy.utils.register_class(SvGetDataObjectNode)


def unregister():
    bpy.utils.unregister_class(SvGetDataObjectNode)
