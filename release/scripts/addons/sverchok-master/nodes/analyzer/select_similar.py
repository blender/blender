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
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty
import bmesh.ops

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, fullList
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

class SvSelectSimilarNode(bpy.types.Node, SverchCustomTreeNode):
    '''
    Like Blender's Shift+G ///
    
    Select vertices, edges, faces similar to selected ones'''

    bl_idname = 'SvSelectSimilarNode'
    bl_label = 'Select similar'
    bl_icon = 'OUTLINER_OB_EMPTY'

    modes = [
            ("verts", "Vertices", "Select similar vertices", 0),
            ("edges", "Edges", "Select similar edges", 1),
            ("faces", "Faces", "Select similar faces", 2)
        ]

    vertex_modes = [
            ("0", "Normal", "Select vertices with similar normal", 0),
            ("1", "Adjacent faces", "Select vertices with similar number of adjacent faces", 1),
            # SIMVERT_VGROUP is skipped for now, since we do not have vgroups at input
            ("3", "Adjacent edges", "Select vertices with similar number of adjacent edges", 3)
        ]

    edge_modes = [
            ("101", "Length", "Select edges with similar length", 101),
            ("102", "Direction", "Select edges with similar direction", 102),
            ("103", "Adjacent faces", "Select edges with similar number of faces around edge", 103),
            ("104", "Face Angle", "Select edges by face angle", 104)
            # SIMEDGE_CREASE, BEVEL, SEAM, SHARP, FREESTYLE are skipped for now,
            # since we do not have such data at input
        ]

    face_modes = [
            # SIMFACE_MATERIAL, IMAGE are skipped for now, since we do not have such data at input
            ("203", "Area", "Select faces with similar area", 203),
            ("204", "Sides", "Select faces with similar number of sides", 204),
            ("205", "Perimeter", "Select faces with similar perimeter", 205),
            ("206", "Normal", "Select faces with similar normal", 206),
            ("207", "CoPlanar", "Select coplanar faces", 207)
            # SIMFACE_SMOOTH, FREESTYLE are skipped for now too
        ]

    cmp_modes = [
            ("0", "=", "Compare by ==", 0),
            ("1", ">=", "Compare by >=", 1),
            ("2", "<=", "Compare by <=", 2)
        ]

    def update_mode(self, context):
        self.outputs['Vertices'].hide_safe = (self.mode != "verts")
        self.outputs['Edges'].hide_safe = (self.mode != "edges")
        self.outputs['Faces'].hide_safe = (self.mode != "faces")

        updateNode(self, context)

    mode = EnumProperty(name = "Select",
            items = modes,
            default = "faces",
            update = update_mode)

    vertex_mode = EnumProperty(name = "Select by",
            items = vertex_modes,
            default = "0",
            update = update_mode)
    edge_mode = EnumProperty(name = "Select by",
            items = edge_modes,
            default = "101",
            update = update_mode)
    face_mode = EnumProperty(name = "Select by",
            items = face_modes,
            default = "203",
            update = update_mode)

    compare = EnumProperty(name = "Compare by",
            items = cmp_modes,
            default = "0",
            update = update_mode)

    threshold = FloatProperty(name = "Threshold",
            min=0.0, default=0.1,
            update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode", expand=True)
        if self.mode == "verts":
            layout.prop(self, "vertex_mode")
        elif self.mode == "edges":
            layout.prop(self, "edge_mode")
        elif self.mode == "faces":
            layout.prop(self, "face_mode")
        layout.prop(self, "compare", expand=True)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Edges")
        self.inputs.new('StringsSocket', "Faces")
        self.inputs.new('StringsSocket', "Mask")

        self.inputs.new('StringsSocket', "Threshold").prop_name = "threshold"

        self.outputs.new('StringsSocket', "Mask")
        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")
        self.outputs.new('StringsSocket', "Faces")

        self.update_mode(context)

    def get_mask(self, new_geom, old_geom):
        mask = []
        for item in old_geom:
            mask.append(item in new_geom)
        return mask

    def process(self):
        if not any(output.is_linked for output in self.outputs):
            return

        vertices_s = self.inputs['Vertices'].sv_get()
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Faces'].sv_get(default=[[]])
        masks_s = self.inputs['Mask'].sv_get(default=[[1]])
        thresholds = self.inputs['Threshold'].sv_get()[0]

        result_verts = []
        result_edges = []
        result_faces = []
        result_mask = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s, masks_s, thresholds])
        for vertices, edges, faces, masks, threshold in zip(*meshes):
            if self.mode == "verts":
                fullList(masks,  len(vertices))
                fullList(thresholds,  len(vertices))
            elif self.mode == "edges":
                fullList(masks,  len(edges))
                fullList(thresholds,  len(edges))
            elif self.mode == "faces":
                fullList(masks,  len(faces))
                fullList(thresholds,  len(faces))

            bm = bmesh_from_pydata(vertices, edges, faces, normal_update=True)

            if self.mode == "verts":
                selected_verts = [v for v, m in zip(bm.verts, masks) if m]
                geom = bmesh.ops.similar_verts(bm, verts = selected_verts,
                        type = int(self.vertex_mode),
                        thresh = threshold,
                        compare = int(self.compare))
                s_verts = [list(v.co[:]) for v in geom['verts']]
                v_mask = self.get_mask(geom['verts'], bm.verts)
                result_verts.append(s_verts)
                result_mask.append(v_mask)

            elif self.mode == "edges":
                selected_edges = []
                for m, edge in zip(masks, edges):
                    if not m:
                        continue
                    found = False
                    for bmesh_edge in bm.edges:
                        if set(v.index for v in bmesh_edge.verts) == set(edge):
                            found = True
                            break
                    if found:
                        selected_edges.append(bmesh_edge)
                    else:
                        print("Cant find edge: " + str(edge))

                geom = bmesh.ops.similar_edges(bm, edges = selected_edges,
                        type = int(self.edge_mode),
                        thresh = threshold,
                        compare = int(self.compare))
                s_edges = [[v.index for v in e.verts] for e in geom['edges']]
                e_mask = self.get_mask(geom['edges'], bm.edges)
                result_edges.append(s_edges)
                result_mask.append(e_mask)

            elif self.mode == "faces":
                selected_faces = []
                for m, face in zip(masks, faces):
                    if not m:
                        continue
                    found = False
                    for bmesh_face in bm.faces:
                        if set(v.index for v in bmesh_face.verts) == set(face):
                            #print("Found: " + str(face))
                            found = True
                            break
                    if found:
                        selected_faces.append(bmesh_face)
                    else:
                        print("Cant find face: " + str(face))

                geom = bmesh.ops.similar_faces(bm, faces = selected_faces,
                        type = int(self.face_mode),
                        thresh = threshold,
                        compare = int(self.compare))
                s_faces = [[v.index for v in f.verts] for f in geom['faces']]
                f_mask = self.get_mask(geom['faces'], bm.faces)
                result_faces.append(s_faces)
                result_mask.append(f_mask)

            else:
                raise NotImplementedError("Unsupported mode: " + self.mode)

            bm.free()

        self.outputs['Mask'].sv_set(result_mask)
        self.outputs['Vertices'].sv_set(result_verts)
        self.outputs['Edges'].sv_set(result_edges)
        self.outputs['Faces'].sv_set(result_faces)

def register():
    bpy.utils.register_class(SvSelectSimilarNode)


def unregister():
    bpy.utils.unregister_class(SvSelectSimilarNode)

