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

from math import sqrt
from mathutils import Vector, Matrix
import numpy as np

import bpy
from bpy.props import IntProperty, EnumProperty, BoolProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, ensure_nesting_level, transpose_list
from sverchok.utils.geom import diameter
from sverchok.utils.geom import LinearSpline, CubicSpline, Spline2D

class SvBendAlongSurfaceNode(bpy.types.Node, SverchCustomTreeNode):
    '''
    Triggers: Bend Surface
    Tooltip: Bend mesh along surface (2-D spline)
    '''
    bl_idname = 'SvBendAlongSurfaceNode'
    bl_label = 'Bend object along surface'
    bl_icon = 'SURFACE_NSURFACE'

    modes = [('SPL', 'Cubic', "Cubic Spline", 0),
             ('LIN', 'Linear', "Linear Interpolation", 1)]

    mode = EnumProperty(name='Mode',
        default="SPL", items=modes,
        update=updateNode)

    metrics =    [('MANHATTAN', 'Manhattan', "Manhattan distance metric", 0),
                  ('DISTANCE', 'Euclidan', "Eudlcian distance metric", 1),
                  ('POINTS', 'Points', "Points based", 2),
                  ('CHEBYSHEV', 'Chebyshev', "Chebyshev distance", 3)]

    metric = EnumProperty(name='Metric',
        description = "Knot mode",
        default="DISTANCE", items=metrics,
        update=updateNode)

    axes = [
            ("X", "X", "X axis", 1),
            ("Y", "Y", "Y axis", 2),
            ("Z", "Z", "Z axis", 3)
        ]

    orient_axis_ = EnumProperty(name = "Orientation axis",
        description = "Which axis of object to put along path",
        default = "Z",
        items = axes, update=updateNode)

    normal_precision = FloatProperty(name='Normal precision',
        description = "Step for normals calculation. Lesser values correspond to better precision.",
        default = 0.001, min=0.000001, max=0.1, precision=8,
        update=updateNode)

    def get_axis_idx(self, letter):
        return 'XYZ'.index(letter)

    def get_orient_axis_idx(self):
        return self.get_axis_idx(self.orient_axis_)

    orient_axis = property(get_orient_axis_idx)

    autoscale = BoolProperty(name="Auto scale",
        description="Scale object along orientation axis automatically",
        default=False,
        update=updateNode)

    grouped = BoolProperty(name = "Grouped",
        description = "If enabled, then the node expects list of lists of vertices, instead of list of vertices. Output has corresponding shape.",
        default = True,
        update=updateNode)

    is_cycle_u = BoolProperty(name="Cycle U",
        description = "Whether the spline is cyclic in U direction",
        default = False,
        update=updateNode)

    is_cycle_v = BoolProperty(name="Cycle V",
        description = "Whether the spline is cyclic in V direction",
        default = False,
        update=updateNode)

    flip = BoolProperty(name = "Flip surface",
        description = "Flip the surface orientation",
        default = False,
        update=updateNode)

    transpose = BoolProperty(name = "Swap U/V",
        description = "Swap U and V directions in surface definition",
        default = False,
        update=updateNode)
    
    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('VerticesSocket', "Surface")
        self.outputs.new('VerticesSocket', 'Vertices')

    def draw_buttons(self, context, layout):
        layout.label("Orientation:")
        layout.prop(self, "orient_axis_", expand=True)
        layout.prop(self, "mode")

        col = layout.column(align=True)
        col.prop(self, "autoscale", toggle=True)
        row = col.row(align=True)
        row.prop(self, "is_cycle_u", toggle=True)
        row.prop(self, "is_cycle_v", toggle=True)
        col.prop(self, "grouped", toggle=True)

    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        layout.prop(self, 'flip')
        layout.prop(self, 'transpose')
        layout.prop(self, 'metric')
        layout.prop(self, 'normal_precision')

    def build_spline(self, surface):
        if self.mode == 'LIN':
            constructor = LinearSpline
        else:
            constructor = CubicSpline
        spline = Spline2D(surface, u_spline_constructor=constructor,
                    is_cyclic_u = self.is_cycle_u,
                    is_cyclic_v = self.is_cycle_v,
                    metric=self.metric)
        return spline
    
    def get_other_axes(self):
        # Select U and V to be two axes except orient_axis
        if self.orient_axis_ == 'X':
            u_index, v_index = 1,2
        elif self.orient_axis_ == 'Y':
            u_index, v_index = 2,0
        else:
            u_index, v_index = 0,1
        return u_index, v_index
    
    def get_uv(self, vertices):
        """
        Translate source vertices to UV space of future spline.
        vertices must be list of list of 3-tuples.
        """
        #print("Vertices: {} of {} of {}".format(type(vertices), type(vertices[0]), type(vertices[0][0])))
        u_index, v_index = self.get_other_axes()

        # Rescale U and V coordinates to [0, 1], drop third coordinate
        us = [vertex[u_index] for col in vertices for vertex in col]
        vs = [vertex[v_index] for col in vertices for vertex in col]
        min_u = min(us)
        max_u = max(us)
        min_v = min(vs)
        max_v = max(vs)

        size_u = max_u - min_u
        size_v = max_v - min_v

        if size_u < 0.00001:
            raise Exception("Object has too small size in U direction")
        if size_v < 0.00001:
            raise Exception("Object has too small size in V direction")
        result = [[((vertex[u_index] - min_u)/size_u, (vertex[v_index] - min_v)/size_v) for vertex in col] for col in vertices]

        return size_u, size_v, result

    def process(self):
        if not any(socket.is_linked for socket in self.outputs):
            return
        if not self.inputs['Vertices'].is_linked:
            return

        vertices_s = self.inputs['Vertices'].sv_get()
        vertices_s = ensure_nesting_level(vertices_s, 4)
        surfaces = self.inputs['Surface'].sv_get()
        surfaces = ensure_nesting_level(surfaces, 4)

        objects = match_long_repeat([vertices_s, surfaces])

        result_vertices = []

        for vertices, surface in zip(*objects):
            if self.transpose:
                surface = transpose_list(surface)
            #print("Surface: {} of {} of {}".format(type(surface), type(surface[0]), type(surface[0][0])))
            spline = self.build_spline(surface)
            # uv_coords will be list[m] of lists[n] of 2-tuples of floats
            # number of "rows" and "columns" in uv_coords will match so of vertices.
            src_size_u, src_size_v, uv_coords = self.get_uv(vertices)
            if self.autoscale:
                u_index, v_index = self.get_other_axes()
                surface_flattened = [v for col in surface for v in col]
                scale_u = diameter(surface_flattened, u_index) / src_size_u
                scale_v = diameter(surface_flattened, v_index) / src_size_v
                scale_z = sqrt(scale_u * scale_v)
            else:
                scale_z = 1.0
            if self.flip:
                scale_z = - scale_z
            new_vertices = []
            for uv_row, vertices_row in zip(uv_coords,vertices):
                new_row = []
                for ((u, v), src_vertex) in zip(uv_row, vertices_row):
                    #print("UV: ({}, {}), SRC: {}".format(u, v, src_vertex))
                    spline_vertex = np.array(spline.eval(u, v))
                    spline_normal = np.array(spline.normal(u, v, h=self.normal_precision))
                    #print("Spline: M {}, N {}".format(spline_vertex, spline_normal))
                    # Coordinate of source vertex corresponding to orientation axis
                    z = src_vertex[self.orient_axis]
                    new_vertex = tuple(spline_vertex + scale_z * z * spline_normal)
                    new_row.append(new_vertex)
                new_vertices.append(new_row)
            result_vertices.append(new_vertices)

        if not self.grouped:
            result_vertices = result_vertices[0]
        self.outputs['Vertices'].sv_set(result_vertices)

def register():
    bpy.utils.register_class(SvBendAlongSurfaceNode)

def unregister():
    bpy.utils.unregister_class(SvBendAlongSurfaceNode)

