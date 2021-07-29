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

import numpy as np

import bpy
from bpy.props import IntProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode

def hilbert(step, n):

    def hilbert3(n):
        if (n <= 0):
            x, y, z = 0, 0, 0
        else:
            [xo, yo, zo] = hilbert3(n-1)
            x = step * .5 * np.array([.5+zo, .5+yo, -.5+yo, -.5-xo, -.5-xo, -.5-yo, .5-yo, .5+zo])
            y = step * .5 * np.array([.5+xo, .5+zo, .5+zo, .5+yo, -.5+yo, -.5-zo, -.5-zo, -.5-xo])
            z = step * .5 * np.array([.5+yo, -.5+xo, -.5+xo, .5-zo, .5-zo, -.5+xo, -.5+xo, .5-yo])
        return [x, y, z]

    vx, vy, vz = hilbert3(n)
    vx = vx.flatten().tolist()
    vy = vy.flatten().tolist()
    vz = vz.flatten().tolist()
    verts = [list(zip(vx, vy, vz))]
    return verts


class Hilbert3dNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Hilbert3d line '''
    bl_idname = 'Hilbert3dNode'
    bl_label = 'Hilbert3d'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level_ = IntProperty(
        name='level', description='Level',
        default=2, min=1, max=5,
        options={'ANIMATABLE'}, update=updateNode)

    size_ = FloatProperty(
        name='size', description='Size',
        default=1.0, min=0.1,
        options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Level").prop_name = 'level_'
        self.inputs.new('StringsSocket', "Size").prop_name = 'size_'
        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")

    def process(self):
        level_socket, size_socket = self.inputs
        verts_socket, edges_socket = self.outputs

        if verts_socket.is_linked:
            Integer = int(level_socket.sv_get()[0][0])
            Step = size_socket.sv_get()[0][0]

            verts = hilbert(Step, Integer)
            verts_socket.sv_set(verts)

            if edges_socket.is_linked:
                listEdg = []
                r = len(verts[0])-1
                for i in range(r):
                    listEdg.append((i, i+1))

                edg = list(listEdg)
                edges_socket.sv_set([edg])


def register():
    bpy.utils.register_class(Hilbert3dNode)


def unregister():
    bpy.utils.unregister_class(Hilbert3dNode)
