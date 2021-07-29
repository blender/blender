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

import colorsys
import bpy
from bpy.props import FloatProperty, BoolProperty, FloatVectorProperty
from mathutils import Color

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, fullList, dataCorrect
from sverchok.utils.sv_itertools import sv_zip_longest

# pylint: disable=w0141


class SvColorsOutNodeMK1(bpy.types.Node, SverchCustomTreeNode):
    ''' color ---> rgb(a) /// Generator for Color data'''
    bl_idname = 'SvColorsOutNodeMK1'
    bl_label = 'Color Out MK1'
    sv_icon = 'SV_COMBINE_OUT'

    def psuedo_update(self, context):
        for idx, socket in enumerate(self.selected_mode):
            self.outputs[idx].name = socket
        updateNode(self, context)

    unit_color = FloatVectorProperty(
        update=updateNode, name='', default=(.3, .3, .2, 1.0),
        size=4, min=0.0, max=1.0, subtype='COLOR'
    )

    use_alpha = BoolProperty(default=False, update=updateNode)

    def sv_init(self, context):
        self.width = 110
        inew = self.inputs.new
        inew('SvColorSocket', "Colors").prop_name = "unit_color"
        onew = self.outputs.new
        onew('StringsSocket', "R")
        onew('StringsSocket', "G")
        onew('StringsSocket', "B")
        onew('StringsSocket', "A")

    def draw_buttons(self, context, layout):
        layout.prop(self, 'use_alpha')

    def process(self):
        """
        colorsys.rgb_to_yiq(r, g, b)
        colorsys.yiq_to_rgb(y, i, q)
        colorsys.rgb_to_hls(r, g, b)
        colorsys.hls_to_rgb(h, l, s)
        colorsys.rgb_to_hsv(r, g, b)
        colorsys.hsv_to_rgb(h, s, v)
        """

        color_input = self.inputs['Colors']
        if color_input.is_linked:
            abc = self.inputs['Colors'].sv_get()
            data = dataCorrect(abc)
        else:
            data = [[self.unit_color[:]]]

        A, B, C, D = [], [], [], []
        if self.use_alpha:
            for obj in data:
                a_, b_, c_, d_ = (list(x) for x in zip(*obj))
                A.append(a_)
                B.append(b_)
                C.append(c_)
                D.append(d_)
            for i, socket in enumerate(self.outputs):
                self.outputs[socket.name].sv_set([A, B, C, D][i])
        else:
            for obj in data:
                a_, b_, c_ = (list(x) for x in zip(*obj))
                A.append(a_)
                B.append(b_)
                C.append(c_)
            for i, socket in enumerate(self.outputs[:3]):
                self.outputs[socket.name].sv_set([A, B, C][i])


def register():
    bpy.utils.register_class(SvColorsOutNodeMK1)


def unregister():
    bpy.utils.unregister_class(SvColorsOutNodeMK1)
