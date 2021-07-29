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

import random

import bpy
from bpy.props import IntProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat


class RandomNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Random numbers 0-1'''
    bl_idname = 'RandomNode'
    bl_label = 'Random'
    bl_icon = 'RNDCURVE'

    count_inner = IntProperty(name='Count',
                              default=1, min=1,
                               update=updateNode)
    seed = FloatProperty(name='Seed',
                         default=0,
                         update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Count").prop_name = 'count_inner'
        self.inputs.new('StringsSocket', "Seed").prop_name = 'seed'

        self.outputs.new('StringsSocket', "Random", "Random")

    def process(self):
        if not self.outputs[0].is_linked:
            return

        Coun = self.inputs['Count'].sv_get()[0]

        Seed = self.inputs['Seed'].sv_get()[0]

        # outputs


        Random = []
        if len(Seed) == 1:
            random.seed(Seed[0])
            for c in Coun:
                Random.append([random.random() for i in range(int(c))])
        else:
            param = match_long_repeat([Seed, Coun])
            for s, c in zip(*param):
                random.seed(s)
                Random.append([random.random() for i in range(int(c))])

        self.outputs[0].sv_set(Random)


def register():
    bpy.utils.register_class(RandomNode)


def unregister():
    bpy.utils.unregister_class(RandomNode)
