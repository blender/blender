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
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty

import time
import re

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, SvSetSocketAnyType
from sverchok.utils.sv_easing_functions import *

DEBUG=False

typeItems = [
    ("INT", "Integer", "", "", 0),
    ("FLOAT", "Float", "", "", 1)]

interplationItems = [
    ("LINEAR", "Linear", "", "IPO_LINEAR", 0),
    ("SINUSOIDAL", "Sinusoidal", "", "IPO_SINE", 1),
    ("QUADRATIC", "Quadratic", "", "IPO_QUAD", 2),
    ("CUBIC", "Cubic", "", "IPO_CUBIC", 3),
    ("QUARTIC", "Quartic", "", "IPO_QUART", 4),
    ("QUINTIC", "Quintic", "", "IPO_QUINT", 5),
    ("EXPONENTIAL", "Exponential", "", "IPO_EXPO", 6),
    ("CIRCULAR", "Circular", "", "IPO_CIRC", 7),
    # DYNAMIC effects
    ("BACK", "Back", "", "IPO_BACK", 8),
    ("BOUNCE", "Bounce", "", "IPO_BOUNCE", 9),
    ("ELASTIC", "Elastic", "", "IPO_ELASTIC", 10)]

easingItems = [
    ("EASE_IN", "Ease In", "", "IPO_EASE_IN", 0),
    ("EASE_OUT", "Ease Out", "", "IPO_EASE_OUT", 1),
    ("EASE_IN_OUT", "Ease In-Out", "", "IPO_EASE_IN_OUT", 2)]


class SvMixNumbersNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Mix Numbers '''
    bl_idname = 'SvMixNumbersNode'
    bl_label = 'Mix Numbers'
    bl_icon = 'IPO'

    # SV easing based interpolator
    def getInterpolator(self):
        # get the interpolator function based on selected interpolation and easing
        if self.interpolation == "LINEAR":
            return LinearInterpolation
        else:
            ''' This maps the Strings used in the Enumerator properties to the associated function'''
            interpolatorName = self.interpolation + "_" + self.easing
            interpolatorName = re.sub('SINUSOIDAL', 'sine', interpolatorName)  # for the exception
            interpolate = globals()[re.sub(r'[_]', '', interpolatorName.lower().title())]

            # setup the interpolator with prepared parameters
            if self.interpolation == "EXPONENTIAL":
                b = self.exponentialBase
                e = self.exponentialExponent
                settings = prepareExponentialSettings(b, e)
                return lambda v : interpolate(v, settings)

            elif self.interpolation == "BACK":
                s = self.backScale
                return lambda v : interpolate(v, s)

            elif self.interpolation == "ELASTIC":
                n = self.elasticBounces
                b = self.elasticBase
                e = self.elasticExponent
                settings = prepareElasticSettings(n, b, e)
                return lambda v : interpolate(v, settings)

            elif self.interpolation == "BOUNCE":
                n = self.bounceBounces
                a = self.bounceAttenuation
                settings = prepareBounceSettings(n, a)
                return lambda v : interpolate(v, settings)

            else:
                return interpolate


    def update_type(self, context):
        if self.numType == 'INT':
            self.inputs['v1'].prop_name = "value_int1"
            self.inputs['v2'].prop_name = "value_int2"
        else: # float type
            self.inputs['v1'].prop_name = "value_float1"
            self.inputs['v2'].prop_name = "value_float2"

        updateNode(self, context)


    numType = EnumProperty(
        name="Number Type",
        default="FLOAT", items=typeItems,
        update=update_type)

    # INTERPOLATION settings
    interpolation = EnumProperty(
        name="Interpolation",
        default="LINEAR", items=interplationItems,
        update=updateNode)

    easing = EnumProperty(
        name="Easing",
        default="EASE_IN_OUT", items=easingItems,
        update=updateNode)

    # BACK interpolation settings
    backScale = FloatProperty(
        name="Scale", description="Back scale",
        default=0.5, soft_min=0.0, soft_max=10.0,
        update=updateNode)

    # ELASTIC interpolation settings
    elasticBase = FloatProperty(
        name="Base", description="Elastic base",
        default=1.6, soft_min=0.0, soft_max=10.0,
        update=updateNode)

    elasticExponent = FloatProperty(
        name="Exponent", description="Elastic exponent",
        default=6.0, soft_min=0.0, soft_max=10.0,
        update=updateNode)

    elasticBounces = IntProperty(
        name="Bounces", description="Elastic bounces",
        default=6, soft_min=1, soft_max=10,
        update=updateNode)

    # EXPONENTIAL interpolation settings
    exponentialBase = FloatProperty(
        name="Base", description="Exponential base",
        default=2.0, soft_min=0.0, soft_max=10.0,
        update=updateNode)

    exponentialExponent = FloatProperty(
        name="Exponent", description="Exponential exponent",
        default=10.0, soft_min=0.0, soft_max=20.0,
        update=updateNode)

    # BOUNCE interpolation settings
    bounceAttenuation = FloatProperty(
        name="Attenuation", description="Bounce attenuation",
        default=0.5, soft_min=0.1, soft_max=0.9,
        update=updateNode)

    bounceBounces = IntProperty(
        name="Bounces", description="Bounce bounces",
        default=4, soft_min=1, soft_max=10,
        update=updateNode)

    # INPUT sockets settings
    value_float1 = FloatProperty(
        name="Value 1", description="Mix FLOAT value 1",
        default=0.0,
        update=updateNode)

    value_float2 = FloatProperty(
        name="Value 2", description="Mix FLOAT value 2",
        default=1.0,
        update=updateNode)

    value_int1 = IntProperty(
        name="Value 1", description="Mix INT value 1",
        default=0,
        update=updateNode)

    value_int2 = IntProperty(
        name="Value 2", description="Mix INT value 2",
        default=1,
        update=updateNode)

    factor = FloatProperty(
        name="Factor", description="Factor value",
        default=0.5, min=0.0, max=1.0,
        update=updateNode)


    def sv_init(self, context):
        self.width = 180
        self.inputs.new('StringsSocket', "v1").prop_name = 'value_float1'
        self.inputs.new('StringsSocket', "v2").prop_name = 'value_float2'
        self.inputs.new('StringsSocket', "f").prop_name = 'factor'

        self.outputs.new('StringsSocket', "Value")


    def draw_buttons(self, context, layout):
        layout.prop(self, 'numType', expand=True)
        layout.prop(self, 'interpolation', expand=False)
        layout.prop(self, 'easing', expand=False)


    def draw_buttons_ext(self, context, layout):
        if self.interpolation == "BACK":
            layout.column().label(text="Interpolation:")
            box = layout.box()
            box.prop(self, 'backScale')

        elif self.interpolation == "ELASTIC":
            layout.column().label(text="Interpolation:")
            box = layout.box()
            box.prop(self, 'elasticBase')
            box.prop(self, 'elasticExponent')
            box.prop(self, 'elasticBounces')

        elif self.interpolation == "EXPONENTIAL":
            layout.column().label(text="Interpolation:")
            box = layout.box()
            box.prop(self, 'exponentialBase')
            box.prop(self, 'exponentialExponent')

        elif self.interpolation == "BOUNCE":
            layout.column().label(text="Interpolation:")
            box = layout.box()
            box.prop(self, 'bounceAttenuation')
            box.prop(self, 'bounceBounces')


    def process(self):
        # return if no outputs are connected
        if not any(s.is_linked for s in self.outputs):
            return

        # input values lists (single or multi value)
        input_value1 = self.inputs["v1"].sv_get()[0]
        input_value2 = self.inputs["v2"].sv_get()[0]
        input_factor = self.inputs["f"].sv_get()[0]

        parameters = match_long_repeat([input_value1, input_value2, input_factor])

        interpolate = self.getInterpolator()

        values=[]
        for v1, v2, f in zip(*parameters):
            t = interpolate(f)
            v = v1*(1-t) + v2*t

            values.append(v)

        self.outputs['Value'].sv_set([values])


def register():
    bpy.utils.register_class(SvMixNumbersNode)


def unregister():
    bpy.utils.unregister_class(SvMixNumbersNode)

if __name__ == '__main__':
    register()
