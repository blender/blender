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


class SvSortObjsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Sort Objects '''
    bl_idname = 'SvSortObjsNode'
    bl_label = 'Object ID Sort'
    bl_icon = 'OUTLINER_OB_EMPTY'

    Modes = StringProperty(name='formula', default='location.x', update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Objects')
        self.inputs.new('StringsSocket', 'CustomValue')
        self.outputs.new('StringsSocket', 'Objects')

    def draw_buttons(self, context, layout):
        if not self.inputs['CustomValue'].is_linked:
            layout.prop(self, "Modes", text="")

    def process(self):
        Io, Cv = self.inputs
        Oo = self.outputs[0]
        if Oo.is_linked:
            X = Io.sv_get()
            if isinstance(X[0], list):
                X = X[0]
            if Cv.is_linked:
                CV = Cv.sv_get()
                Y = CV[0] if isinstance(CV[0],list) else CV
            else:
                Y = eval("[i."+self.Modes+" for i in X]")
            X.sort(key=dict(zip(X, Y)).get)
            Oo.sv_set(X)

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvSortObjsNode)


def unregister():
    bpy.utils.unregister_class(SvSortObjsNode)
