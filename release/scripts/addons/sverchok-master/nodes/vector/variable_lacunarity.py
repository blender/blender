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
from bpy.props import EnumProperty, IntProperty, FloatProperty
from mathutils import noise

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
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


def var_func(position, distortion, _noise_type1, _noise_type2):
    return noise.variable_lacunarity(position, distortion, _noise_type1, _noise_type2)


noise_dict = {t[0]: t[1] for t in noise_options}
avail_noise = [(t[0], t[0].title(), t[0].title(), '', t[1]) for t in noise_options]


class SvLacunarityNode(bpy.types.Node, SverchCustomTreeNode):
    '''Variable lacunarity node'''
    bl_idname = 'SvLacunarityNode'
    bl_label = 'Variable Lacunarity'
    bl_icon = 'FORCE_TURBULENCE'

    noise_type1 = EnumProperty(
        items=avail_noise,
        default='STDPERLIN',
        description="Noise type",
        update=updateNode)

    noise_type2 = EnumProperty(
        items=avail_noise,
        default='STDPERLIN',
        description="Noise type",
        update=updateNode)

    distortion = FloatProperty(
        default=0.2, name="Distortion", update=updateNode)

    seed = IntProperty(default=0, name='Seed', update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices')
        self.inputs.new('StringsSocket', 'Seed').prop_name = 'seed'
        self.inputs.new('StringsSocket', 'Distrortion').prop_name = 'distortion'
        self.outputs.new('StringsSocket', 'Value')

    def draw_buttons(self, context, layout):
        layout.prop(self, 'noise_type1', text="Type")
        layout.prop(self, 'noise_type2', text="Type")

    def process(self):
        inputs, outputs = self.inputs, self.outputs

        if not outputs[0].is_linked:
            return

        out = []
        verts = inputs['Vertices'].sv_get(deepcopy=False)
        _seed = inputs['Seed'].sv_get()[0][0]
        _distortion = inputs['Distrortion'].sv_get()[0][0]
        _noise_type1 = noise_dict[self.noise_type1]
        _noise_type2 = noise_dict[self.noise_type2]

        for vert_list in verts:

            final_vert_list = seed_adjusted(vert_list, _seed)
            out.append([var_func(v, _distortion, _noise_type1, _noise_type2) for v in final_vert_list])

        outputs[0].sv_set(out)

    def draw_label(self):
        if self.hide:
            if not self.inputs['Seed'].is_linked:
                seed = ' + ({0})'.format(str(int(self.seed)))
            else:
                seed = ' + seed(s)'
            return self.noise_type1.title() + ' + ' + self.noise_type2.title() + seed
        else:
            return self.label or self.name


def register():
    bpy.utils.register_class(SvLacunarityNode)


def unregister():
    bpy.utils.unregister_class(SvLacunarityNode)
