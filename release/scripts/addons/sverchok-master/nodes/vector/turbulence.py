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
from bpy.props import EnumProperty, IntProperty, FloatProperty, BoolProperty
from mathutils import noise, Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, Vector_degenerate, fullList)
from sverchok.utils.sv_seed_funcs import get_offset, seed_adjusted

# noise nodes
# from http://www.blender.org/documentation/blender_python_api_current/mathutils.noise.html


noise_options = [
    ('BLENDER', 0),
    ('STDPERLIN', 1),
    ('NEWPERLIN', 2),
    ('VORONOI_F1', 3),
    ('VORONOI_F2', 4),
    ('VORONOI_F3', 5),
    ('VORONOI_F4', 6),
    ('VORONOI_F2F1', 7),
    ('VORONOI_CRACKLE', 8),
    ('CELLNOISE', 14)
]

noise_dict = {t[0]: t[1] for t in noise_options}
avail_noise = [(t[0], t[0].title(), t[0].title(), '', t[1]) for t in noise_options]

turbulence_f = {'SCALAR': noise.turbulence, 'VECTOR': noise.turbulence_vector}

class SvTurbulenceNode(bpy.types.Node, SverchCustomTreeNode):
    '''Vector Turbulence node'''
    bl_idname = 'SvTurbulenceNode'
    bl_label = 'Vector Turbulence'
    bl_icon = 'FORCE_TURBULENCE'

    def changeMode(self, context):
        outputs = self.outputs
        if self.out_mode == 'SCALAR':
            if 'Noise S' not in outputs:
                outputs[0].replace_socket('StringsSocket', 'Noise S')
                return
        if self.out_mode == 'VECTOR':
            if 'Noise V' not in outputs:
                outputs[0].replace_socket('VerticesSocket', 'Noise V')
                return

    out_modes = [
        ('SCALAR', 'Scalar', 'Scalar output', '', 1),
        ('VECTOR', 'Vector', 'Vector output', '', 2)]

    out_mode = EnumProperty(
        items=out_modes,
        default='VECTOR',
        description='Output type',
        update=changeMode)

    noise_type = EnumProperty(
        items=avail_noise,
        default='STDPERLIN',
        description="Noise type",
        update=updateNode)

    octaves = IntProperty(default=3, min=0, max=6,
                          description='Octaves',
                          name='Octaves', update=updateNode)
    hard = BoolProperty(default=True,
                          description="Hard(sharp) or soft (smooth)transitions",
                          name="Hard", update=updateNode)
    amp = FloatProperty(default=0.50,
                          description="The amplitude scaling factor",
                          name="Amplitude", update=updateNode)
    freq = FloatProperty(default=2.00,
                          description="The frequency scaling factor",
                          name="Frequency", update=updateNode)
    rseed = IntProperty(default=0,
                          description="Random seed",
                          name="Random seed", update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices')
        self.inputs.new('StringsSocket', 'Octaves').prop_name = 'octaves'
        self.inputs.new('StringsSocket', 'Hard').prop_name = 'hard'
        self.inputs.new('StringsSocket', 'Amplitude').prop_name = 'amp'
        self.inputs.new('StringsSocket', 'Frequency').prop_name = 'freq'
        self.inputs.new('StringsSocket', 'Random seed').prop_name = 'rseed'
        self.outputs.new('VerticesSocket', 'Noise V')

    def draw_buttons(self, context, layout):
        layout.prop(self, 'out_mode', expand=True)
        layout.prop(self, 'noise_type', text="Type")

    def process(self):
            inputs, outputs = self.inputs, self.outputs

            if not outputs[0].is_linked:
                return

            _noise_type = noise_dict[self.noise_type]
            tfunc = turbulence_f[self.out_mode]

            verts = inputs['Vertices'].sv_get(deepcopy=False)
            maxlen = len(verts)
            arguments = [verts]

            # gather socket data into arguments
            for socket in inputs[1:]:
                data = socket.sv_get()[0]
                fullList(data, maxlen)
                arguments.append(data)

            # iterate over vert lists and pass arguments to the turbulence function
            out = []
            for idx, (vert_list, octaves, hard, amp, freq, seed) in enumerate(zip(*arguments)):
                final_vert_list = seed_adjusted(vert_list, seed)
                out.append([tfunc(v, octaves, hard, _noise_type, amp, freq) for v in final_vert_list])

            if 'Noise V' in outputs:
                out = Vector_degenerate(out)
            outputs[0].sv_set(out)


def register():
    bpy.utils.register_class(SvTurbulenceNode)


def unregister():
    bpy.utils.unregister_class(SvTurbulenceNode)
