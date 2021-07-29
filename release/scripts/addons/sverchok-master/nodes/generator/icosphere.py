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

from math import pi, sqrt
import bpy
import bmesh
from bpy.props import IntProperty, FloatProperty
from mathutils import Matrix, Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh
from sverchok.nodes.vector.vector_polar_in import cylindrical as from_cylindrical

def icosahedron_cylindrical(r):

    d = 2.0/sqrt(5)

    # Calculate icosahedron vertices in cylindrical coordinates 
    vertices = []
    vertices.append((0, 0, r))
    for i in range(5):
        vertices.append((d*r, pi/5 + i*2*pi/5, 0.5*d*r))
    for i in range(5):
        vertices.append((d*r, i*2*pi/5, -0.5*d*r))
    vertices.append((0, 0, -r))

    edges = []
    for i in range(1,6):
        edges.append((0,i))
    for i in range(1,5):
        edges.append((i, i+1))
    edges.append((1,5))
    for i in range(1,6):
        edges.append((i, i+5))
    for i in range(1,5):
        edges.append((i, i+6))
    edges.append((5,6))
    for i in range(6,10):
        edges.append((i, i+1))
    edges.append((6,10))
    for i in range(6,11):
        edges.append((i, 11))
        
    faces = []
    for i in range(1,5):
        faces.append([0, i, i+1])
    faces.append([0, 5, 1])
    for i in range(1,5):
        faces.append([i, i+6, i+1])
    faces.append([1, 5, 6])
    for i in range(1,5):
        faces.append([i, i+5, i+6])
    faces.append([5, 10, 6])
    for i in range(6,10):
        faces.append([i+1, i, 11])
    faces.append([6, 10, 11])

    return vertices, edges, faces

def icosahedron(r):
    vertices, edges, faces = icosahedron_cylindrical(r)
    vertices = [from_cylindrical(rho, phi, z, 'radians') for rho, phi, z in vertices]
    return vertices, edges, faces

class SvIcosphereNode(bpy.types.Node, SverchCustomTreeNode):
    "IcoSphere primitive"

    bl_idname = 'SvIcosphereNode'
    bl_label = 'IcoSphere'
    bl_icon = 'MESH_ICOSPHERE'

    replacement_nodes = [('SphereNode', None, dict(Faces='Polygons'))]

    def set_subdivisions(self, value):
        # print(value, self.subdivisions_max)
        if value > self.subdivisions_max:
            self['subdivisions'] = self.subdivisions_max
        else:
            self['subdivisions'] = value
        return None

    def get_subdivisions(self):
        return self['subdivisions']

    subdivisions = IntProperty(
        name = "Subdivisions", description = "How many times to recursively subdivide the sphere",
        default=2, min=0,
        set = set_subdivisions, get = get_subdivisions,
        update=updateNode)

    subdivisions_max = IntProperty(
        name = "Max. Subdivisions", description = "Maximum number of subdivisions available",
        default = 5, min=2,
        update=updateNode)
    
    radius = FloatProperty(
        name = "Radius",
        default=1.0, min=0.0,
        update=updateNode)

    def sv_init(self, context):
        self['subdivisions'] = 2
        
        self.inputs.new('StringsSocket', 'Subdivisions').prop_name = 'subdivisions'
        self.inputs.new('StringsSocket', 'Radius').prop_name = 'radius'

        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket',  "Edges")
        self.outputs.new('StringsSocket',  "Faces")

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, "subdivisions_max")

    def process(self):
        # return if no outputs are connected
        if not any(s.is_linked for s in self.outputs):
            return

        subdivisions_s = self.inputs['Subdivisions'].sv_get()[0]
        radius_s = self.inputs['Radius'].sv_get()[0]

        out_verts = []
        out_edges = []
        out_faces = []

        objects = match_long_repeat([subdivisions_s, radius_s])

        for subdivisions, radius in zip(*objects):
            if subdivisions == 0:
                # In this case we just return the icosahedron
                verts, edges, faces = icosahedron(radius)
                out_verts.append(verts)
                out_edges.append(edges)
                out_faces.append(faces)
                continue

            if subdivisions > self.subdivisions_max:
                subdivisions = self.subdivisions_max
            
            bm = bmesh.new()
            bmesh.ops.create_icosphere(bm,
                    subdivisions = subdivisions,
                    diameter = radius)
            verts, edges, faces = pydata_from_bmesh(bm)
            bm.free()

            out_verts.append(verts)
            out_edges.append(edges)
            out_faces.append(faces)

        self.outputs['Vertices'].sv_set(out_verts)
        self.outputs['Edges'].sv_set(out_edges)
        self.outputs['Faces'].sv_set(out_faces)

def register():
    bpy.utils.register_class(SvIcosphereNode)

def unregister():
    bpy.utils.unregister_class(SvIcosphereNode)

