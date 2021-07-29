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
from bpy.props import StringProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


class svAxisInputNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Generator for X, Y or Z axis.

    This gives convenience in node view to make clear even in a minimized
    state which Vector-axis the node outputs. Especially useful for rotation input.

    outputs the vectors:
        X: [1,0,0]
        Y: [0,1,0]
        Z: [0,0,1]
    '''

    bl_idname = 'svAxisInputNode'
    bl_label = 'Vector X | Y | Z'    # shall default to Z Axis in svint.
    bl_icon = 'MANIPUL'

    def mode_change(self, context):
        if not (self.selected_axis == self.current_axis):
            self.label = self.selected_axis
            self.current_axis = self.selected_axis
            updateNode(self, context)

    axis_options = [
        ("X", "X", "", 0),
        ("Y", "Y", "", 1),
        ("Z", "Z", "", 2)
    ]
    current_axis = StringProperty(default='Z')

    selected_axis = EnumProperty(
        items=axis_options,
        name="Type of axis",
        description="offers basic axis output vectors X|Y|Z",
        default="Z",
        update=mode_change)

    def sv_init(self, context):
        self.width = 100
        self.outputs.new('VerticesSocket', "Vectors", "Vectors")
        self.bl_label = 'Z'

    def draw_buttons(self, context, layout):
        row = layout.row()
        row.prop(self, 'selected_axis', expand=True)

    def process(self):

        if self.outputs['Vectors'].is_linked:

            axial_vector = {
                'X': (1, 0, 0),
                'Y': (0, 1, 0),
                'Z': (0, 0, 1)
            }.get(self.current_axis, None)

            self.outputs[0].sv_set([[axial_vector]])


def register():
    bpy.utils.register_class(svAxisInputNode)


def unregister():
    bpy.utils.unregister_class(svAxisInputNode)
