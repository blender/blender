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

from mathutils import Matrix, Vector
from math import radians, sin, cos, pi

import bpy
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty
import bmesh.ops

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, fullList, Matrix_generate

def limit(src_min, src_max, low_limit, hi_limit):
    if src_min == src_max:
        return (src_min, src_max)
    delta = src_max - src_min
    new_min = src_min + low_limit * delta * 0.01
    new_max = src_min + hi_limit * delta * 0.01
    return (new_min, new_max)

def map_linear(src_min, src_max, new_min, new_max, value):
    if src_min == src_max:
        return (new_min + new_max) / 2.0
    if value <= src_min:
        return new_min
    if value >= src_max:
        return new_max
    scale = (new_max - new_min) / (src_max - src_min)
    return new_min + scale * (value - src_min)

class SvSimpleDeformNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Simple deformation node'''
    bl_idname = 'SvSimpleDeformNode'
    bl_label = 'Simple deformation'
    bl_icon = 'MOD_SIMPLEDEFORM'

    modes = [
            ("Twist", "Twist", "", 0),
            ("Bend", "Bend", "", 1),
            ("Taper", "Taper", "", 2)
        ]

    def update_mode(self, context):
        self.inputs['Angle'].hide_safe = (self.mode == 'Taper')
        self.inputs['Factor'].hide_safe = (self.mode != 'Taper')
        updateNode(self, context)

    mode = EnumProperty(name="Mode",
            default="Twist",
            items = modes,
            update=update_mode)

    angle_modes = [
            ("radians", "Radian", "Use angles in radians", 1),
            ("degrees", "Degree", "Use angles in degrees", 2)
        ]

    angles_mode = EnumProperty(items=angle_modes, default="radians", update=updateNode)

    angle = FloatProperty(name="Angle",
                default=pi/4,
                update=updateNode)

    factor = FloatProperty(name="Factor",
                default=0.785,
                update=updateNode)

    lock_x = BoolProperty(name="Lock X",
                default=False,
                update=updateNode)
    lock_y = BoolProperty(name="Lock Y",
                default=False,
                update=updateNode)

    low_limit = FloatProperty(name="Low limit",
                default=0.0,
                min=0.0, max=100.0,
                update=updateNode)
    hi_limit = FloatProperty(name="High limit",
                default=100.0,
                min=0.0, max=100.0,
                update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode")
        if self.mode != 'Taper':
            layout.prop(self, "angles_mode", expand=True)
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, "lock_x", toggle=True)
        row.prop(self, "lock_y", toggle=True)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('MatrixSocket', 'Origin')
        self.inputs.new('StringsSocket', "Angle").prop_name = "angle"
        self.inputs.new('StringsSocket', "Factor").prop_name = "factor"
        self.inputs.new('StringsSocket', "LowLimit").prop_name = "low_limit"
        self.inputs.new('StringsSocket', "HighLimit").prop_name = "hi_limit"

        self.outputs.new('VerticesSocket', 'Vertices')

        self.update_mode(context)

    def twist(self, mins, maxs, low_limit, hi_limit, angle, vertex):
        src_min, src_max = limit(mins[2], maxs[2], low_limit, hi_limit)
        angle = map_linear(src_min, src_max, -angle/2.0, angle/2.0, vertex[2])
        if self.angles_mode == 'degrees':
            angle = radians(angle)
        matrix = Matrix.Rotation(angle, 4, 'Z')
        return matrix * vertex

    def bend(self, mins, maxs, low_limit, hi_limit, angle, vertex):
        if self.angles_mode == 'degrees':
            angle = radians(angle)
        if abs(angle) < 0.00001:
            return vertex
        x,y,z = tuple(vertex)
        L = maxs[0] - mins[0]
        angle = 100.0 * angle / (hi_limit - low_limit)
        R = L / angle

        src_min, src_max = limit(mins[0], maxs[0], low_limit, hi_limit)
        dx, dy = 0, 0
        if x < src_min:
            dx = (x - src_min) * cos(src_min * angle / L)
            dy = (x - src_min) * sin(src_min * angle / L)
            x = src_min
        if x > src_max:
            dx = (x - src_max) * cos(src_max * angle / L)
            dy = (x - src_max) * sin(src_max * angle / L)
            x = src_max
        scale = x/L

        phi = scale * angle - pi/2.0
        rho = R - y

        x1 = rho * cos(phi)
        y1 = rho * sin(phi) + R

        return Vector((x1 + dx, y1 + dy, z))

    def taper(self, mins, maxs, low_limit, hi_limit, factor, vertex):
        src_min, src_max = limit(mins[2], maxs[2], low_limit, hi_limit)
        scale = map_linear(src_min, src_max, 1-factor/2, 1+factor/2, vertex[2])
        x,y,z = tuple(vertex)
        return Vector((x*scale, y*scale, z))

    def process(self):

        if not any(output.is_linked for output in self.outputs):
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        origins = self.inputs['Origin'].sv_get(default=[Matrix()])
        angles_s = self.inputs['Angle'].sv_get()
        factors_s = self.inputs['Factor'].sv_get()
        low_limits_s = self.inputs['LowLimit'].sv_get()
        hi_limts_s = self.inputs['HighLimit'].sv_get()

        out_vertices = []

        meshes = match_long_repeat([vertices_s, origins, angles_s, factors_s, low_limits_s, hi_limts_s])
        for vertices, origin, angles, factors, low_limits, hi_limits in zip(*meshes):
            fullList(angles, len(vertices))
            fullList(factors, len(vertices))
            fullList(low_limits, len(vertices))
            fullList(hi_limits, len(vertices))

            if not isinstance(origin, Matrix):
                origin = Matrix(origin)
            src_vertices = [origin.inverted() * Vector(v) for v in vertices]
            # bounding box
            mins = tuple(min([vertex[i] for vertex in src_vertices]) for i in range(3))
            maxs = tuple(max([vertex[i] for vertex in src_vertices]) for i in range(3))

            vs = []
            for vertex, angle, factor, low_limit, hi_limit in zip(src_vertices, angles, factors, low_limits, hi_limits):
                if self.mode == 'Twist':
                    v = self.twist(mins, maxs, low_limit, hi_limit, angle, vertex)
                elif self.mode == 'Bend':
                    v = self.bend(mins, maxs, low_limit, hi_limit, angle, vertex)
                elif self.mode == 'Taper':
                    v = self.taper(mins, maxs, low_limit, hi_limit, factor, vertex)
                if self.lock_x:
                    v[0] = vertex[0]
                if self.lock_y:
                    v[1] = vertex[1]
                v = tuple(origin * v)
                vs.append(v)

            out_vertices.append(vs)

        self.outputs['Vertices'].sv_set(out_vertices)

def register():
    bpy.utils.register_class(SvSimpleDeformNode)


def unregister():
    bpy.utils.unregister_class(SvSimpleDeformNode)

