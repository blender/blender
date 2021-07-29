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
from mathutils import Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (fullList, updateNode)


def interp_v3_v3v3(a, b, t=0.5):
    if t == 0.0: return a
    elif t == 1.0: return b
    else:
        s = 1.0 - t
        return (s * a[0] + t * b[0], s * a[1] + t * b[1], s * a[2] + t * b[2])


class SvVectorLerp(bpy.types.Node, SverchCustomTreeNode):
    ''' Linear Interpolation between two vectors '''
    bl_idname = 'SvVectorLerp'
    bl_label = 'Vector Lerp'
    bl_icon = 'OUTLINER_OB_EMPTY'

    factor_ = FloatProperty(
        name='factor', description='Step length',
        default=0.5, soft_min=0.0, soft_max=1.0,
        options={'ANIMATABLE'}, update=updateNode)

    process_mode = bpy.props.EnumProperty(
        items=[(t, t, '', idx) for idx, t in enumerate(["Lerp", "Evaluate"])],
        description="choose LERP or Evaluate",
        default="Lerp", update=updateNode
    )

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Factor", "Factor").prop_name = 'factor_'
        self.inputs.new('VerticesSocket', "Vertices A")
        self.inputs.new('VerticesSocket', "Vertices B")
        self.outputs.new('VerticesSocket', "EvPoint", "EvPoint")

    def draw_buttons(self, context, layout):
        layout.prop(self, 'process_mode', text='Evaluate', expand=True)

    def process(self):
        if not self.outputs['EvPoint'].is_linked:
            return
        VerticesA = self.inputs[1].sv_get()
        VerticesB = self.inputs[2].sv_get()
        factor = self.inputs['Factor'].sv_get()

        # match inputs using fullList, longest list matching on A and B
        # extend factor list if necessary, it should not control length of output

        max_obj = max(len(VerticesA), len(VerticesB))
        fullList(VerticesA, max_obj)
        fullList(VerticesB, max_obj)
        if len(factor) < max_obj:
            fullList(factor, max_obj)

        points = []
        for i in range(max_obj):

            max_l = max(len(VerticesA[i]), len(VerticesB[i]))
            fullList(VerticesA[i], max_l)
            fullList(VerticesB[i], max_l)

            temp_points = []
            temp_append = temp_points.append
            temp_extend = temp_points.extend
            
            if self.process_mode == 'Evaluate':
                # this matches the old Evaluate Line's code
                for j in range(max_l):
                    a = VerticesA[i][j]
                    b = VerticesB[i][j]
                    temp_extend([interp_v3_v3v3(a, b, f) for f in factor[i]])

            else:
                # This is Vector Lerp
                fullList(factor[i], max_l)   # extend factor list to match vert pair.
                for j in range(max_l):
                    a = VerticesA[i][j]
                    b = VerticesB[i][j]
                    lerp_factor = factor[i][j]
                    temp_append(interp_v3_v3v3(a, b, lerp_factor))

            points.append(temp_points)

        self.outputs['EvPoint'].sv_set(points)


def register():
    bpy.utils.register_class(SvVectorLerp)


def unregister():
    bpy.utils.unregister_class(SvVectorLerp)
