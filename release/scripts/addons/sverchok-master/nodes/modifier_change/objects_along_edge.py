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

from math import pi, degrees, floor, ceil, copysign
from mathutils import Vector, Matrix
import numpy as np

import bpy
from bpy.props import IntProperty, EnumProperty, BoolProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, Matrix_generate, Vector_generate, Vector_degenerate
from sverchok.utils.geom import autorotate_householder, autorotate_track, autorotate_diff, diameter

all_axes = [
        Vector((1.0, 0.0, 0.0)),
        Vector((0.0, 1.0, 0.0)),
        Vector((0.0, 0.0, 1.0))
    ]

def Matrix_degenerate(ms):
    return [[ j[:] for j in M ] for M in ms]

class SvDuplicateAlongEdgeNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Duplicate meshes along edge '''
    bl_idname = 'SvDuplicateAlongEdgeNode'
    bl_label = 'Duplicate objects along edge'
    bl_icon = 'OUTLINER_OB_EMPTY'
    sv_icon = 'SV_DUPLICATE'

    count_modes = [
            ("count", "Count", "Specify number of donor objects per edge", 1),
            ("up", "Up", "Calculate count of objects automatically, scaling them only up", 2),
            ("down", "Down", "Calculate count of objects automatically, scaling them only down", 3),
            ("off", "Off", "Calculate count of objects automatically, do not scale them", 4),
        ]

    def count_const(self, edge_length, vertices, count):
        return count

    def count_up(self, edge_length, vertices, count):
        donor_size = diameter(vertices, self.orient_axis)
        if donor_size == 0.0:
            return 1
        return floor( edge_length / donor_size )

    def count_down(self, edge_length, vertices, count):
        donor_size = diameter(vertices, self.orient_axis)
        if donor_size == 0.0:
            return 1
        return ceil( edge_length / donor_size )

    count_funcs = {"count": count_const,
                   "up": count_up,
                   "down": count_down,
                   "off": count_up}

    def count_mode_change(self, context):
        self.inputs["Count"].hide = self.count_mode != "count"
        self.inputs["Padding"].hide = self.count_mode == "off"
        updateNode(self, context)

    count_mode = EnumProperty(name = "Scaling mode",
        description = "Mode of determining number of objects in array and their scale",
        default = "count",
        items = count_modes, update=count_mode_change)

    algorithms = [
            ("householder", "Householder", "Use Householder reflection matrix", 1),
            ("track", "Tracking", "Use quaternion-based tracking", 2),
            ("diff", "Rotation difference", "Use rotational difference calculation", 3)
        ]

    algorithm = EnumProperty(name = "Algorithm",
        description = "Rotation calculation algorithm",
        default = "householder",
        items = algorithms, update=updateNode)

    axes = [
            ("X", "X", "X axis", 1),
            ("Y", "Y", "Y axis", 2),
            ("Z", "Z", "Z axis", 3)
        ]

    def orient_axis_change(self, context):
        updateNode(self, context)

    orient_axis_ = EnumProperty(name = "Orientation axis",
        description = "Which axis of donor objects to align with recipient edge",
        default = "X",
        items = axes, update=orient_axis_change)

    up_axis = EnumProperty(name = "Up axis",
        description = "Which axis of donor objects should look up",
        default = 'Z',
        items = axes, update=updateNode)

    def get_axis_idx(self, letter):
        return 'XYZ'.index(letter)

    def get_orient_axis_idx(self):
        return self.get_axis_idx(self.orient_axis_)

    orient_axis = property(get_orient_axis_idx)

    count_ = IntProperty(name='Count',
        description='Number of copies',
        default=3, min=1,
        update=updateNode)

    padding_ = FloatProperty(name='Padding',
        description='Fraction of destination edge length to keep empty from each side',
        default=0.0, min=0.0, max=0.49,
        update=updateNode)

    scale_all = BoolProperty(name="Scale all axes",
        description="Scale donor objects along all axes or only along orientation axis",
        default=False,
        update=updateNode)

    apply_matrices = BoolProperty(name="Apply matrices",
        description="Apply generated matrices to generated objects internally",
        default=True,
        update=updateNode)

    def get_count(self, v1, v2, vertices, count, padding):
        func = self.count_funcs[self.count_mode]
        edge_length = (1.0 - 2*padding) * (v1-v2).length
        return func(self, edge_length, vertices, count)

    def get_scale_off(self):
        return self.count_mode == "off"

    scale_off = property(get_scale_off)

    input_modes = [
            ("edge", "Edges", "Define recipient edges from set of vertices and set of edges", 1),
            ("fixed", "Fixed", "Use two specified vertices to define recipient edge", 2),
        ]

    def input_mode_change(self, context):
        self.inputs["Vertex1"].hide = self.input_mode != "fixed" 
        self.inputs["Vertex2"].hide = self.input_mode != "fixed" 
        self.inputs["VerticesR"].hide = self.input_mode != "edge"
        self.inputs["EdgesR"].hide = self.input_mode != "edge"

    input_mode = EnumProperty(name = "Input mode",
        description = "Mode of specifying recipient edges",
        default = "edge",
        items = input_modes, update=input_mode_change)

    def get_recipient_vertices(self, vs1, vs2, vertices, edges):
        if self.input_mode == "fixed":
            return vs1, vs2
        elif self.input_mode == "edge":
            rs1 = [vertices[i] for (i,j) in edges]
            rs2 = [vertices[j] for (i,j) in edges]
            return rs1, rs2

    def duplicate_vertices(self, v1, v2, vertices, edges, faces, count, p):
        direction = v2 - v1
        edge_length = (1.0 - 2*p) * direction.length
        one_item_length = edge_length / count
        actual_length = diameter(vertices, self.orient_axis)
        if actual_length != 0.0:
            x_scale = one_item_length / actual_length
        else:
            x_scale = 1.0
        x = all_axes[self.orient_axis]
        # for actual_length = 1.0 and edge_length = 3.0, let origins be [0.5, 1.5, 2.5]
        u = direction.normalized()
        alphas = np.linspace(0.0, 1.0, count+1)
        origins = [v1 + (1-2*p)*direction*alpha + p*direction + 0.5*one_item_length*u for alpha in alphas][:-1]
        assert len(origins) == count

        if self.scale_off:
            scale = None
        else:
            if self.scale_all:
                scale = Matrix.Scale(x_scale, 4)
            else:
                scale = Matrix.Scale(x_scale, 4, x)

        need_flip = False
        if self.algorithm == 'householder':
            rot = autorotate_householder(x, direction).inverted()
            # Since Householder transformation is reflection, we need to reflect things back
            need_flip = True
        elif self.algorithm == 'track':
            rot = autorotate_track(self.orient_axis_, direction, self.up_axis)
        elif self.algorithm == 'diff':
            rot = autorotate_diff(x, direction).inverted()
        else:
            raise Exception("Unsupported algorithm")

        if need_flip:
            flip = Matrix([[-1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]])
        else:
            flip = Matrix.Identity(4)
        if scale is None:
            matrices = [Matrix.Translation(o)*rot*flip for o in origins]
        else:
            matrices = [Matrix.Translation(o)*rot*scale*flip for o in origins]

        if self.apply_matrices:
            result_vertices = [[m * vertex for vertex in vertices] for m in matrices]
        else:
            result_vertices = [vertices] * count
        return matrices, result_vertices

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('StringsSocket', 'Edges', 'Edges')
        self.inputs.new('StringsSocket', 'Polygons', 'Polygons')
        self.inputs.new('VerticesSocket', "Vertex1")
        self.inputs.new('VerticesSocket', "Vertex2")
        self.inputs.new('VerticesSocket', "VerticesR")
        self.inputs.new('StringsSocket', 'EdgesR')
        self.inputs.new('StringsSocket', "Count").prop_name = "count_"
        self.inputs.new('StringsSocket', "Padding").prop_name = "padding_"

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges')
        self.outputs.new('StringsSocket', 'Polygons')
        self.outputs.new('MatrixSocket', 'Matrices')

        self.input_mode_change(context)
  
    def draw_buttons(self, context, layout):
        layout.prop(self, "count_mode")
        layout.prop(self, "orient_axis_", expand=True)
        layout.prop(self, "algorithm")
        if self.algorithm == 'track':
            layout.prop(self, "up_axis")
        layout.prop(self, "input_mode", expand=True)

    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        if not self.scale_off:
            layout.prop(self, "scale_all")
        layout.prop(self, "apply_matrices")

    def process(self):
        # VerticesR & EdgesR or Vertex1 & Vertex2 are necessary anyway
        # to define recipient edge
        if self.input_mode == "edge":
            if not (self.inputs['VerticesR'].is_linked and self.inputs['EdgesR'].is_linked):
                return
        elif self.input_mode == "fixed":
            if not (self.inputs['Vertex1'].is_linked and self.inputs['Vertex2'].is_linked):
                return
        # Input vertices are used now to define count of objects.
        # Theoretically it is possible to not use them in "Count" mode.
        if not self.inputs['Vertices'].is_linked:
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        vertices_s = Vector_generate(vertices_s)
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])
        inp_vertices1_s = self.inputs['Vertex1'].sv_get(default=[[]])
        inp_vertices1_s = Vector_generate(inp_vertices1_s)[0]
        inp_vertices2_s = self.inputs['Vertex2'].sv_get(default=[[]])
        inp_vertices2_s = Vector_generate(inp_vertices2_s)[0]
        vertices_r = self.inputs['VerticesR'].sv_get(default=[[]])
        vertices_r = Vector_generate(vertices_r)[0]
        edges_r = self.inputs['EdgesR'].sv_get(default=[[]])[0]
        counts = self.inputs['Count'].sv_get()[0]
        paddings = self.inputs['Padding'].sv_get()[0]

        vertices1_s, vertices2_s = self.get_recipient_vertices(inp_vertices1_s, inp_vertices2_s, vertices_r, edges_r)

        # It may be also useful to output just matrices, without vertices or edges/faces
        if self.outputs['Vertices'].is_linked or self.outputs['Matrices'].is_linked:

            result_matrices = []
            result_vertices = []
            result_edges = []
            result_faces = []

            meshes = match_long_repeat([vertices_s, edges_s, faces_s, vertices1_s, vertices2_s, counts, paddings])

            for vertices, edges, faces, vertex1, vertex2, inp_count, padding in zip(*meshes):
                count = self.get_count(vertex1, vertex2, vertices, inp_count, padding)
                count = max(count,1)
                new_matrices, new_vertices = self.duplicate_vertices(vertex1, vertex2, vertices, edges, faces, count, padding)
                result_edges.extend( [edges] * count )
                result_faces.extend( [faces] * count )
                result_vertices.extend( new_vertices )
                result_matrices.extend( new_matrices )

            result_vertices = Vector_degenerate(result_vertices)
            result_matrices = Matrix_degenerate(result_matrices)
            self.outputs['Vertices'].sv_set(result_vertices)
            if self.outputs['Edges'].is_linked:
                self.outputs['Edges'].sv_set(result_edges)
            if self.outputs['Polygons'].is_linked:
                self.outputs['Polygons'].sv_set(result_faces)
            if self.outputs['Matrices'].is_linked:
                self.outputs['Matrices'].sv_set(result_matrices)

def register():
    bpy.utils.register_class(SvDuplicateAlongEdgeNode)


def unregister():
    bpy.utils.unregister_class(SvDuplicateAlongEdgeNode)

if __name__ == '__main__':
    register()

