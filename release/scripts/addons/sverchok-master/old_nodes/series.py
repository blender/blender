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
from bpy.props import FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, SvSetSocketAnyType, SvGetSocketAnyType


class GenSeriesNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Generator series '''
    bl_idname = 'GenSeriesNode'
    bl_label = 'List Series'
    bl_icon = 'OUTLINER_OB_EMPTY'

    start_ = FloatProperty(name='start', description='start',
                           default=0,
                           options={'ANIMATABLE'}, update=updateNode)
    stop_ = FloatProperty(name='stop', description='stop',
                          default=10,
                          options={'ANIMATABLE'}, update=updateNode)
    step_ = FloatProperty(name='step', description='step',
                          default=1,
                          options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Start", "Start")
        self.inputs.new('StringsSocket', "Stop", "Stop")
        self.inputs.new('StringsSocket', "Step", "Step")
        self.outputs.new('StringsSocket', "Series", "Series")

    def draw_buttons(self, context, layout):
        layout.prop(self, "start_", text="start")
        layout.prop(self, "stop_", text="stop")
        layout.prop(self, "step_", text="step")

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

        if 'Step' in self.inputs and self.inputs['Step'].links:
            tmp = SvGetSocketAnyType(self, self.inputs['Step'])
            Step = tmp[0][0]
        else:
            Step = self.step_

        # outputs
        if 'Series' in self.outputs and len(self.outputs['Series'].links) > 0:
            #print (Start, Stop, Step)
            if Step < 0:
                Step = 1
            if Stop < Start:
                Stop = Start+1
            series = [c for c in self.xfrange(Start, Stop, Step)]

            SvSetSocketAnyType(self, 'Series', [series])

    def xfrange(self, start, stop, step):
        while start < stop:
            yield start
            start += step

def register():
    bpy.utils.register_class(GenSeriesNode)


def unregister():
    bpy.utils.unregister_class(GenSeriesNode)
