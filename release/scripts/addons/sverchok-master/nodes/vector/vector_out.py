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

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import dataCorrect


class VectorsOutNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Vectors out '''
    bl_idname = 'VectorsOutNode'
    bl_label = 'Vector out'
    sv_icon = 'SV_COMBINE_OUT'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vectors", "Vectors")
        self.outputs.new('StringsSocket', "X", "X")
        self.outputs.new('StringsSocket', "Y", "Y")
        self.outputs.new('StringsSocket', "Z", "Z")

    def process(self):
        # inputs
        if self.inputs['Vectors'].is_linked:
            xyz = self.inputs['Vectors'].sv_get(deepcopy=False)

            data = dataCorrect(xyz)
            X, Y, Z = [], [], []
            for obj in data:
                x_, y_, z_ = (list(x) for x in zip(*obj))
                X.append(x_)
                Y.append(y_)
                Z.append(z_)
            for i, name in enumerate(['X', 'Y', 'Z']):
                if self.outputs[name].is_linked:
                    self.outputs[name].sv_set([X, Y, Z][i])


def register():
    bpy.utils.register_class(VectorsOutNode)


def unregister():
    bpy.utils.unregister_class(VectorsOutNode)
