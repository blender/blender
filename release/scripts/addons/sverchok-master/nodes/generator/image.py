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
from bpy.props import IntProperty, FloatProperty, StringProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, fullList


class ImageNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Image '''
    bl_idname = 'ImageNode'
    bl_label = 'Image'
    bl_icon = 'FILE_IMAGE'


    name_image = StringProperty(name='image_name', description='image name', default='', update=updateNode)

    R = FloatProperty(
        name='R', description='R', default=0.30, min=0, max=1,
        options={'ANIMATABLE'}, update=updateNode)

    G = FloatProperty(
        name='G', description='G', default=0.59, min=0, max=1,
        options={'ANIMATABLE'}, update=updateNode)

    B = FloatProperty(
        name='B', description='B', default=0.11, min=0, max=1,
        options={'ANIMATABLE'}, update=updateNode)

    Xvecs = IntProperty(
        name='Xvecs', description='Xvecs', default=10, min=2, max=100,
        options={'ANIMATABLE'}, update=updateNode)

    Yvecs = IntProperty(
        name='Yvecs', description='Yvecs', default=10, min=2, max=100,
        options={'ANIMATABLE'}, update=updateNode)

    Xstep = FloatProperty(
        name='Xstep', description='Xstep', default=1.0, min=0.01, max=100,
        options={'ANIMATABLE'}, update=updateNode)

    Ystep = FloatProperty(
        name='Ystep', description='Ystep', default=1.0, min=0.01, max=100,
        options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "vecs X").prop_name = 'Xvecs'
        self.inputs.new('StringsSocket', "vecs Y").prop_name = 'Yvecs'
        self.inputs.new('StringsSocket', "Step X").prop_name = 'Xstep'
        self.inputs.new('StringsSocket', "Step Y").prop_name = 'Ystep'
        self.outputs.new('VerticesSocket', "vecs")
        self.outputs.new('StringsSocket', "edgs")
        self.outputs.new('StringsSocket', "pols")

    def draw_buttons(self, context, layout):
        layout.prop_search(self, "name_image", bpy.data, 'images', text="image")
        row = layout.row(align=True)
        row.scale_x = 10.0
        row.prop(self, "R", text="R")
        row.prop(self, "G", text="G")
        row.prop(self, "B", text="B")

    def process(self):
        inputs, outputs = self.inputs, self.outputs

        # inputs
        if inputs['vecs X'].is_linked:
            IntegerX = min(int(inputs['vecs X'].sv_get()[0][0]), 100)
        else:
            IntegerX = int(self.Xvecs)

        if inputs['vecs Y'].is_linked:
            IntegerY = min(int(inputs['vecs Y'].sv_get()[0][0]), 100)
        else:
            IntegerY = int(self.Yvecs)

        step_x_linked = inputs['Step X'].is_linked
        step_y_linked = inputs['Step Y'].is_linked
        StepX = inputs['Step X'].sv_get()[0] if step_x_linked else [self.Xstep]
        StepY = inputs['Step Y'].sv_get()[0] if step_y_linked else [self.Ystep]
        fullList(StepX, IntegerX)
        fullList(StepY, IntegerY)

        # outputs
        out = [[[]]]
        edg = [[[]]]
        plg = [[[]]]

        if outputs['vecs'].is_linked:
            out = [self.make_vertices(IntegerX-1, IntegerY-1, StepX, StepY, self.name_image)]
        outputs['vecs'].sv_set(out)

        if outputs['edgs'].is_linked:
            listEdg = []
            
            for i in range(IntegerY):
                for j in range(IntegerX-1):
                    listEdg.append((IntegerX*i+j, IntegerX*i+j+1))
            for i in range(IntegerX):
                for j in range(IntegerY-1):
                    listEdg.append((IntegerX*j+i, IntegerX*j+i+IntegerX))

            edg = [list(listEdg)]
        outputs['edgs'].sv_set(edg)

        if outputs['pols'].is_linked:
            listPlg = []
            for i in range(IntegerX-1):
                for j in range(IntegerY-1):
                    listPlg.append((IntegerX*j+i, IntegerX*j+i+1, IntegerX*j+i+IntegerX+1, IntegerX*j+i+IntegerX))
            plg = [list(listPlg)]
        outputs['pols'].sv_set(plg)
        

    def make_vertices(self, delitelx, delitely, stepx, stepy, image_name):
        lenx = bpy.data.images[image_name].size[0]
        leny = bpy.data.images[image_name].size[1]
        if delitelx > lenx:
            delitelx = lenx
        if delitely > leny:
            delitely = leny
        R, G, B = self.R, self.G, self.B
        xcoef = lenx//delitelx
        ycoef = leny//delitely
        # copy images data, pixels is created on every access with [i], extreme speedup.
        # http://blender.stackexchange.com/questions/3673/why-is-accessing-image-data-so-slow
        imag = bpy.data.images[image_name].pixels[:]
        vertices = []
        addition = 0
        for y in range(delitely+1):
            addition = int(ycoef*y*4*lenx)
            for x in range(delitelx+1):
                #  каждый пиксель кодируется RGBA, и записан строкой, без разделения на строки и столбцы.
                middle = (imag[addition]*R+imag[addition+1]*G+imag[addition+2]*B)*imag[addition+3]
                vertex = [x*stepx[x], y*stepy[y], middle]
                vertices.append(vertex) 
                addition += int(xcoef*4)
        return vertices


def register():
    bpy.utils.register_class(ImageNode)


def unregister():
    bpy.utils.unregister_class(ImageNode)
