from math import sin, cos, pi, degrees, radians

import bpy
from bpy.props import BoolProperty, IntProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (fullList, match_long_repeat, updateNode,
                            SvSetSocketAnyType, SvGetSocketAnyType)

class CircleNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Circle '''
    bl_idname = 'CircleNode'
    bl_label = 'Circle'
    bl_icon = 'OUTLINER_OB_EMPTY'

    rad_ = FloatProperty(name='rad_', description='Radius',
                         default=1.0, options={'ANIMATABLE'},
                         update=updateNode)
    vert_ = IntProperty(name='vert_', description='Vertices',
                        default=24, min=3, options={'ANIMATABLE'},
                        update=updateNode)
    degr_ = FloatProperty(name='degr_', description='Degrees',
                          default=360, min=0, max=360,
                          options={'ANIMATABLE'}, update=updateNode)
    mode_ = BoolProperty(name='mode_', description='Mode',
                         default=0, options={'ANIMATABLE'},
                         update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Radius", "Radius")
        self.inputs.new('StringsSocket', "Nº Vertices", "Nº Vertices")
        self.inputs.new('StringsSocket', "Degrees", "Degrees")
        self.outputs.new('VerticesSocket', "Vertices", "Vertices")
        self.outputs.new('StringsSocket', "Edges", "Edges")
        self.outputs.new('StringsSocket', "Polygons", "Polygons")

    def draw_buttons(self, context, layout):
        layout.prop(self, "rad_", text="Radius")
        layout.prop(self, "vert_", text="Nº Vert")
        layout.prop(self, "degr_", text="Degrees")
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

        X = listVertX
        Y = listVertY
        Z = [0.0]

        max_num = max(len(X), len(Y), len(Z))

        fullList(X, max_num)
        fullList(Y, max_num)
        fullList(Z, max_num)

        points = list(zip(X, Y, Z))
        return points

    def make_edges(self, Vertices, Angle):
        listEdg = [(i, i+1) for i in range(Vertices-1)]

        if Angle < 360 and self.mode_ == 1:
            listEdg.append((0, Vertices))
            listEdg.append((Vertices-1, Vertices))
        else:
            listEdg.append((0, Vertices-1))
        return listEdg

    def make_faces(self, Angle, Vertices):
        listPlg = list(range(Vertices))

        if Angle < 360 and self.mode_ == 1:
            listPlg.insert(0, Vertices)
        return [listPlg]

    def process(self):
        # inputs
        if self.inputs['Radius'].links:
            Radius = SvGetSocketAnyType(self, self.inputs['Radius'])[0]
        else:
            Radius = [self.rad_]

        if self.inputs['Nº Vertices'].links:
            Vertices = SvGetSocketAnyType(self, self.inputs['Nº Vertices'])[0]
            Vertices = list(map(lambda x: max(3, int(x)), Vertices))
        else:
            Vertices = [self.vert_]

        if self.inputs['Degrees'].links:
            Angle = SvGetSocketAnyType(self, self.inputs['Degrees'])[0]
            Angle = list(map(lambda x: min(360, max(0, x)), Angle))
        else:
            Angle = [self.degr_]

        parameters = match_long_repeat([Angle, Vertices, Radius])

        if self.outputs['Vertices'].links:
            points = [self.make_verts(a, v, r) for a, v, r in zip(*parameters)]
            SvSetSocketAnyType(self, 'Vertices', points)

        if self.outputs['Edges'].links:
            edg = [self.make_edges(v, a) for a, v, r in zip(*parameters)]
            SvSetSocketAnyType(self, 'Edges', edg)

        if self.outputs['Polygons'].links:
            plg = [self.make_faces(a, v) for a, v, r in zip(*parameters)]
            SvSetSocketAnyType(self, 'Polygons', plg)

def register():
    bpy.utils.register_class(CircleNode)

def unregister():
    bpy.utils.unregister_class(CircleNode)
