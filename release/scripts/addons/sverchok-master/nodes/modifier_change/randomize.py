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
from bpy.props import FloatProperty, IntProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat

def randomize(vertices, random_x, random_y, random_z, seed):
    random.seed(seed)
    result = []
    for x,y,z in vertices:
        rx = random.uniform(-random_x, random_x)
        ry = random.uniform(-random_y, random_y)
        rz = random.uniform(-random_z, random_z)
        r = (x+rx, y+ry, z+rz)
        result.append(r)
    return result


class SvRandomizeVerticesNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Randomize input vertices locations '''
    bl_idname = 'SvRandomizeVerticesNode'
    bl_label = 'Randomize input vertices'
    bl_icon = 'OUTLINER_OB_EMPTY'

    random_x_ = FloatProperty(name='X amplitude', description='Amplitude of randomization along X axis',
                           default=0.0, min=0.0,
                           update=updateNode)
    random_y_ = FloatProperty(name='Y amplitude', description='Amplitude of randomization along Y axis',
                           default=0.0, min=0.0,
                           update=updateNode)
    random_z_ = FloatProperty(name='Z amplitude', description='Amplitude of randomization along Z axis',
                           default=0.0, min=0.0,
                           update=updateNode)
    random_seed_ = IntProperty(name='Seed', description='Random seed',
                           default=0,
                           update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "RandomX").prop_name = "random_x_"
        self.inputs.new('StringsSocket', "RandomY").prop_name = "random_y_"
        self.inputs.new('StringsSocket', "RandomZ").prop_name = "random_z_"
        self.inputs.new('StringsSocket', "Seed").prop_name = "random_seed_"

        self.outputs.new('VerticesSocket', "Vertices")

    def process(self):
        # inputs
        if not self.inputs['Vertices'].is_linked:
            return

        vertices = self.inputs['Vertices'].sv_get()
        random_x = self.inputs['RandomX'].sv_get()[0]
        random_y = self.inputs['RandomY'].sv_get()[0]
        random_z = self.inputs['RandomZ'].sv_get()[0]
        seed = self.inputs['Seed'].sv_get()[0]

        if self.outputs['Vertices'].is_linked:

            parameters = match_long_repeat([vertices, random_x, random_y, random_z, seed])

            result = [randomize(vs, rx, ry, rz, se) for vs, rx, ry, rz, se in zip(*parameters)]

            self.outputs['Vertices'].sv_set(result)

def register():
    bpy.utils.register_class(SvRandomizeVerticesNode)


def unregister():
    bpy.utils.unregister_class(SvRandomizeVerticesNode)

if __name__ == '__main__':
    register()
