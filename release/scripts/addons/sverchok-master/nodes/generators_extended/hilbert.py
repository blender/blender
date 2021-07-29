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

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


class HilbertNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Hilbert line '''
    bl_idname = 'HilbertNode'
    bl_label = 'Hilbert'
    bl_icon = 'OUTLINER_OB_EMPTY'
    sv_icon = 'SV_HILBERT2D'

    level_ = IntProperty(
        name='level', description='Level', default=2, min=1, max=6,
        options={'ANIMATABLE'}, update=updateNode)

    size_ = FloatProperty(
        name='size', description='Size', default=1.0, min=0.1,
        options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Level", "Level").prop_name = 'level_'
        self.inputs.new('StringsSocket', "Size", "Size").prop_name = 'size_'
        self.outputs.new('VerticesSocket', "Vertices", "Vertices")
        self.outputs.new('StringsSocket', "Edges", "Edges")

    def draw_buttons(self, context, layout):
        pass

    def process(self):
        level_socket, size_socket = self.inputs
        verts_socket, edges_socket = self.outputs

        if verts_socket.is_linked:

            Integer = int(level_socket.sv_get()[0][0])
            Step = size_socket.sv_get()[0][0]

            verts = self.hilbert(0.0, 0.0, Step*1.0, 0.0, 0.0, Step*1.0, Integer)
            verts_socket.sv_set([verts])

            if edges_socket.is_linked:

                listEdg = []
                r = len(verts)-1
                for i in range(r):
                    listEdg.append((i, i+1))

                edg = list(listEdg)
                edges_socket.sv_set([edg])

    def hilbert(self, x0, y0, xi, xj, yi, yj, n):
        out = []
        if n <= 0:
            X = x0 + (xi + yi)/2
            Y = y0 + (xj + yj)/2
            out.append(X)
            out.append(Y)
            out.append(0)
            return [out]

        else:
            out.extend(self.hilbert(x0,               y0,               yi/2, yj/2, xi/2, xj/2, n - 1))
            out.extend(self.hilbert(x0 + xi/2,        y0 + xj/2,        xi/2, xj/2, yi/2, yj/2, n - 1))
            out.extend(self.hilbert(x0 + xi/2 + yi/2, y0 + xj/2 + yj/2, xi/2, xj/2, yi/2, yj/2, n - 1))
            out.extend(self.hilbert(x0 + xi/2 + yi,   y0 + xj/2 + yj,  -yi/2,-yj/2,-xi/2,-xj/2, n - 1))
            return out


def register():
    bpy.utils.register_class(HilbertNode)


def unregister():
    bpy.utils.unregister_class(HilbertNode)
