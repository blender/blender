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
from bpy.props import IntProperty, EnumProperty, BoolProperty, FloatProperty
import bmesh.ops

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh


def get_bevel_edges(bm, bevel_edges):
    if bevel_edges:
        b_edges = []
        for edge in bevel_edges:
            b_edge = [e for e in bm.edges if set([v.index for v in e.verts]) == set(edge)]
            b_edges.append(b_edge[0])
    else:
        b_edges = bm.edges

    return b_edges


class SvBevelNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Bevel vertices, edges and faces'''
    bl_idname = 'SvBevelNode'
    bl_label = 'Bevel'
    bl_icon = 'MOD_BEVEL'

    offset_ = FloatProperty(
        name='Amount',
        description='Amount to offset beveled edge',
        default=0.0, min=0.0, update=updateNode)

    offset_modes = [
        ("0", "Offset", "Amount is offset of new edges from original", 1),
        ("1", "Width", "Amount is width of new face", 2),
        ("2", "Depth", "Amount is perpendicular distance from original edge to bevel face", 3),
        ("3", "Percent", "Amount is percent of adjacent edge length", 4)
    ]

    offsetType = EnumProperty(
        name='Amount Type',
        description="What distance Amount measures",
        items=offset_modes, update=updateNode)

    segments_ = IntProperty(
        name="Segments",
        description="Number of segments in bevel",
        default=1, min=1, update=updateNode)

    profile_ = FloatProperty(
        name="Profile",
        description="Profile shape; 0.5 - round",
        default=0.5, min=0.0, max=1.0, update=updateNode)

    vertexOnly = BoolProperty(
        name="Vertex only",
        description="Only bevel edges, not edges",
        default=False, update=updateNode)


    def sv_init(self, context):
        si, so = self.inputs.new, self.outputs.new
        si('VerticesSocket', "Vertices")
        si('StringsSocket', 'Edges')
        si('StringsSocket', 'Polygons')
        si('StringsSocket', 'BevelEdges')
        si('StringsSocket', "Offset").prop_name = "offset_"
        si('StringsSocket', "Segments").prop_name = "segments_"
        si('StringsSocket', "Profile").prop_name = "profile_"
        so('VerticesSocket', 'Vertices')
        so('StringsSocket', 'Edges')
        so('StringsSocket', 'Polygons')
        so('StringsSocket', 'NewPolys')

    def draw_buttons(self, context, layout):
        layout.prop(self, "offsetType")
        layout.prop(self, "vertexOnly")


    def get_socket_data(self):
        Vertices, Edges, Polygons, BevelEdges, Offsets, Segments, Profiles = self.inputs
        return [
            Vertices.sv_get(default=[[]]),
            Edges.sv_get(default=[[]]),
            Polygons.sv_get(default=[[]]),
            BevelEdges.sv_get(default=[[]]),
            Offsets.sv_get()[0],
            Segments.sv_get()[0],
            Profiles.sv_get()[0]
        ]


    def process(self):

        if not (self.inputs[0].is_linked and self.inputs[2].is_linked):
            return
        if not any(self.outputs[name].is_linked for name in ['Vertices', 'Edges', 'Polygons', 'NewPolys']):
            return

        out, result_bevel_faces = [], []

        meshes = match_long_repeat(self.get_socket_data())
        for vertices, edges, faces, bevel_edges, offset, segments, profile in zip(*meshes):
            bm = bmesh_from_pydata(vertices, edges, faces)
            b_edges = get_bevel_edges(bm, bevel_edges)

            geom = list(bm.verts) + list(b_edges) + list(bm.faces)
            bevel_faces = bmesh.ops.bevel(
                bm, geom=geom, offset=offset,
                offset_type=int(self.offsetType), segments=segments,
                profile=profile, vertex_only=self.vertexOnly, material=-1)['faces']

            new_bevel_faces = [[v.index for v in face.verts] for face in bevel_faces]
            out.append(pydata_from_bmesh(bm))
            bm.free()
            result_bevel_faces.append(new_bevel_faces)

        Vertices, Edges, Polygons, NewPolygons = self.outputs
        Vertices.sv_set([i[0] for i in out])
        Edges.sv_set([i[1] for i in out])
        Polygons.sv_set([i[2] for i in out])
        NewPolygons.sv_set(result_bevel_faces)


def register():
    bpy.utils.register_class(SvBevelNode)


def unregister():
    bpy.utils.unregister_class(SvBevelNode)
