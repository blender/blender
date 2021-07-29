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
from bpy.props import BoolProperty, IntProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, fullList, match_long_repeat

from mathutils import Vector


def make_plane(int_x, int_y, step_x, step_y, separate, center):
    vertices = [(0.0, 0.0, 0.0)]
    vertices_S = []
    int_x = [int(int_x) if type(int_x) is not list else int(int_x[0])]
    int_y = [int(int_y) if type(int_y) is not list else int(int_y[0])]

    # center the grid: offset the starting point of the grid by half its size
    if center:
        Nnx = int_x[0]-1   # number of steps based on the number of X vertices
        Nsx = len(step_x)  # number of steps given by the X step list

        Nny = int_y[0]-1   # number of steps based on the number of Y vertices
        Nsy = len(step_y)  # number of steps given by the Y step list

        # grid size along X (step list & repeated last step if any)
        sizeX1 = sum(step_x[:min(Nnx, Nsx)])          # step list size
        sizeX2 = max(0, (Nnx - Nsx)) * step_x[Nsx-1]  # repeated last step size
        sizeX = sizeX1 + sizeX2                       # total size

        # grid size along Y (step list & repeated last step if any)
        sizeY1 = sum(step_y[:min(Nny, Nsy)])          # step list size
        sizeY2 = max(0, (Nny - Nsy)) * step_y[Nsy-1]  # repeated last step size
        sizeY = sizeY1 + sizeY2                       # total size

        # starting point of the grid offset by half its size in both directions
        vertices = [(-0.5*sizeX, -0.5*sizeY, 0.0)]

    if type(step_x) is not list:
        step_x = [step_x]
    if type(step_y) is not list:
        step_y = [step_y]
    fullList(step_x, int_x[0])
    fullList(step_y, int_y[0])

    for i in range(int_x[0]-1):
        v = Vector(vertices[i]) + Vector((step_x[i], 0.0, 0.0))
        vertices.append(v[:])

    a = [int_y[0] if separate else int_y[0]-1]
    for i in range(a[0]):
        out = []
        for j in range(int_x[0]):
            out.append(vertices[j+int_x[0]*i])
        for j in out:
            v = Vector(j) + Vector((0.0, step_y[i], 0.0))
            vertices.append(v[:])
        if separate:
            vertices_S.append(out)

    edges = []
    edges_S = []
    for i in range(int_y[0]):
        for j in range(int_x[0]-1):
            edges.append((int_x[0]*i+j, int_x[0]*i+j+1))

    if separate:
        out = []
        for i in range(int_x[0]-1):
            out.append(edges[i])
        edges_S.append(out)
        for i in range(int_y[0]-1):
            edges_S.append(edges_S[0])
    else:
        for i in range(int_x[0]):
            for j in range(int_y[0]-1):
                edges.append((int_x[0]*j+i, int_x[0]*j+i+int_x[0]))

    polygons = []
    for i in range(int_x[0]-1):
        for j in range(int_y[0]-1):
            polygons.append((int_x[0]*j+i, int_x[0]*j+i+1, int_x[0]*j+i+int_x[0]+1, int_x[0]*j+i+int_x[0]))

    if separate:
        return vertices_S, edges_S, []
    else:
        return vertices, edges, polygons


class PlaneNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Plane '''
    bl_idname = 'PlaneNode'
    bl_label = 'Plane'
    bl_icon = 'MESH_PLANE'

    int_X = IntProperty(name='N Vert X', description='Nº Vertices X',
                        default=2, min=2,
                        options={'ANIMATABLE'}, update=updateNode)
    int_Y = IntProperty(name='N Vert Y', description='Nº Vertices Y',
                        default=2, min=2,
                        options={'ANIMATABLE'}, update=updateNode)
    step_X = FloatProperty(name='Step X', description='Step length X',
                           default=1.0, options={'ANIMATABLE'},
                           update=updateNode)
    step_Y = FloatProperty(name='Step Y', description='Step length Y',
                           default=1.0,
                           options={'ANIMATABLE'}, update=updateNode)
    Separate = BoolProperty(name='Separate', description='Separate UV coords',
                            default=False,
                            update=updateNode)
    Center = BoolProperty(name='Center', description='Center the grid',
                          default=False,
                          update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Nº Vertices X").prop_name = 'int_X'
        self.inputs.new('StringsSocket', "Nº Vertices Y").prop_name = 'int_Y'
        self.inputs.new('StringsSocket', "Step X").prop_name = 'step_X'
        self.inputs.new('StringsSocket', "Step Y").prop_name = 'step_Y'

        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")
        self.outputs.new('StringsSocket', "Polygons")

    def draw_buttons(self, context, layout):
        layout.prop(self, "Separate", text="Separate")
        layout.prop(self, "Center", text="Center")

    def process(self):
        inputs = self.inputs
        outputs = self.outputs

        if not 'Polygons' in outputs:
            return

        int_x = inputs["Nº Vertices X"].sv_get()
        int_y = inputs["Nº Vertices Y"].sv_get()
        step_x = inputs["Step X"].sv_get()
        step_y = inputs["Step Y"].sv_get()

        params = match_long_repeat([int_x, int_y, step_x, step_y, [self.Separate]])
        out = [a for a in (zip(*[make_plane(i_x, i_y, s_x, s_y, s, self.Center) for i_x, i_y, s_x, s_y, s in zip(*params)]))]

        # outputs
        if outputs['Vertices'].is_linked:
            outputs['Vertices'].sv_set(out[0])

        if outputs['Edges'].is_linked:
            outputs['Edges'].sv_set(out[1])

        if outputs['Polygons'].is_linked:
            outputs['Polygons'].sv_set(out[2])

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(PlaneNode)


def unregister():
    bpy.utils.unregister_class(PlaneNode)
