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
from bpy.props import BoolProperty, EnumProperty
from sverchok.node_tree import SverchCustomTreeNode, StringsSocket, VerticesSocket
from sverchok.data_structure import updateNode, match_long_repeat, fullList

class SvMaskConvertNode(bpy.types.Node, SverchCustomTreeNode):
    '''vertex -> edges and so on'''
    bl_idname = 'SvMaskConvertNode'
    bl_label = 'Mask Converter'
    bl_icon = 'OUTLINER_OB_EMPTY'

    modes = [
            ('ByVertex', "Vertices", "Get edges and faces masks by vertex mask", 0),
            ('ByEdge', "Edges", "Get vertex and faces masks by edges mask", 1),
            ('ByFace', "Faces", "Get vertex and edge masks by faces mask", 2)
        ]

    def update_mode(self, context):
        self.inputs['Vertices'].hide_safe = (self.mode == 'ByVertex')

        self.inputs['VerticesMask'].hide_safe = (self.mode != 'ByVertex')
        self.inputs['EdgesMask'].hide_safe = (self.mode != 'ByEdge')
        self.inputs['FacesMask'].hide_safe = (self.mode != 'ByFace')

        self.outputs['VerticesMask'].hide_safe = (self.mode == 'ByVertex')
        self.outputs['EdgesMask'].hide_safe = (self.mode == 'ByEdge')
        self.outputs['FacesMask'].hide_safe = (self.mode == 'ByFace')

        updateNode(self, context)

    mode = EnumProperty(
            name = "Mode",
            default = 'ByVertex', items = modes,
            update=update_mode)

    include_partial = BoolProperty(name="Include partial selection",
            description="Include partially selected edges/faces",
            default=False,
            update=updateNode)

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        col.label("From:")
        row = col.row(align=True)
        row.prop(self, 'mode', expand=True)
        col.prop(self, 'include_partial', toggle=True)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Edges")
        self.inputs.new('StringsSocket', "Faces")

        self.inputs.new('StringsSocket', "VerticesMask")
        self.inputs.new('StringsSocket', "EdgesMask")
        self.inputs.new('StringsSocket', "FacesMask")

        self.outputs.new('StringsSocket', 'VerticesMask')
        self.outputs.new('StringsSocket', 'EdgesMask')
        self.outputs.new('StringsSocket', 'FacesMask')

        self.update_mode(context)

    def by_vertex(self, verts_mask, edges, faces):
        indicies = set(i for (i, m) in enumerate(verts_mask) if m)
        if self.include_partial:
            edges_mask = [any(v in indicies for v in edge) for edge in edges]
            faces_mask = [any(v in indicies for v in face) for face in faces]
        else:
            edges_mask = [all(v in indicies for v in edge) for edge in edges]
            faces_mask = [all(v in indicies for v in face) for face in faces]

        return edges_mask, faces_mask

    def by_edge(self, edge_mask, verts, edges, faces):
        indicies = set()
        for m, (u,v) in zip(edge_mask, edges):
            if m:
                indicies.add(u)
                indicies.add(v)

        verts_mask = [i in indicies for i in range(len(verts))]
        if self.include_partial:
            faces_mask = [any(v in indicies for v in face) for face in faces]
        else:
            faces_mask = [all(v in indicies for v in face) for face in faces]

        return verts_mask, faces_mask

    def by_face(self, faces_mask, verts, edges, faces):
        indicies = set()
        for m, face in zip(faces_mask, faces):
            if m:
                indicies.update(set(face))

        verts_mask = [i in indicies for i in range(len(verts))]
        if self.include_partial:
            edges_mask = [any(v in indicies for v in edge) for edge in edges]
        else:
            edges_mask = [all(v in indicies for v in edge) for edge in edges]

        return verts_mask, edges_mask

    def process(self):

        if not any(output.is_linked for output in self.outputs):
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Faces'].sv_get(default=[[]])

        verts_mask_s = self.inputs['VerticesMask'].sv_get(default=[[True]])
        edge_mask_s = self.inputs['EdgesMask'].sv_get(default=[[True]])
        face_mask_s = self.inputs['FacesMask'].sv_get(default=[[True]])

        out_verts_masks = []
        out_edges_masks = []
        out_faces_masks = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s, verts_mask_s, edge_mask_s, face_mask_s])
        for vertices, edges, faces, verts_mask, edges_mask, faces_mask in zip(*meshes):

            fullList(verts_mask, len(vertices))
            fullList(edges_mask, len(edges))
            fullList(faces_mask, len(faces))

            if self.mode == 'ByVertex':
                out_edges_mask, out_faces_mask = self.by_vertex(verts_mask, edges, faces)
                out_verts_mask = verts_mask
            elif self.mode == 'ByEdge':
                out_verts_mask, out_faces_mask = self.by_edge(edges_mask, vertices, edges, faces)
                out_edges_mask = edges_mask
            elif self.mode == 'ByFace':
                out_verts_mask, out_edges_mask = self.by_face(faces_mask, vertices, edges, faces)
                out_faces_mask = faces_mask
            else:
                raise ValueError("Unknown mode: " + self.mode)

            out_verts_masks.append(out_verts_mask)
            out_edges_masks.append(out_edges_mask)
            out_faces_masks.append(out_faces_mask)

        self.outputs['VerticesMask'].sv_set(out_verts_masks)
        self.outputs['EdgesMask'].sv_set(out_edges_masks)
        self.outputs['FacesMask'].sv_set(out_faces_masks)

def register():
    bpy.utils.register_class(SvMaskConvertNode)


def unregister():
    bpy.utils.unregister_class(SvMaskConvertNode)

