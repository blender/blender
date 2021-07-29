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

from mathutils import Vector, Matrix, kdtree

import bpy
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh
from sverchok.nodes.analyzer.normals import calc_mesh_normals

class SvMeshSelectNode(bpy.types.Node, SverchCustomTreeNode):
    '''Select vertices, edges, faces by geometric criteria'''
    bl_idname = 'SvMeshSelectNode'
    bl_label = 'Select mesh elements by location'
    bl_icon = 'UV_SYNC_SELECT'

    modes = [
            ("BySide", "By side", "Select specified side of mesh", 0),
            ("ByNormal", "By normal direction", "Select faces with normal in specified direction", 1),
            ("BySphere", "By center and radius", "Select vertices within specified distance from center", 2),
            ("ByPlane", "By plane", "Select vertices within specified distance from plane defined by point and normal vector", 3),
            ("ByCylinder", "By cylinder", "Select vertices within specified distance from straight line defined by point and direction vector", 4),
            ("EdgeDir", "By edge direction", "Select edges that are nearly parallel to specified direction", 5),
            ("Outside", "Normal pointing outside", "Select faces with normals pointing outside", 6),
            ("BBox", "By bounding box", "Select vertices within bounding box of specified points", 7)
        ]

    def update_mode(self, context):
        self.inputs['Radius'].hide_safe = (self.mode not in ['BySphere', 'ByPlane', 'ByCylinder', 'BBox'])
        self.inputs['Center'].hide_safe = (self.mode not in ['BySphere', 'ByPlane', 'ByCylinder', 'Outside', 'BBox'])
        self.inputs['Percent'].hide_safe = (self.mode not in ['BySide', 'ByNormal', 'EdgeDir', 'Outside'])
        self.inputs['Direction'].hide_safe = (self.mode not in ['BySide', 'ByNormal', 'ByPlane', 'ByCylinder', 'EdgeDir'])

        updateNode(self, context)

    mode = EnumProperty(name="Mode",
            items=modes,
            default='ByNormal',
            update=update_mode)

    include_partial = BoolProperty(name="Include partial selection",
            description="Include partially selected edges/faces",
            default=False,
            update=updateNode)

    percent = FloatProperty(name="Percent", 
            default=1.0,
            min=0.0, max=100.0,
            update=updateNode)

    radius = FloatProperty(name="Radius", default=1.0, min=0.0, update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, 'mode')
        if self.mode not in ['ByNormal', 'EdgeDir']:
            layout.prop(self, 'include_partial')

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Edges")
        self.inputs.new('StringsSocket', "Polygons")

        d = self.inputs.new('VerticesSocket', "Direction")
        d.use_prop = True
        d.prop = (0.0, 0.0, 1.0)

        c = self.inputs.new('VerticesSocket', "Center")
        c.use_prop = True
        c.prop = (0.0, 0.0, 0.0)

        self.inputs.new('StringsSocket', 'Percent', 'Percent').prop_name = 'percent'
        self.inputs.new('StringsSocket', 'Radius', 'Radius').prop_name = 'radius'

        self.outputs.new('StringsSocket', 'VerticesMask')
        self.outputs.new('StringsSocket', 'EdgesMask')
        self.outputs.new('StringsSocket', 'FacesMask')

        self.update_mode(context)

    def map_percent(self, values, percent):
        maxv = max(values)
        minv = min(values)
        if maxv <= minv:
            return maxv
        return maxv - percent * (maxv - minv) * 0.01

    def select_verts_by_faces(self, faces, verts):
        return [any(v in face for face in faces) for v in range(len(verts))]

    def select_edges_by_verts(self, verts_mask, edges):
        result = []
        for u,v in edges:
            if self.include_partial:
                ok = verts_mask[u] or verts_mask[v]
            else:
                ok = verts_mask[u] and verts_mask[v]
            result.append(ok)
        return result

    def select_faces_by_verts(self, verts_mask, faces):
        result = []
        for face in faces:
            if self.include_partial:
                ok = any(verts_mask[i] for i in face)
            else:
                ok = all(verts_mask[i] for i in face)
            result.append(ok)
        return result

    def by_normal(self, vertices, edges, faces):
        vertex_normals, face_normals = calc_mesh_normals(vertices, edges, faces)
        percent = self.inputs['Percent'].sv_get(default=[1.0])[0][0]
        direction = self.inputs['Direction'].sv_get()[0][0]
        values = [Vector(n).dot(direction) for n in face_normals]
        threshold = self.map_percent(values, percent)

        out_face_mask = [(value >= threshold) for value in values]
        out_faces = [face for (face, mask) in zip(faces, out_face_mask) if mask]
        out_verts_mask = self.select_verts_by_faces(out_faces, vertices)
        out_edges_mask = self.select_edges_by_verts(out_verts_mask, edges)

        return out_verts_mask, out_edges_mask, out_face_mask
    
    def by_side(self, vertices, edges, faces):
        percent = self.inputs['Percent'].sv_get(default=[1.0])[0][0]
        direction = self.inputs['Direction'].sv_get()[0][0]
        values = [Vector(v).dot(direction) for v in vertices]
        threshold = self.map_percent(values, percent)

        out_verts_mask = [(value >= threshold) for value in values]
        out_edges_mask = self.select_edges_by_verts(out_verts_mask, edges)
        out_faces_mask = self.select_faces_by_verts(out_verts_mask, faces)

        return out_verts_mask, out_edges_mask, out_faces_mask

    def by_sphere(self, vertices, edges, faces):
        radius = self.inputs['Radius'].sv_get(default=[1.0])[0][0]
        centers = self.inputs['Center'].sv_get()[0]

        if len(centers) == 1:
            center = centers[0]
            out_verts_mask = [((Vector(v) - Vector(center)).length <= radius) for v in vertices]
        else:
            # build KDTree
            tree = kdtree.KDTree(len(centers))
            for i, v in enumerate(centers):
                tree.insert(v, i)
            tree.balance()

            out_verts_mask = []
            for vertex in vertices:
                _, _, rho = tree.find(vertex)
                mask = rho <= radius
                out_verts_mask.append(mask)

        out_edges_mask = self.select_edges_by_verts(out_verts_mask, edges)
        out_faces_mask = self.select_faces_by_verts(out_verts_mask, faces)

        return out_verts_mask, out_edges_mask, out_faces_mask

    def by_plane(self, vertices, edges, faces):
        center = self.inputs['Center'].sv_get()[0][0]
        radius = self.inputs['Radius'].sv_get(default=[1.0])[0][0]
        direction = self.inputs['Direction'].sv_get()[0][0]

        d = - Vector(direction).dot(center)
        denominator = Vector(direction).length

        def rho(vertex):
            return abs(Vector(vertex).dot(direction) + d) / denominator

        out_verts_mask = [(rho(v) <= radius) for v in vertices]
        out_edges_mask = self.select_edges_by_verts(out_verts_mask, edges)
        out_faces_mask = self.select_faces_by_verts(out_verts_mask, faces)

        return out_verts_mask, out_edges_mask, out_faces_mask

    def by_cylinder(self, vertices, edges, faces):
        center = self.inputs['Center'].sv_get()[0][0]
        radius = self.inputs['Radius'].sv_get(default=[1.0])[0][0]
        direction = self.inputs['Direction'].sv_get()[0][0]

        denominator = Vector(direction).length

        def rho(vertex):
            numerator = (Vector(center) - Vector(vertex)).cross(direction).length
            return numerator / denominator

        out_verts_mask = [(rho(v) <= radius) for v in vertices]
        out_edges_mask = self.select_edges_by_verts(out_verts_mask, edges)
        out_faces_mask = self.select_faces_by_verts(out_verts_mask, faces)

        return out_verts_mask, out_edges_mask, out_faces_mask

    def by_edge_dir(self, vertices, edges, faces):
        percent = self.inputs['Percent'].sv_get(default=[1.0])[0][0]
        direction = self.inputs['Direction'].sv_get()[0][0]
        dirvector = Vector(direction)
        dirlength = dirvector.length
        if dirlength <= 0:
            raise ValueError("Direction vector must have nonzero length!")

        values = []
        for i, j in edges:
            u = vertices[i]
            v = vertices[j]
            edge = Vector(u) - Vector(v)
            if edge.length > 0:
                value = abs(edge.dot(dirvector)) / (edge.length * dirlength)
            else:
                value = 0
            values.append(value)
        threshold = self.map_percent(values, percent)
    
        out_edges_mask = [(value >= threshold) for value in values]
        out_edges = [edge for (edge, mask) in zip (edges, out_edges_mask) if mask]
        out_verts_mask = self.select_verts_by_faces(out_edges, vertices)
        out_faces_mask = self.select_faces_by_verts(out_verts_mask, faces)

        return out_verts_mask, out_edges_mask, out_faces_mask

    def by_outside(self, vertices, edges, faces):
        vertex_normals, face_normals = calc_mesh_normals(vertices, edges, faces)
        percent = self.inputs['Percent'].sv_get(default=[1.0])[0][0]
        center = self.inputs['Center'].sv_get()[0][0]
        center = Vector(center)

        def get_center(face):
            verts = [Vector(vertices[i]) for i in face]
            result = Vector((0,0,0))
            for v in verts:
                result += v
            return (1.0/float(len(verts))) * result

        values = []
        for face, normal in zip(faces, face_normals):
            face_center = get_center(face)
            direction = face_center - center
            dirlength = direction.length
            if dirlength > 0:
                value = math.pi - direction.angle(normal)
            else:
                value = math.pi
            values.append(value)
        threshold = self.map_percent(values, percent)

        out_face_mask = [(value >= threshold) for value in values]
        out_faces = [face for (face, mask) in zip(faces, out_face_mask) if mask]
        out_verts_mask = self.select_verts_by_faces(out_faces, vertices)
        out_edges_mask = self.select_edges_by_verts(out_verts_mask, edges)

        return out_verts_mask, out_edges_mask, out_face_mask

    def by_bbox(self, vertices, edges, faces):
        points = self.inputs['Center'].sv_get()[0]
        radius = self.inputs['Radius'].sv_get(default=[1.0])[0][0]

        # bounding box
        mins = tuple(min([point[i] for point in points]) for i in range(3))
        maxs = tuple(max([point[i] for point in points]) for i in range(3))

        # plus radius
        mins = tuple(mins[i] - radius for i in range(3))
        maxs = tuple(maxs[i] + radius for i in range(3))

        out_verts_mask = []
        for vertex in vertices:
            min_ok = all(mins[i] <= vertex[i] for i in range(3))
            max_ok = all(vertex[i] <= maxs[i] for i in range(3))
            out_verts_mask.append(min_ok and max_ok)

        out_edges_mask = self.select_edges_by_verts(out_verts_mask, edges)
        out_faces_mask = self.select_faces_by_verts(out_verts_mask, faces)

        return out_verts_mask, out_edges_mask, out_faces_mask

    def process(self):

        if not any(output.is_linked for output in self.outputs):
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])

        out_vertices = []
        out_edges = []
        out_faces = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s])
        for vertices, edges, faces in zip(*meshes):
            if self.mode == 'BySide':
                vs, es, fs = self.by_side(vertices, edges, faces)
            elif self.mode == 'ByNormal':
                vs, es, fs = self.by_normal(vertices, edges, faces)
            elif self.mode == 'BySphere':
                vs, es, fs = self.by_sphere(vertices, edges, faces)
            elif self.mode == 'ByPlane':
                vs, es, fs = self.by_plane(vertices, edges, faces)
            elif self.mode == 'ByCylinder':
                vs, es, fs = self.by_cylinder(vertices, edges, faces)
            elif self.mode == 'EdgeDir':
                vs, es, fs = self.by_edge_dir(vertices, edges, faces)
            elif self.mode == 'Outside':
                vs, es, fs = self.by_outside(vertices, edges, faces)
            elif self.mode == 'BBox':
                vs, es, fs = self.by_bbox(vertices, edges, faces)
            else:
                raise ValueError("Unknown mode: " + self.mode)

            out_vertices.append(vs)
            out_edges.append(es)
            out_faces.append(fs)

        self.outputs['VerticesMask'].sv_set(out_vertices)
        self.outputs['EdgesMask'].sv_set(out_edges)
        self.outputs['FacesMask'].sv_set(out_faces)

def register():
    bpy.utils.register_class(SvMeshSelectNode)


def unregister():
    bpy.utils.unregister_class(SvMeshSelectNode)



