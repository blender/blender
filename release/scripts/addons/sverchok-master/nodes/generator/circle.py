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

from math import sin, cos, pi, degrees, radians

import bpy
from bpy.props import BoolProperty, IntProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (fullList, match_long_repeat, updateNode)


class SvCircleNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Circle '''
    bl_idname = 'SvCircleNode'
    bl_label = 'Circle'
    bl_icon = 'MESH_CIRCLE'

    rad_ = FloatProperty(name='Radius', description='Radius',
                         default=1.0,
                         update=updateNode)
    vert_ = IntProperty(name='N Vertices', description='Vertices',
                        default=24, min=3,
                        update=updateNode)
    degr_ = FloatProperty(name='Degrees', description='Degrees',
                          default=pi*2, min=0, max=pi*2, subtype='ANGLE',
                          options={'ANIMATABLE'}, update=updateNode)
    mode_ = BoolProperty(name='mode_', description='Mode',
                         default=0,  update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Radius").prop_name = 'rad_'
        self.inputs.new('StringsSocket', "Nº Vertices").prop_name = 'vert_'
        self.inputs.new('StringsSocket', "Degrees").prop_name = 'degr_'

        self.outputs.new('VerticesSocket', "Vertices", "Vertices")
        self.outputs.new('StringsSocket', "Edges", "Edges")
        self.outputs.new('StringsSocket', "Polygons", "Polygons")

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode_", text="Mode")

    def make_verts(self, Angle, Vertices, Radius):
        if Angle < 360:
            theta = Angle/(Vertices-1)
        else:
            theta = Angle/Vertices
        listVertX = []
        listVertY = []
        for i in range(Vertices):
            listVertX.append(Radius*cos(radians(theta*i)))
            listVertY.append(Radius*sin(radians(theta*i)))

        if Angle < 360 and self.mode_ == 0:
            sigma = radians(Angle)
            listVertX[-1] = Radius*cos(sigma)
            listVertY[-1] = Radius*sin(sigma)
        elif Angle < 360 and self.mode_ == 1:
            listVertX.append(0.0)
            listVertY.append(0.0)

        points = list((x,y,0) for x,y in zip(listVertX, listVertY) )
        return points

    def make_edges(self, Vertices, Angle):
        listEdg = [(i, i+1) for i in range(Vertices-1)]

        if Angle < 360 and self.mode_ == 1:
            listEdg.append((0, Vertices))
            listEdg.append((Vertices-1, Vertices))
        else:
            listEdg.append((Vertices-1, 0))
        return listEdg

    def make_faces(self, Angle, Vertices):
        listPlg = list(range(Vertices))

        if Angle < 360 and self.mode_ == 1:
            listPlg.insert(0, Vertices)
        return [listPlg]

    def process(self):
        inputs, outputs = self.inputs, self.outputs

        # inputs
        if inputs['Radius'].is_linked:
            Radius = inputs['Radius'].sv_get(deepcopy=False)[0]
        else:
            Radius = [self.rad_]

        if inputs['Nº Vertices'].is_linked:
            Vertices = inputs['Nº Vertices'].sv_get(deepcopy=False)[0]
            Vertices = list(map(lambda x: max(3, int(x)), Vertices))
        else:
            Vertices = [self.vert_]

        Angle = inputs['Degrees'].sv_get(deepcopy=False)[0]
        if inputs['Degrees'].is_linked:
            Angle = list(map(lambda x: min(360, max(0, x)), Angle))
            

        parameters = match_long_repeat([Angle, Vertices, Radius])

        # outputs
        if outputs['Vertices'].is_linked:
            points = [self.make_verts(a, v, r) for a, v, r in zip(*parameters)]
            outputs['Vertices'].sv_set(points)

        if outputs['Edges'].is_linked:
            edg = [self.make_edges(v, a) for a, v, r in zip(*parameters)]
            outputs['Edges'].sv_set(edg)

        if outputs['Polygons'].is_linked:
            plg = [self.make_faces(a, v) for a, v, r in zip(*parameters)]
            outputs['Polygons'].sv_set(plg)


def register():
    bpy.utils.register_class(SvCircleNode)


def unregister():
    bpy.utils.unregister_class(SvCircleNode)
