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
from bpy.props import IntProperty
from mathutils.noise import seed_set, random_unit_vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, match_long_repeat,
                            SvSetSocketAnyType, SvGetSocketAnyType)


class RandomVectorNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Random Vectors with len=1'''
    bl_idname = 'RandomVectorNode'
    bl_label = 'Random Vector'
    bl_icon = 'RNDCURVE'

    count_inner = IntProperty(name='Count', description='random',
                              default=1, min=1,
                              options={'ANIMATABLE'}, update=updateNode)
    seed = IntProperty(name='Seed', description='random seed',
                       default=1,
                       options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Count").prop_name = 'count_inner'
        self.inputs.new('StringsSocket', "Seed").prop_name = 'seed'

        self.outputs.new('VerticesSocket', "Random", "Random")

    def process(self):
        # inputs
        if 'Count' in self.inputs and self.inputs['Count'].links and \
           type(self.inputs['Count'].links[0].from_socket) == bpy.types.StringsSocket:
            Coun = SvGetSocketAnyType(self, self.inputs['Count'])[0]
        else:
            Coun = [self.count_inner]

        if 'Seed' in self.inputs and self.inputs['Seed'].links and \
           type(self.inputs['Seed'].links[0].from_socket) == bpy.types.StringsSocket:
            Seed = SvGetSocketAnyType(self, self.inputs['Seed'])[0]
        else:
            Seed = [self.seed]

        # outputs
        if 'Random' in self.outputs and self.outputs['Random'].links:
            Random = []
            param = match_long_repeat([Coun, Seed])
            # set seed, protect against float input
            # seed = 0 is special value for blender which unsets the seed value
            # and starts to use system time making the random values unrepeatable.
            # So when seed = 0 we use a random value far from 0, generated used random.org
            for c, s in zip(*param):
                int_seed = int(round(s))
                if int_seed:
                    seed_set(int_seed)
                else:
                    seed_set(140230)

                Random.append([random_unit_vector().to_tuple() for i in range(int(max(1, c)))])

            SvSetSocketAnyType(self, 'Random', Random)



def register():
    bpy.utils.register_class(RandomVectorNode)


def unregister():
    bpy.utils.unregister_class(RandomVectorNode)
