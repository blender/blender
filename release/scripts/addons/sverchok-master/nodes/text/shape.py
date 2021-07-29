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
from sverchok.data_structure import updateNode, describe_data_shape

class SvDataShapeNode(bpy.types.Node, SverchCustomTreeNode):
    """
    Triggers: Shape
    Tooltip: Inspect shape of input data
    """
    bl_idname = "SvDataShapeNode"
    bl_label = "Data shape"
    bl_icon = 'OUTLINER_OB_EMPTY'

    text = StringProperty(name='Text')

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Data")
        self.outputs.new('StringsSocket', "Text Out")

    def draw_buttons(self, context, layout):
        layout.label(text=self.text)

    def process(self):
        if self.inputs['Data'].is_linked:
            self.text = describe_data_shape(self.inputs['Data'].sv_get())
            self.outputs['Text Out'].sv_set(self.text)
        else:
            self.text = "No data"


    def copy(self, node):
        self.text = ""

def register():
    bpy.utils.register_class(SvDataShapeNode)

def unregister():
    bpy.utils.unregister_class(SvDataShapeNode)

