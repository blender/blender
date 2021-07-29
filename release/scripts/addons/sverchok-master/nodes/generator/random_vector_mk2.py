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
from mathutils.noise import seed_set, random_unit_vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat


class RandomVectorNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' rv Random unit Vec'''
    bl_idname = 'RandomVectorNodeMK2'
    bl_label = 'Random Vector MK2'
    bl_icon = 'RNDCURVE'

    count_inner = IntProperty(
        name='Count', description='random', default=1, min=1,
        options={'ANIMATABLE'}, update=updateNode)

    scale = FloatProperty(
        name='Scale', description='scale for vectors', default=1.0,
        options={'ANIMATABLE'}, update=updateNode)

    seed = IntProperty(
        name='Seed', description='random seed', default=1,
        options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Count").prop_name = 'count_inner'
        self.inputs.new('StringsSocket', "Seed").prop_name = 'seed'
        self.inputs.new('StringsSocket', "Scale").prop_name = 'scale'
        self.outputs.new('VerticesSocket', "Random")

    def process(self):

        count_socket = self.inputs['Count']
        seed_socket = self.inputs['Seed']
        scale_socket = self.inputs['Scale']
        random_socket = self.outputs['Random']

        # inputs
        Coun = count_socket.sv_get(deepcopy=False)[0]
        Seed = seed_socket.sv_get(deepcopy=False)[0]
        Scale = scale_socket.sv_get(deepcopy=False, default=[])[0]

        # outputs
        if random_socket.is_linked:
            Random = []
            param = match_long_repeat([Coun, Seed, Scale])
            # set seed, protect against float input
            # seed = 0 is special value for blender which unsets the seed value
            # and starts to use system time making the random values unrepeatable.
            # So when seed = 0 we use a random value far from 0, generated used random.org
            for c, s, sc in zip(*param):
                int_seed = int(round(s))
                if int_seed:
                    seed_set(int_seed)
                else:
                    seed_set(140230)

                Random.append([(random_unit_vector()*sc).to_tuple() for i in range(int(max(1, c)))])

            random_socket.sv_set(Random)



def register():
    bpy.utils.register_class(RandomVectorNodeMK2)


def unregister():
    bpy.utils.unregister_class(RandomVectorNodeMK2)
