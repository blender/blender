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
from bpy.props import StringProperty, IntProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


class HilbertImageNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Hilbert Image '''
    bl_idname = 'HilbertImageNode'
    bl_label = 'Hilbert image'
    bl_icon = 'OUTLINER_OB_EMPTY'

    name_image = StringProperty(
        name='image_name', description='image name', update=updateNode)

    level_ = IntProperty(
        name='level', description='Level', default=2, min=1, max=20,
        options={'ANIMATABLE'}, update=updateNode)

    size_ = FloatProperty(
        name='size', description='Size', default=1.0, min=0.1,
        options={'ANIMATABLE'}, update=updateNode)

    sensitivity_ = FloatProperty(
        name='sensitivity', description='sensitivity', default=1, min=0.1, max=1.0,
        options={'ANIMATABLE'}, update=updateNode)

    R = FloatProperty(
        name='R', description='R',
        default=0.30, min=0, max=1,
        options={'ANIMATABLE'}, update=updateNode)

    G = FloatProperty(
        name='G', description='G',
        default=0.59, min=0, max=1,
        options={'ANIMATABLE'}, update=updateNode)

    B = FloatProperty(
        name='B', description='B',
        default=0.11, min=0, max=1,
        options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Level").prop_name = 'level_'
        self.inputs.new('StringsSocket', "Size").prop_name = 'size_'
        self.inputs.new('StringsSocket', "Sensitivity").prop_name = 'sensitivity_'
        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")

    def draw_buttons(self, context, layout):
        layout.prop_search(self, "name_image", bpy.data, 'images', text="image")
        row = layout.row(align=True)
        row.scale_x = 10.0
        row.prop(self, "R", text="R")
        row.prop(self, "G", text="G")
        row.prop(self, "B", text="B")

    def process(self):
        level_socket, size_socket, sensitivity_socket = self.inputs
        verts_socket, edges_socket = self.outputs

        # inputs
        if verts_socket.is_linked and self.name_image:
            Integer = int(level_socket.sv_get()[0][0])
            Step = size_socket.sv_get()[0][0]
            Sensitivity = sensitivity_socket.sv_get()[0][0]

            # outputs
            img = bpy.data.images.get(self.name_image)
            if not img:
                print('image not in images, for some reason!..')
                return

            pixels = img.pixels[:]
            verts = self.hilbert(0.0, 0.0, 1.0, 0.0, 0.0, 1.0, Integer, img, pixels, Sensitivity)
            for iv, v in enumerate(verts):
                for ip, _ in enumerate(v):
                    verts[iv][ip] *= Step

            verts_socket.sv_set(verts)

            if edges_socket.is_linked:
                listEdg = []
                r = len(verts)-1
                for i in range(r):
                    listEdg.append((i, i+1))

                edges_socket.sv_set([list(listEdg)])


    def hilbert(self, x0, y0, xi, xj, yi, yj, n, img, pixels, Sensitivity):
        w = img.size[0]-1
        h = img.size[1]-1
        px = x0+(xi+yi)/2
        py = y0+(xj+yj)/2
        xy = int(int(px*w)+int(py*h)*(w+1))*4
        p = (pixels[xy]*self.R+pixels[xy+1]*self.G+pixels[xy+2]*self.B)#*pixels[xy+3]
        if p > 0:
            n = n-p**(1/Sensitivity)
        out = []
        if n <= 0:
            X = x0 + (xi + yi)/2
            Y = y0 + (xj + yj)/2
            out.append(X)
            out.append(Y)
            out.append(0)
            return [out]
        else:
            out.extend(self.hilbert(x0,               y0,               yi/2, yj/2, xi/2, xj/2, n - 1, img, pixels, Sensitivity))
            out.extend(self.hilbert(x0 + xi/2,        y0 + xj/2,        xi/2, xj/2, yi/2, yj/2, n - 1, img, pixels, Sensitivity))
            out.extend(self.hilbert(x0 + xi/2 + yi/2, y0 + xj/2 + yj/2, xi/2, xj/2, yi/2, yj/2, n - 1, img, pixels, Sensitivity))
            out.extend(self.hilbert(x0 + xi/2 + yi,   y0 + xj/2 + yj,  -yi/2,-yj/2,-xi/2,-xj/2, n - 1, img, pixels, Sensitivity))
            return out


def register():
    bpy.utils.register_class(HilbertImageNode)


def unregister():
    bpy.utils.unregister_class(HilbertImageNode)
