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

from mathutils import Vector

import bpy
from bpy.props import FloatProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat


class SvScaleNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Scale '''
    bl_idname = 'SvScaleNode'
    bl_label = 'Scale'
    bl_icon = 'MAN_SCALE'

    factor_ = FloatProperty(name='Factor', description='scaling factor',
                            default=1.0,
                            options={'ANIMATABLE'}, update=updateNode)

    Separate = BoolProperty(name='Separate', description='Separate UV coords',
                            default=False,
                            update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('VerticesSocket', "Center", "Center")
        self.inputs.new('StringsSocket', "Factor", "Factor").prop_name = "factor_"
        self.outputs.new('VerticesSocket', "Vertices", "Vertices")

    def draw_buttons(self, context, layout):
        layout.prop(self, 'Separate')

    def scaling(self, vertex, center, factor):
        pt = Vector(vertex)
        c = Vector(center)
        return (c + factor * (pt - c))[:]

    def vert_scl(self, vertex, center, factor):
        scaled = []
        params = match_long_repeat([center, factor])
        for c, f in zip(*params):
            scaled_ = []
            for v in vertex:
                scaled_.append(self.scaling(v, c, f))
            if self.Separate:
                scaled.append(scaled_)
            else:
                scaled.extend(scaled_)
        return scaled

    def process(self):
        # inputs
        Vertices = self.inputs['Vertices'].sv_get()
        Center = self.inputs['Center'].sv_get(default=[[[0.0, 0.0, 0.0]]])
        Factor = self.inputs['Factor'].sv_get()

        parameters = match_long_repeat([Vertices, Center, Factor])

        # outputs
        if self.outputs['Vertices'].is_linked:
            points = [self.vert_scl(v, c, f) for v, c, f in zip(*parameters)]
            self.outputs['Vertices'].sv_set(points)


def register():
    bpy.utils.register_class(SvScaleNode)


def unregister():
    bpy.utils.unregister_class(SvScaleNode)
