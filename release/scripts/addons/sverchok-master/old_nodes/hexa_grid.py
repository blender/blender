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
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty

from math import sqrt, sin, cos, radians

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.ui.sv_icons import custom_icon
from sverchok.utils.geom import circle
from sverchok.utils.sv_mesh_utils import mesh_join

gridLayoutItems = [
    ("RECTANGLE", "Rectangle", "", custom_icon("SV_HEXA_GRID_RECTANGLE"), 0),
    ("TRIANGLE", "Triangle", "", custom_icon("SV_HEXA_GRID_TRIANGLE"), 1),
    ("DIAMOND", "Diamond", "", custom_icon("SV_HEXA_GRID_DIAMOND"), 2),
    ("HEXAGON", "Hexagon", "", custom_icon("SV_HEXA_GRID_HEXAGON"), 3)]


def generate_grid(center, layout, settings):
    r = settings[0]   # radius
    a = settings[1]   # angle

    dx = r * 3 / 2    # distance between two consecutive points along X
    dy = r * sqrt(3)  # distance between two consecutive points along Y

    '''
    X : number of points along X
    Y : number of points along Y for each X location
    O : offset of the points in each column
    C : center of the grid
    '''
    if layout == "TRIANGLE":  # pattern Y(3) : 1 2 3 4 5
        _, _, level = settings
        X = level
        Y = range(1, X + 1)
        O = range(X)
        C = [(level - 1) * 2 / 3, 0.0]
    elif layout == "HEXAGON": # pattern Y(3) : 3 4 5 4 3
        _, _, level = settings
        X = 2 * level - 1
        Y = [X - abs(level - 1 - l) for l in range(X)]
        O = [level - 1 - abs(level - 1 - l) for l in range(X)]
        C = [level - 1, (level - 1) / 2]
    elif layout == "DIAMOND": # pattern  Y(3) : 1 2 3 2 1
        _, _, level = settings
        X = 2 * level - 1
        Y = [level - abs(level - 1 - l) for l in range(X)]
        O = [level - 1 - abs(level - 1 - l) for l in range(X)]
        C = [level - 1, 0.0]
    elif layout == "RECTANGLE":
        _, _, numx, numy = settings
        X = numx
        Y = [numy] * numx
        O = [l % 2 for l in range(X)]
        C = [(numx - 1) / 2, (numy - 1.0 + 0.5 * (numx > 1)) / 2]

    cx = C[0] * dx if center else 0
    cy = C[1] * dy if center else 0

    grid = [(x * dx - cx, y * dy - O[x] * dy / 2 - cy, 0.0) for x in range(X) for y in range(Y[x])]

    angle = radians(a)
    cosa = cos(angle)
    sina = sin(angle)
    rGrid = [(x*cosa-y*sina, x*sina+y*cosa, 0.0) for x,y,_ in grid]

    return rGrid


def generate_tiles(radius, angle, join, gridList):
    verts, edges, polys = circle(radius, radians(30-angle), 6, None, 'pydata')

    vertGridList, edgeGridList, polyGridList = [[], [], []]
    for grid in gridList:
        vertList, edgeList, polyList = [[], [], []]
        addVert, addEdge, addPoly = [vertList.append, edgeList.append, polyList.append]
        for cx, cy, _ in grid:
            verts2 = [(x + cx, y + cy, 0.0) for x, y, _ in verts]
            addVert(verts2)
            addEdge(edges)
            addPoly(polys)

        if join:
            vertList, edgeList, polyList = mesh_join(vertList, edgeList, polyList)

        vertGridList.append(vertList)
        edgeGridList.append(edgeList)
        polyGridList.append(polyList)

    return vertGridList, edgeGridList, polyGridList


class SvHexaGridNode(bpy.types.Node, SverchCustomTreeNode):

    ''' Hexa Grid '''
    bl_idname = 'SvHexaGridNode'
    bl_label = 'Hexa Grid'
    sv_icon = 'SV_HEXA_GRID'

    def update_layout(self, context):
        self.update_sockets()
        updateNode(self, context)

    gridLayout = EnumProperty(
        name="Layout",
        default="RECTANGLE", items=gridLayoutItems,
        update=update_layout)

    level = IntProperty(
        name="Level", description="Number of levels in non rectangular layouts",
        default=3, min=1, soft_min=1,
        update=updateNode)

    numx = IntProperty(
        name="NumX", description="Number of points along X",
        default=7, min=1, soft_min=1,
        update=updateNode)

    numy = IntProperty(
        name="NumY", description="Number of points along Y",
        default=6, min=1, soft_min=1,
        update=updateNode)

    radius = FloatProperty(
        name="Radius", description="Radius of the grid tile",
        default=1.0, min=0.0, soft_min=0.0,
        update=updateNode)

    angle = FloatProperty(
        name="Angle", description="Angle to rotate the grid and tiles",
        default=0.0, min=0.0, soft_min=0.0,
        update=updateNode)

    scale = FloatProperty(
        name="Scale", description="Scale of the polygon tile",
        default=1.0, min=0.0, soft_min=0.0,
        update=updateNode)

    center = BoolProperty(
        name="Center", description="Center grid around origin",
        default=True,
        update=updateNode)

    join = BoolProperty(
        name="Join", description="Join meshes into one",
        default=False,
        update=updateNode)

    def sv_init(self, context):
        self.width = 170
        self.inputs.new('StringsSocket', "Radius").prop_name = 'radius'
        self.inputs.new('StringsSocket', "Scale").prop_name = 'scale'
        self.inputs.new('StringsSocket', "Angle").prop_name = 'angle'
        self.inputs.new('StringsSocket', "Level").prop_name = 'level'
        self.inputs.new('StringsSocket', "NumX").prop_name = 'numx'
        self.inputs.new('StringsSocket', "NumY").prop_name = 'numy'

        self.outputs.new('VerticesSocket', "Centers")
        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")
        self.outputs.new('StringsSocket', "Polygons")

        self.update_layout(context)

    def update_sockets(self):
        inputs = self.inputs
        named_sockets = ['NumX', 'NumY']

        if self.gridLayout == "RECTANGLE":
            if "Level" in inputs:
                inputs.remove(inputs["Level"])
            if not "NumX" in inputs:
                inputs.new("StringsSocket", "NumX").prop_name = "numx"
            if not "NumY" in inputs:
                inputs.new("StringsSocket", "NumY").prop_name = "numy"

        elif self.gridLayout in {"TRIANGLE", "DIAMOND", "HEXAGON"}:
            if not "Level" in inputs:
                inputs.new("StringsSocket", "Level").prop_name = "level"
            for socket_name in named_sockets:
                if socket_name in inputs:
                    inputs.remove(inputs[socket_name])

    def draw_buttons(self, context, layout):
        layout.prop(self, 'gridLayout', expand=False)
        row = layout.row(align=True)
        row.prop(self, 'join')
        row.prop(self, 'center')

    def process(self):
        # return if no outputs are connected
        if not any(s.is_linked for s in self.outputs):
            return

        # input values lists
        inputs = self.inputs
        input_level = inputs["Level"].sv_get()[0] if "Level" in inputs else [0]
        input_numx = inputs["NumX"].sv_get()[0] if "NumX" in inputs else [1]
        input_numy = inputs["NumY"].sv_get()[0] if "NumY" in inputs else [1]
        input_radius = inputs["Radius"].sv_get()[0]
        input_angle = inputs["Angle"].sv_get()[0]
        input_scale = inputs["Scale"].sv_get()[0]

        # sanitize the input values
        input_level = list(map(lambda x: max(1, x), input_level))
        input_numx = list(map(lambda x: max(1, x), input_numx))
        input_numy = list(map(lambda x: max(1, x), input_numy))
        input_radius = list(map(lambda x: max(0, x), input_radius))
        input_scale = list(map(lambda x: max(0, x), input_scale))

        # generate the vectorized grids
        paramLists = []
        if self.gridLayout == 'RECTANGLE':
            paramLists.extend([input_radius, input_angle, input_numx, input_numy])
        else:  # TRIANGLE, DIAMOND HEXAGON layouts
            paramLists.extend([input_radius, input_angle, input_level])
        params = match_long_repeat(paramLists)
        gridList = [generate_grid(self.center, self.gridLayout, args) for args in zip(*params)]
        self.outputs['Centers'].sv_set(gridList)

        # generate the vectorized tiles only if any of VEP outputs are linked
        _, V, E, P = self.outputs[:]
        if not any(s.is_linked for s in [V, E, P]):
            return

        params = match_long_repeat([input_radius, input_angle, input_scale, gridList])

        vertList, edgeList, polyList = [[], [], []]
        for r, a, s, grid in zip(*params):
            verts, edges, polys = generate_tiles(r * s, a, self.join, [grid])
            vertList.extend(verts)
            edgeList.extend(edges)
            polyList.extend(polys)

        self.outputs['Vertices'].sv_set(vertList)
        self.outputs['Edges'].sv_set(edgeList)
        self.outputs['Polygons'].sv_set(polyList)


def register():
    bpy.utils.register_class(SvHexaGridNode)


def unregister():
    bpy.utils.unregister_class(SvHexaGridNode)

if __name__ == '__main__':
    register()
