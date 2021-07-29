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
from bpy.props import IntProperty, EnumProperty, FloatProperty, StringProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat


def frange(start, stop, step):
    '''Behaves like range but for floats'''
    if start == stop:
        stop += 1
    step = max(1e-5, abs(step))
    if start < stop:
        while start < stop:
            yield start
            start += step
    else:
        step = -abs(step)
        while start > stop:
            yield start
            start += step


def frange_count(start, stop, count):
    ''' Gives count total values in [start,stop] '''
    if count < 2:
        yield start
    else:
        count = int(count)
        step = (stop - start) / (count - 1)
        yield start
        for i in range(count - 2):
            start += step
            yield start
        yield stop


def frange_step(start, step, count):
    ''' Gives count values with step from start'''
    if abs(step) < 1e-5:
        step = 1
    for i in range(int(count)):
        yield start
        start += step


class SvGenFloatRange(bpy.types.Node, SverchCustomTreeNode):
    ''' Generator range list of floats'''
    bl_idname = 'SvGenFloatRange'
    bl_label = 'Range Float'
    bl_icon = 'OUTLINER_OB_EMPTY'

    start_ = FloatProperty(
        name='start', description='start',
        default=0,
        options={'ANIMATABLE'}, update=updateNode)

    stop_ = FloatProperty(
        name='stop', description='stop',
        default=10,
        options={'ANIMATABLE'}, update=updateNode)
    count_ = IntProperty(
        name='count', description='num items',
        default=10,
        options={'ANIMATABLE'}, min=1, update=updateNode)

    step_ = FloatProperty(
        name='step', description='step',
        default=1.0,
        options={'ANIMATABLE'}, update=updateNode)

    current_mode = StringProperty(default="FRANGE")

    modes = [
        ("FRANGE", "Range", "Series based frange like function", 1),
        ("FRANGE_COUNT", "Count", "Create series based on count", 2),
        ("FRANGE_STEP", "Step", "Create range based step and count", 3),
    ]

    def mode_change(self, context):

        # just because click doesn't mean we need to change mode
        mode = self.mode
        if mode == self.current_mode:
            return

        if mode == 'FRANGE':
            self.inputs[1].prop_name = 'stop_'
            self.inputs[2].prop_name = 'step_'
        elif mode == 'FRANGE_COUNT':
            self.inputs[1].prop_name = 'stop_'
            self.inputs[2].prop_name = 'count_'
        else:
            self.inputs[1].prop_name = 'step_'
            self.inputs[2].prop_name = 'count_'

        self.current_mode = mode
        updateNode(self, context)

    mode = EnumProperty(items=modes, default='FRANGE', update=mode_change)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Start").prop_name = 'start_'
        self.inputs.new('StringsSocket', "Step").prop_name = 'stop_'
        self.inputs.new('StringsSocket', "Stop").prop_name = 'step_'

        self.outputs.new('StringsSocket', "Range", "Range")

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode", expand=True)

    func_dict = {'FRANGE': frange,
                 'FRANGE_COUNT': frange_count,
                 'FRANGE_STEP': frange_step}

    def process(self):
        inputs = self.inputs
        outputs = self.outputs
        if not outputs[0].is_linked:
            return
        param = [inputs[i].sv_get()[0] for i in range(3)]
        f = self.func_dict[self.mode]
        out = [list(f(*args)) for args in zip(*match_long_repeat(param))]
        outputs['Range'].sv_set(out)


def register():
    bpy.utils.register_class(SvGenFloatRange)


def unregister():
    bpy.utils.unregister_class(SvGenFloatRange)
