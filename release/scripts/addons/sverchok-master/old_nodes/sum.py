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

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import SvSetSocketAnyType, SvGetSocketAnyType


class ListSumNode(bpy.types.Node, SverchCustomTreeNode):
    ''' List summa WIP, not ready yet '''
    bl_idname = 'ListSumNode'
    bl_label = 'List summa'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Data", "Data")
        self.outputs.new('StringsSocket', "Sum", "Sum")

    def process(self):
        # достаём два слота - вершины и полики
        if 'Sum' in self.outputs and self.outputs['Sum'].is_linked:
            if 'Data' in self.inputs and self.inputs['Data'].is_linked:
                data = SvGetSocketAnyType(self, self.inputs['Data'])

                out_ = self.summ(data)
                #    print(out_)
                out = [[sum(out_)]]

                SvSetSocketAnyType(self, 'Sum', out)

    def summ(self, data):
        out = []

        if data and (type(data[0]) in [type(1.2), type(1)]):
            for obj in data:
                out.append(obj)
            #print (data)
        elif data and (type(data[0]) in [tuple, list]):
            for obj in data:
                out.extend(self.summ(obj))
        return out


def register():
    bpy.utils.register_class(ListSumNode)


def unregister():
    bpy.utils.unregister_class(ListSumNode)
