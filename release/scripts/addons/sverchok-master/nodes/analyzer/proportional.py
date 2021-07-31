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

from mathutils import Vector, kdtree
import math
import bpy
from bpy.props import FloatProperty, EnumProperty, StringProperty, BoolProperty

from sverchok.data_structure import updateNode, fullList, match_long_repeat
from sverchok.node_tree import SverchCustomTreeNode

def smooth(x):
    return 3*x*x - 2*x*x*x

def sharp(x):
    return x * (2 - x)

def root(x):
    return 1.0 - math.sqrt(1.0 - x)

def linear(x):
    return x

def const(x):
    return 0.0

def sphere(x):
    return 1.0 - math.sqrt(1.0 - x*x)

def invsquare(x):
    return x*x

class SvProportionalEditNode(bpy.types.Node, SverchCustomTreeNode):
    bl_idname = 'SvProportionalEditNode'
    bl_label = 'Proportional Edit Falloff'
    bl_icon = 'PROP_ON'

    falloff_types = [
            ("smooth", "Smooth", "", 'SMOOTHCURVE', 0),
            ("sharp", "Sharp", "", 'SHARPCURVE', 1),
            ("root", "Root", "", 'ROOTCURVE', 2),
            ("linear", "Linear", "", 'LINCURVE', 3),
            ("sphere", "Sphere", "", 'SPHERECURVE', 4),
            ("invsquare", "Inverse Square", "", 'ROOTCURVE', 5),
            ("const", "Constant", "", 'NOCURVE', 6)
        ]

    falloff_type = EnumProperty(name="Falloff type",
            items=falloff_types,
            default='smooth',
            update=updateNode)

    radius = FloatProperty(name="Radius", 
            default=1.0, min=0.0001,
            update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Mask")
        self.inputs.new('StringsSocket', "Radius").prop_name = 'radius'

        self.outputs.new('StringsSocket', "Coeffs")

    def draw_buttons(self, context, layout):
        layout.prop(self, 'falloff_type')

    def falloff(self, radius, rho):
        if rho <= 0:
            return 1.0
        if rho > radius:
            return 0.0
        func = globals()[self.falloff_type]
        return 1.0 - func(rho / radius)

    def process(self):
        if not any(output.is_linked for output in self.outputs):
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        masks_s = self.inputs['Mask'].sv_get()
        radius_s = self.inputs['Radius'].sv_get()

        out_coeffs = []
        meshes = match_long_repeat([vertices_s, masks_s, radius_s])
        for vertices, masks, radius in zip(*meshes):
            fullList(masks, len(vertices))

            if isinstance(radius, list) and isinstance(radius[0], (int, float)):
                radius = radius[0]

            # build KDTree
            base = [v for v, mask in zip(vertices, masks) if mask]
            
            if len(base):

                tree = kdtree.KDTree(len(base))
                for i, v in enumerate(base):
                    tree.insert(v, i)
                tree.balance()

                coeffs = []
                for vertex, mask in zip(vertices, masks):
                    if mask:
                        coef = 1.0
                    else:
                        _, _, rho = tree.find(vertex)
                        coef = self.falloff(radius, rho)
                    coeffs.append(coef)

            else:
                coeffs = [0.0 for _ in masks]

            out_coeffs.append(coeffs)

        self.outputs['Coeffs'].sv_set(out_coeffs)


def register():
    bpy.utils.register_class(SvProportionalEditNode)


def unregister():
    bpy.utils.unregister_class(SvProportionalEditNode)

