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

from sverchok.node_tree import SverchCustomTreeNode, StringsSocket


class Float2IntNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Float2Int '''
    bl_idname = 'Float2IntNode'
    bl_label = 'Float to Int'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "float", "float")
        self.outputs.new('StringsSocket', "int", "int")

    def process(self):
        Number = self.inputs['float'].sv_get()
        if self.outputs['int'].is_linked:
            result = self.inte(Number)
            self.outputs['int'].sv_set(result)

    @classmethod
    def inte(cls, l):
        if isinstance(l, (int, float)):
            return round(l)
        else:
            return [cls.inte(i) for i in l]


def register():
    bpy.utils.register_class(Float2IntNode)


def unregister():
    bpy.utils.unregister_class(Float2IntNode)
