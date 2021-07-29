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
#from math import copysign

import bpy
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty
import bmesh.ops

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, fullList, Matrix_generate
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

def is_matrix(lst):
    return len(lst) == 4 and len(lst[0]) == 4

def get_faces_center(faces):
    result = Vector((0,0,0))
    for face in faces:
        result += Vector(face.calc_center_median())
    result = (1.0/float(len(faces))) * result
    return result

def get_avg_normal(faces):
    result = Vector((0,0,0))
    for face in faces:
        result += Vector(face.normal)
    result = (1.0/float(len(faces))) * result
    return result

class SvExtrudeRegionNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Extrude region of faces '''
    bl_idname = 'SvExtrudeRegionNode'
    bl_label = 'Extrude Region'
    bl_icon = 'OUTLINER_OB_EMPTY'

    keep_original = BoolProperty(name="Keep original",
        description="Keep original geometry",
        default=False,
        update=updateNode)

    transform_modes = [
            ("Matrix", "By matrix", "Transform vertices by specified matrix", 0),
            ("Normal", "Along normal", "Extrude vertices along normal", 1)
        ]

    def update_mode(self, context):
        self.inputs['Matrices'].hide_safe = (self.transform_mode != "Matrix")
        self.inputs['Height'].hide_safe = (self.transform_mode != "Normal")
        self.inputs['Scale'].hide_safe = (self.transform_mode != "Normal")

        if self.transform_mode == "Normal":
            self.multiple = True
        updateNode(self, context)

    transform_mode = EnumProperty(name="Transformation mode",
            description="How vertices transformation is specified",
            default="Matrix",
            items = transform_modes,
            update=update_mode)

    height_ = FloatProperty(name="Height", description="Extrusion amount",
                default=0.0,
                update=updateNode)
    scale_ = FloatProperty(name="Scale", description="Extruded faces scale",
                default=1.0, min=0.0,
                update=updateNode)
    multiple = BoolProperty(name="Multiple extrude", description="Extrude the same region several times",
                default=False,
                update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('StringsSocket', 'Edges', 'Edges')
        self.inputs.new('StringsSocket', 'Polygons', 'Polygons')
        self.inputs.new('StringsSocket', 'Mask')
        self.inputs.new('MatrixSocket', 'Matrices')
        self.inputs.new('StringsSocket', "Height").prop_name = "height_"
        self.inputs.new('StringsSocket', "Scale").prop_name = "scale_"

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges')
        self.outputs.new('StringsSocket', 'Polygons')
        self.outputs.new('VerticesSocket', 'NewVertices')
        self.outputs.new('StringsSocket', 'NewEdges')
        self.outputs.new('StringsSocket', 'NewFaces')

        self.update_mode(context)

    def draw_buttons(self, context, layout):
        layout.prop(self, "transform_mode")
        if self.transform_mode == "Matrix":
            layout.prop(self, "multiple", toggle=True)
  
    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        layout.prop(self, "keep_original", toggle=True)
  
    def process(self):
        # inputs
        if not (self.inputs['Vertices'].is_linked and self.inputs['Polygons'].is_linked):
            return
        if not any(output.is_linked for output in self.outputs):
            return

        vertices_s = self.inputs['Vertices'].sv_get()
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])
        masks_s = self.inputs['Mask'].sv_get(default=[[1]])
        if self.transform_mode == "Matrix":
            matrices_s = self.inputs['Matrices'].sv_get(default=[[]])
            if is_matrix(matrices_s[0]):
                matrices_s = [Matrix_generate(matrices_s)]
            else:
                matrices_s = [Matrix_generate(matrices) for matrices in matrices_s]
            heights_s = [0.0]
            scales_s = [1.0]
        else:
            matrices_s = [[]]
            heights_s = self.inputs['Height'].sv_get()
            scales_s  = self.inputs['Scale'].sv_get()

        result_vertices = []
        result_edges = []
        result_faces = []
        result_ext_vertices = []
        result_ext_edges = []
        result_ext_faces = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s, masks_s, matrices_s, heights_s, scales_s])

        for vertices, edges, faces, masks, matrix_per_iteration, height_per_iteration, scale_per_iteration in zip(*meshes):
            if self.transform_mode == "Matrix":
                if not matrix_per_iteration:
                    matrix_per_iteration = [Matrix()]

            if self.multiple:
                if self.transform_mode == "Matrix":
                    n_iterations = len(matrix_per_iteration)
                else:
                    n_iterations = max(len(height_per_iteration), len(scale_per_iteration))
                    fullList(height_per_iteration, n_iterations)
                    fullList(scale_per_iteration, n_iterations)
            else:
                n_iterations = 1
                matrix_per_iteration = [matrix_per_iteration]

            fullList(masks,  len(faces))

            bm = bmesh_from_pydata(vertices, edges, faces, normal_update=True)

            b_faces = []
            b_edges = set()
            b_verts = set()
            for mask, face in zip(masks, bm.faces):
                if mask:
                    b_faces.append(face)
                    for edge in face.edges:
                        b_edges.add(edge)
                    for vert in face.verts:
                        b_verts.add(vert)

            extrude_geom = b_faces+list(b_edges)+list(b_verts)

            extruded_verts_last = []
            extruded_edges_last = []
            extruded_faces_last = []

            matrix_spaces = [Matrix()]

            for idx in range(n_iterations):

                new_geom = bmesh.ops.extrude_face_region(bm,
                                geom=extrude_geom,
                                edges_exclude=set(),
                                use_keep_orig=self.keep_original)['geom']

                extruded_verts = [v for v in new_geom if isinstance(v, bmesh.types.BMVert)]
                extruded_faces = [f for f in new_geom if isinstance(f, bmesh.types.BMFace)]

                if self.transform_mode == "Matrix":
                    matrices = matrix_per_iteration[idx]
                    if isinstance(matrices, Matrix):
                        matrices = [matrices]
                    fullList(matrix_spaces, len(extruded_verts))
                    for vertex_idx, (vertex, matrix) in enumerate(zip(*match_long_repeat([extruded_verts, matrices]))):
                        bmesh.ops.transform(bm, verts=[vertex], matrix=matrix, space=matrix_spaces[vertex_idx])
                        matrix_spaces[vertex_idx] = matrix.inverted() * matrix_spaces[vertex_idx]
                else:
                    height = height_per_iteration[idx]
                    scale = scale_per_iteration[idx]
                    
                    normal = get_avg_normal(extruded_faces) 
                    dr = normal * height
                    center = get_faces_center(extruded_faces)
                    translation = Matrix.Translation(center)
                    rotation = normal.rotation_difference((0,0,1)).to_matrix().to_4x4()
                    m = translation * rotation
                    bmesh.ops.scale(bm, vec=(scale, scale, scale), space=m.inverted(), verts=extruded_verts)
                    bmesh.ops.translate(bm, verts=extruded_verts, vec=dr)

                extruded_verts_last = [tuple(v.co) for v in extruded_verts]

                extruded_edges = [e for e in new_geom if isinstance(e, bmesh.types.BMEdge)]
                extruded_edges_last = [tuple(v.index for v in edge.verts) for edge in extruded_edges]

                extruded_faces_last = [[v.index for v in edge.verts] for edge in extruded_faces]

                extrude_geom = new_geom

            new_vertices, new_edges, new_faces = pydata_from_bmesh(bm)
            bm.free()

            result_vertices.append(new_vertices)
            result_edges.append(new_edges)
            result_faces.append(new_faces)
            result_ext_vertices.append(extruded_verts_last)
            result_ext_edges.append(extruded_edges_last)
            result_ext_faces.append(extruded_faces_last)

        self.outputs['Vertices'].sv_set(result_vertices)
        self.outputs['Edges'].sv_set(result_edges)
        self.outputs['Polygons'].sv_set(result_faces)
        self.outputs['NewVertices'].sv_set(result_ext_vertices)
        self.outputs['NewEdges'].sv_set(result_ext_edges)
        self.outputs['NewFaces'].sv_set(result_ext_faces)

def register():
    bpy.utils.register_class(SvExtrudeRegionNode)


def unregister():
    bpy.utils.unregister_class(SvExtrudeRegionNode)

if __name__ == '__main__':
    register()

