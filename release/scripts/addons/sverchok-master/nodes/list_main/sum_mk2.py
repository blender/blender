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
from sverchok.data_structure import levelsOflist, updateNode
from bpy.props import IntProperty


class ListSumNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' List summa MK2 '''
    bl_idname = 'ListSumNodeMK2'
    bl_label = 'List Sum'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level = IntProperty(name='level_to_count',
                        default=1, min=1,
                        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Data", "Data")
        self.outputs.new('StringsSocket', "Sum", "Sum")

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="level")

    def process(self):
        # достаём два слота - вершины и полики
        if 'Sum' in self.outputs and self.outputs['Sum'].is_linked:
            if 'Data' in self.inputs and self.inputs['Data'].is_linked:
                data = self.inputs['Data'].sv_get()

                lol = levelsOflist(data) - 1
                level = min(lol, self.level)
                out = self.summ(data, level, lol)

                if self.level == 1:
                    out = [out]
                self.outputs['Sum'].sv_set(out)

    def summ(self, data, level, lol):
        out = []
        if level == 0  and lol > 0:
            for obj in data:
                print(obj)
                out.append(self.summ(obj,level,lol-1))
            return sum(out)
        elif level == 0 and lol == 0:
            return sum(data)
        elif level > 0 and lol > 0:
            for obj in data:
                out.append(self.summ(obj,level-1,lol-1))
        else:
            return data
        return out


def register():
    bpy.utils.register_class(ListSumNodeMK2)


def unregister():
    bpy.utils.unregister_class(ListSumNodeMK2)

if __name__ == '__main__':
    register()
