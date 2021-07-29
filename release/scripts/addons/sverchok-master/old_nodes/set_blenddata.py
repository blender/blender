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
from bpy.props import StringProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, second_as_first_cycle)


class SvSetDataObjectNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Set Object Props '''
    bl_idname = 'SvSetDataObjectNode'
    bl_label = 'Object ID Set'
    bl_icon = 'OUTLINER_OB_EMPTY'

    formula = StringProperty(name='formula', default='delta_location', update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self,  "formula", text="")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Objects')
        self.inputs.new('StringsSocket', 'values')
        self.outputs.new('StringsSocket', 'outvalues')

    def process(self):
        O, V = self.inputs
        Ov = self.outputs[0]
        objs = O.sv_get()
        if isinstance(objs[0], list):
            objs = objs[0]
        Prop = self.formula
        if V.is_linked:
            v = V.sv_get()
            if isinstance(v[0], list):
                v = v[0]
            v = second_as_first_cycle(objs, v)
            exec("for i, i2 in zip(objs, v):\n    i."+Prop+"= i2")
        elif Ov.is_linked:
            Ov.sv_set(eval("[i."+Prop+" for i in objs]"))
        else:
            exec("for i in objs:\n    i."+Prop)


def register():
    bpy.utils.register_class(SvSetDataObjectNode)


def unregister():
    bpy.utils.unregister_class(SvSetDataObjectNode)
