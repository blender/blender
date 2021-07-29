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
from bpy.props import IntProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, SvSetSocketAnyType, SvGetSocketAnyType


class GenRangeNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Generator: Range '''
    bl_idname = 'GenRangeNode'
    bl_label = 'List Range'
    bl_icon = 'OUTLINER_OB_EMPTY'

    start_ = FloatProperty(name='start', description='start',
                           default=0,
                           options={'ANIMATABLE'}, update=updateNode)
    stop_ = FloatProperty(name='stop', description='stop',
                          default=1,
                          options={'ANIMATABLE'}, update=updateNode)
    divisions_ = IntProperty(name='divisions', description='divisions',
                             default=10, min=2,
                             options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Start", "Start")
        self.inputs.new('StringsSocket', "Stop", "Stop")
        self.inputs.new('StringsSocket', "Divisions", "Divisons")
        self.outputs.new('StringsSocket', "Range", "Range")

    def draw_buttons(self, context, layout):
        layout.prop(self, "start_", text="start")
        layout.prop(self, "stop_", text="stop")
        layout.prop(self, "divisions_", text="divisons")

    def process(self):
        # inputs
        if 'Start' in self.inputs and self.inputs['Start'].links:
            tmp = SvGetSocketAnyType(self, self.inputs['Start'])
            Start = tmp[0][0]
        else:
            Start = self.start_

        if 'Stop' in self.inputs and self.inputs['Stop'].links:
            tmp = SvGetSocketAnyType(self, self.inputs['Stop'])
            Stop = tmp[0][0]
        else:
            Stop = self.stop_

        if 'Divisions' in self.inputs and self.inputs['Divisions'].links:
            tmp = SvGetSocketAnyType(self, self.inputs['Divisions'])
            Divisions = tmp[0][0]
        else:
            Divisions = self.divisions_

        # outputs
        if 'Range' in self.outputs and self.outputs['Range'].links:
            if Divisions < 2:
                Divisions = 2
            Range = [Start]
            if Divisions > 2:
                Range.extend([c for c in self.xfrange(Start, Stop, Divisions)])
            Range.append(Stop)
            SvSetSocketAnyType(self, 'Range', [Range])

    def xfrange(self, start, stop, divisions):
        step = (stop - start) / (divisions - 1)
        count = start
        for i in range(divisions - 2):
            count += step
            yield count



def register():
    bpy.utils.register_class(GenRangeNode)


def unregister():
    bpy.utils.unregister_class(GenRangeNode)
