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

import math

from mathutils import Vector, Matrix

import bpy
from bpy.props import BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


# unit normal vector of plane defined by points a, b, and c
def unit_normal(a, b, c):
    mat_x = Matrix(((1, a[1], a[2]), (1, b[1], b[2]), (1, c[1], c[2])))
    mat_y = Matrix(((a[0], 1, a[2]), (b[0], 1, b[2]), (c[0], 1, c[2])))
    mat_z = Matrix(((a[0], a[1], 1), (b[0], b[1], 1), (c[0], c[1], 1)))

    x = Matrix.determinant(mat_x)
    y = Matrix.determinant(mat_y)
    z = Matrix.determinant(mat_z)

    magnitude = (x**2 + y**2 + z**2)**.5
    if magnitude == 0:
        magnitude = 1
    return (x/magnitude, y/magnitude, z/magnitude)

# area of polygon poly
def area_pol(poly):
    if len(poly) < 3:  # not a plane - no area
        return 0

    total = Vector((0, 0, 0))
    for i in range(len(poly)):
        vi1 = Vector(poly[i])
        if i is len(poly)-1:
            vi2 = Vector(poly[0])
        else:
            vi2 = Vector(poly[i+1])

        prod = vi1.cross(vi2)[:]
        total[0] += prod[0]
        total[1] += prod[1]
        total[2] += prod[2]

    result = total.dot(unit_normal(poly[0], poly[1], poly[2]))
    return abs(result/2)

def areas(Vertices, Polygons, per_face):
    areas = []
    for i, obj in enumerate(Polygons):
        res = []
        for face in obj:
            poly = []
            for j in face:
                poly.append(Vertices[i][j])
            res.append(area_pol(poly))

        if per_face:
            areas.extend(res)
        else:
            areas.append(math.fsum(res))

    return areas

class AreaNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Area '''
    bl_idname = 'AreaNode'
    bl_label = 'Area'
    bl_icon = 'OUTLINER_OB_EMPTY'
    sv_icon = 'SV_AREA'

    per_face = BoolProperty(name='per_face',
                            default=True,
                            update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('StringsSocket', "Polygons", "Polygons")
        self.outputs.new('StringsSocket', "Area", "Area")

    def draw_buttons(self, context, layout):
        layout.prop(self, "per_face", text="Count faces")

    def process(self):
        # inputs
        inputs = self.inputs
        outputs = self.outputs

        if not 'Area' in outputs:
            return

        Vertices = inputs["Vertices"].sv_get()
        Polygons = inputs["Polygons"].sv_get()

        # outputs
        if outputs['Area'].is_linked:
            outputs['Area'].sv_set([areas(Vertices, Polygons, self.per_face)])



def register():
    bpy.utils.register_class(AreaNode)


def unregister():
    bpy.utils.unregister_class(AreaNode)
