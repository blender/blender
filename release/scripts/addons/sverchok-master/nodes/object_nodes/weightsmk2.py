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
from bpy.props import StringProperty, BoolProperty, FloatProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, second_as_first_cycle)


class SvVertexGroupNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' Vertex Group mk2'''
    bl_idname = 'SvVertexGroupNodeMK2'
    bl_label = 'Vertex group weights'
    bl_icon = 'OUTLINER_OB_EMPTY'

    fade_speed = FloatProperty(name='fade', default=2, update=updateNode)
    clear = BoolProperty(name='clear w', default=True, update=updateNode)
    group_name = StringProperty(default='Sv_VGroup', update=updateNode)

    def draw_buttons(self, context,   layout):
        layout.prop(self, "group_name", text="")

    def draw_buttons_ext(self, context, layout):
        lp = layout.prop
        lp(self,    "clear",   text="clear unindexed")
        lp(self, "fade_speed", text="Clearing speed")

    def sv_init(self, context):
        self.inputs.new('SvObjectSocket', "Object")
        self.inputs.new('StringsSocket', "VertIND")
        self.inputs.new('StringsSocket', "Weights")
        self.outputs.new('StringsSocket', "OutWeights")

    def process(self):
        Objs, Ve, We = self.inputs
        Owe = self.outputs[0]
        outobs = []
        for obj in self.inputs['Object'].sv_get():
            if not obj.vertex_groups:
                obj.vertex_groups.new(name=self.group_name)
            if self.group_name not in obj.vertex_groups:
                return
            ovgs = obj.vertex_groups.get(self.group_name)
            Vi = [i.index for i in obj.data.vertices]
            if Ve.is_linked:
                verts = Ve.sv_get()[0]
            else:
                verts = Vi
            if We.is_linked:
                if self.clear:
                    ovgs.add(Vi, self.fade_speed, "SUBTRACT")
                wei = second_as_first_cycle(verts, We.sv_get()[0])
                for i, i2 in zip(verts, wei):
                    ovgs.add([i], i2, "REPLACE")
            obj.data.update()
            if Owe.is_linked:
                out = []
                for i in verts:
                    try:
                        out.append(ovgs.weight(i))
                    except Exception:
                        out.append(0.0)  # ovgs.weight() error if vertex not in vgroup
                outobs.append(out)
        Owe.sv_set(outobs)


def register():
    bpy.utils.register_class(SvVertexGroupNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvVertexGroupNodeMK2)

if __name__ == '__main__':
    register()
