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

from math import sin, cos, radians

import bpy
from bpy.props import BoolProperty, IntProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (match_long_repeat, sv_zip,
                                     updateNode)


def cylinder_vertices(Subd, Vertices, Height, RadiusBot, RadiusTop, Separate):
    theta = 360/Vertices
    heightSubd = Height/(Subd+1)
    X = []
    Y = []
    Z = []
    for i in range(Subd+2):
        radius = RadiusBot - ((RadiusBot-RadiusTop)/(Subd+1))*i
        for j in range(Vertices):
            X.append(radius*cos(radians(theta*j)))
            Y.append(radius*sin(radians(theta*j)))
            Z.append(heightSubd*i)

    points = list(sv_zip(X, Y, Z))
    if Separate:
        out = []
        out_ = []
        x = 0
        for y, P in enumerate(points):
            x += 1
            out_.append(P)
            if x//Vertices:
                out.append(out_)
                out_ = []
                x = 0
        points = out
        #points = list(zip(*out))
    return points


def cylinder_edges(Subd, Vertices):
    listEdg = []
    for i in range(Subd+2):
        for j in range(Vertices-1):
            listEdg.append([j+Vertices*i, j+1+Vertices*i])
        listEdg.append([Vertices-1+Vertices*i, 0+Vertices*i])
    for i in range(Subd+1):
        for j in range(Vertices):
            listEdg.append([j+Vertices*i, j+Vertices+Vertices*i])

    return listEdg


def cylinder_faces(Subd, Vertices, Cap):
    listPlg = []
    for i in range(Subd+1):
        for j in range(Vertices-1):
            listPlg.append([j+Vertices*i, j+1+Vertices*i, j+1+Vertices*i+Vertices, j+Vertices*i+Vertices])
        listPlg.append([Vertices-1+Vertices*i, 0+Vertices*i, 0+Vertices*i+Vertices, Vertices-1+Vertices*i+Vertices])
    if Cap:
        capBot = []
        capTop = []
        for i in range(Vertices):
            capBot.append(i)
            capTop.append(Vertices*(Subd+1)+i)
        capBot.reverse()
        listPlg.append(capBot)
        listPlg.append(capTop)
    return listPlg


class CylinderNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Cylinder '''
    bl_idname = 'CylinderNode'
    bl_label = 'Cylinder'
    bl_icon = 'MESH_CYLINDER'

    radTop_ = FloatProperty(name='Radius Top',
                            default=1.0,
                            options={'ANIMATABLE'}, update=updateNode)
    radBot_ = FloatProperty(name='Radius Bottom',
                            default=1.0,
                            options={'ANIMATABLE'}, update=updateNode)
    vert_ = IntProperty(name='Vertices',
                        default=32, min=3,
                        options={'ANIMATABLE'}, update=updateNode)
    height_ = FloatProperty(name='Height',
                            default=2.0,
                            options={'ANIMATABLE'}, update=updateNode)
    subd_ = IntProperty(name='Subdivisions',
                        default=0, min=0,
                        options={'ANIMATABLE'}, update=updateNode)
    cap_ = BoolProperty(name='Caps',
                        default=True,
                        options={'ANIMATABLE'}, update=updateNode)
    Separate = BoolProperty(name='Separate', description='Separate UV coords',
                            default=False,
                            update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "RadTop").prop_name = 'radTop_'
        self.inputs.new('StringsSocket', "RadBot").prop_name = 'radBot_'
        self.inputs.new('StringsSocket', "Vertices").prop_name = 'vert_'
        self.inputs.new('StringsSocket', "Height").prop_name = 'height_'
        self.inputs.new('StringsSocket', "Subdivisions").prop_name = 'subd_'

        self.outputs.new('VerticesSocket', "Vertices", "Vertices")
        self.outputs.new('StringsSocket', "Edges", "Edges")
        self.outputs.new('StringsSocket', "Polygons", "Polygons")

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, "Separate", text="Separate")
        row.prop(self, "cap_", text="Caps")

    def process(self):
        # inputs

        inputs = self.inputs

        RadiusTop = inputs['RadTop'].sv_get()[0]
        RadiusBot = inputs['RadBot'].sv_get()[0]
        Vertices = [max(int(v), 3) for v in inputs['Vertices'].sv_get()[0]]
        Height = inputs['Height'].sv_get()[0]
        Sub = [max(int(s), 0) for s in inputs['Subdivisions'].sv_get()[0]]
        params = match_long_repeat([Sub, Vertices, Height, RadiusBot, RadiusTop])
        # outputs
        if self.outputs['Vertices'].is_linked:

            points = [cylinder_vertices(s, v, h, rb, rt, self.Separate)
                      for s, v, h, rb, rt in zip(*params)]
            self.outputs['Vertices'].sv_set(points)

        if self.outputs['Edges'].is_linked:
            edges = [cylinder_edges(s, v)
                     for s, v, h, rb, rt in zip(*params)]
            self.outputs['Edges'].sv_set(edges)

        if self.outputs['Polygons'].is_linked:
            faces = [cylinder_faces(s, v, self.cap_)
                     for s, v, h, rb, rt in zip(*params)]
            self.outputs['Polygons'].sv_set(faces)


def register():
    bpy.utils.register_class(CylinderNode)


def unregister():
    bpy.utils.unregister_class(CylinderNode)

if __name__ == '__main__':
    register()
