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
from sverchok.data_structure import (updateNode)


class SvFilterObjsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Filter Objects '''
    bl_idname = 'SvFilterObjsNode'
    bl_label = 'Object ID Filter'
    bl_icon = 'OUTLINER_OB_EMPTY'

    formula = StringProperty(name='formula', default='write name here', update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Objects')
        self.inputs.new('StringsSocket', 'mask')
        self.outputs.new('StringsSocket', 'Objects(have)')
        self.outputs.new('StringsSocket', 'Objects(not)')

    def draw_buttons(self, context, layout):
        if not self.inputs['mask'].is_linked:
            layout.prop(self,  "formula", text="")

    def process(self):
        In1,In2 = self.inputs
        O1,O2 = self.outputs
        objs = In1.sv_get()
        if isinstance(objs[0], list):
            objs = objs[0]
        out1,out2 = [],[]
        if In2.is_linked:
            m = In2.sv_get()
            if isinstance(m[0], list):
                m = m[0]
            for i, i2 in zip(objs, m):
                if i2 == 1:
                    out1.append(i)
                else:
                    out2.append(i)
        else:
            S = self.formula
            for i in objs:
                if S in i.name:
                    out1.append(i)
                else:
                    out2.append(i)
        O1.sv_set(out1)
        O2.sv_set(out2)

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvFilterObjsNode)


def unregister():
    bpy.utils.unregister_class(SvFilterObjsNode)
