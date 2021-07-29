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

from mathutils import Vector, Matrix
import numpy as np

import bpy
from bpy.props import IntProperty, EnumProperty, BoolProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, Vector_generate, Vector_degenerate, ensure_nesting_level
from sverchok.utils.geom import autorotate_householder, autorotate_track, autorotate_diff
from sverchok.utils.geom import LinearSpline, CubicSpline

class SvBendAlongPathNode(bpy.types.Node, SverchCustomTreeNode):
    '''
    Triggers: Bend Path
    Tooltip: Bend mesh along path (1-D spline)
    '''
    bl_idname = 'SvBendAlongPathNode'
    bl_label = 'Bend object along path'
    bl_icon = 'CURVE_NCURVE'

    algorithms = [
            ("householder", "Householder", "Use Householder reflection matrix", 1),
            ("track", "Tracking", "Use quaternion-based tracking", 2),
            ("diff", "Rotation difference", "Use rotational difference calculation", 3)
        ]

    algorithm = EnumProperty(name = "Algorithm",
        description = "Rotation calculation algorithm",
        default = "householder",
        items = algorithms, update=updateNode)

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

    def get_axis_idx(self, letter):
        return 'XYZ'.index(letter)

    def get_orient_axis_idx(self):
        return self.get_axis_idx(self.orient_axis_)

    orient_axis = property(get_orient_axis_idx)

    up_axis = EnumProperty(name = "Up axis",
        description = "Which axis of object should look up",
        default = 'X',
        items = axes, update=updateNode)

    scale_all = BoolProperty(name="Scale all axes",
        description="Scale objects along all axes or only along orientation axis",
        default=True,
        update=updateNode)

    flip = BoolProperty(name = "Flip spline",
        description = "Invert spline direction - not from lesser coordinate values to greater, but vice versa",
        default = False,
        update=updateNode)
    
    is_cyclic = BoolProperty(name = "Cyclic",
        description = "Whether the spline is cyclic",
        default = False,
        update=updateNode)

    tangent_precision = FloatProperty(name='Tangent precision',
        description = "Step for tangents calculation. Lesser values correspond to better precision.",
        default = 0.001, min=0.000001, max=0.1, precision=8,
        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('VerticesSocket', "Path")
        self.outputs.new('VerticesSocket', 'Vertices')

    def draw_buttons(self, context, layout):
        layout.label("Orientation:")
        layout.prop(self, "orient_axis_", expand=True)
        layout.prop(self, "mode")

        col = layout.column(align=True)
        col.prop(self, "scale_all", toggle=True)
        col.prop(self, 'is_cyclic', toggle=True)
        layout.prop(self, "algorithm")
        if self.algorithm == 'track':
            layout.prop(self, "up_axis")

    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        layout.prop(self, 'flip')
        layout.prop(self, 'metric')
        layout.prop(self, 'tangent_precision')

    def build_spline(self, path):
        if self.mode == 'LIN':
            spline = LinearSpline(path, metric = self.metric, is_cyclic = self.is_cyclic)
        else:  # SPL
            spline = CubicSpline(path, metric = self.metric, is_cyclic = self.is_cyclic)
        return spline

    def get_matrix(self, tangent, scale):
        x = Vector((1.0, 0.0, 0.0))
        y = Vector((0.0, 1.0, 0.0))
        z = Vector((0.0, 0.0, 1.0))

        if self.orient_axis == 0:
            ax1, ax2, ax3 = x, y, z
        elif self.orient_axis == 1:
            ax1, ax2, ax3 = y, x, z
        else:
            ax1, ax2, ax3 = z, x, y

        if self.scale_all:
            scale_matrix = Matrix.Scale(1/scale, 4, ax1) * Matrix.Scale(scale, 4, ax2) * Matrix.Scale(scale, 4, ax3)
        else:
            scale_matrix = Matrix.Identity(4)

        if self.algorithm == 'householder':
            rot = autorotate_householder(ax1, tangent).inverted()
        elif self.algorithm == 'track':
            rot = autorotate_track(self.orient_axis_, tangent, self.up_axis)
        elif self.algorithm == 'diff':
            rot = autorotate_diff(tangent, ax1)
        else:
            raise Exception("Unsupported algorithm")

        return rot * scale_matrix

    def process(self):
        if not any(socket.is_linked for socket in self.outputs):
            return
        if not self.inputs['Vertices'].is_linked:
            return

        vertices_s = self.inputs['Vertices'].sv_get()
        paths = self.inputs['Path'].sv_get()
        paths = ensure_nesting_level(paths, 3)

        objects = match_long_repeat([vertices_s, paths])

        result_vertices = []
        result_spline = []

        for vertices, path in zip(*objects):
            # Scale orientation coordinate of input vertices to [0, 1] range
            # these values are used to calculate points on the spline
            t_values = np.array([vertex[self.orient_axis] for vertex in vertices])
            m = t_values.min()
            M = t_values.max()
            object_size = M - m
            if object_size > 0.00001:
                t_values = (t_values - m) / object_size
            else:
                raise Exception("Size of provided object along axis {} is too small".format(self.orient_axis_))

            if self.flip:
                t_values = 1.0 - t_values

            spline = self.build_spline(path)
            scale = spline.length(t_values) / object_size
            # These are points lying on the spline
            # (a projection of object to spline)
            spline_vertices = [Vector(v) for v in spline.eval(t_values).tolist()]
            spline_tangents = [Vector(v) for v in spline.tangent(t_values, h=self.tangent_precision).tolist()]

            new_vertices = []
            for src_vertex, spline_vertex, spline_tangent in zip(vertices, spline_vertices, spline_tangents):
                # Scaling and rotation matrix
                matrix = self.get_matrix(spline_tangent, scale)
                # Source vertex projected to plane orthogonal to orientation axis
                src_vertex_projection = Vector(src_vertex)
                src_vertex_projection[self.orient_axis] = 0
                # Scale and rotate the projection, then move it towards spline vertex
                new_vertex = matrix * Vector(src_vertex_projection) + spline_vertex
                new_vertices.append(new_vertex)

            result_vertices.append(new_vertices)

        self.outputs['Vertices'].sv_set(Vector_degenerate(result_vertices))

def register():
    bpy.utils.register_class(SvBendAlongPathNode)

def unregister():
    bpy.utils.unregister_class(SvBendAlongPathNode)

