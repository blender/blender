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

from mathutils import Vector, Matrix

import bpy
from bpy.props import EnumProperty, IntProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

class Vertices(object):
    outputs = [
            ('VerticesSocket', 'YesVertices'),
            ('VerticesSocket', 'NoVertices'),
            ('StringsSocket', 'VerticesMask'),
            ('StringsSocket', 'YesEdges'),
            ('StringsSocket', 'NoEdges'),
            ('StringsSocket', 'YesFaces'),
            ('StringsSocket', 'NoFaces'),
        ]

    submodes = [
            ("Wire", "Wire", "Wire", 1),
            ("Boundary", "Boundary", "Boundary", 2),
            ("Interior", "Interior", "Interior", 3)
        ]

    default_submode = "Interior"

    @staticmethod
    def on_update_submode(node):
        node.outputs['YesFaces'].hide = node.submode == "Wire"
        node.outputs['NoFaces'].hide = node.submode == "Wire"

    @staticmethod
    def process(bm, submode):

        def find_new_idxs(data, old_idxs):
            new_idxs = []
            for old_idx in old_idxs:
                new = [n for (co, o, n) in data if o == old_idx]
                if not new:
                    return None
                new_idxs.append(new[0])
            return new_idxs

        def is_good(v):
            if submode == "Wire":
                return v.is_wire
            if submode == "Boundary":
                return v.is_boundary
            if submode == "Interior":
                return (v.is_manifold and not v.is_boundary)

        good = []
        bad = []

        good_idx = 0
        bad_idx = 0
        mask = []

        for v in bm.verts:
            co = tuple(v.co)
            ok = is_good(v)
            if ok:
                good.append((co, v.index, good_idx))
                good_idx += 1
            else:
                bad.append((co, v.index, bad_idx))
                bad_idx += 1
            mask.append(ok)

        good_vertices = [x[0] for x in good]
        bad_vertices = [x[0] for x in bad]

        good_edges = []
        bad_edges = []

        for e in bm.edges:
            sv1, sv2 = e.verts[0].index, e.verts[1].index
            good_edge = find_new_idxs(good, [sv1, sv2])
            if good_edge:
                good_edges.append(good_edge)
            bad_edge = find_new_idxs(bad, [sv1, sv2])
            if bad_edge:
                bad_edges.append(bad_edge)

        good_faces = []
        bad_faces = []

        for f in bm.faces:
            vs = [v.index for v in f.verts]
            good_face = find_new_idxs(good, vs)
            if good_face:
                good_faces.append(good_face)
            bad_face = find_new_idxs(bad, vs)
            if bad_face:
                bad_faces.append(bad_face)

        return [good_vertices, bad_vertices, mask,
                good_edges, bad_edges,
                good_faces, bad_faces]

class Edges(object):
    outputs = [
            ('StringsSocket', 'YesEdges'),
            ('StringsSocket', 'NoEdges'),
            ('StringsSocket', 'Mask'),
        ]
    
    submodes = [
            ("Wire", "Wire", "Wire", 1),
            ("Boundary", "Boundary", "Boundary", 2),
            ("Interior", "Interior", "Interior", 3),
            ("Convex", "Convex", "Convex", 4),
            ("Concave", "Concave", "Concave", 5),
            ("Contiguous", "Contiguous", "Contiguous", 6),
        ]

    default_submode = "Interior"

    @staticmethod
    def process(bm, submode):

        good = []
        bad = []
        mask = []

        def is_good(e):
            if submode == "Wire":
                return e.is_wire
            if submode == "Boundary":
                return e.is_boundary
            if submode == "Interior":
                return (e.is_manifold and not e.is_boundary)
            if submode == "Convex":
                return e.is_convex
            if submode == "Concave":
                return e.is_contiguous and not e.is_convex
            if submode == "Contiguous":
                return e.is_contiguous

        for e in bm.edges:
            idxs = (e.verts[0].index, e.verts[1].index)
            ok = is_good(e)
            if ok:
                good.append(idxs)
            else:
                bad.append(idxs)
            mask.append(ok)

        return [good, bad, mask]

class Faces(object):
    outputs = [
            ('StringsSocket', 'Interior'),
            ('StringsSocket', 'Boundary'),
            ('StringsSocket', 'BoundaryMask'),
        ]
    
    @staticmethod
    def process(bm, submode):
        interior = []
        boundary = []
        mask = []

        for f in bm.faces:
            idxs = [v.index for v in f.verts]
            is_boundary = False
            for e in f.edges:
                if e.is_boundary:
                    is_boundary = True
                    break
            if is_boundary:
                boundary.append(idxs)
            else:
                interior.append(idxs)
            mask.append(int(is_boundary))

        return [interior, boundary, mask]


class SvMeshFilterNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Filter mesh elements: manifold vs boundary etc. '''
    bl_idname = 'SvMeshFilterNode'
    bl_label = 'Mesh filter'
    bl_icon = 'FILTER'

    modes = [
            ("Vertices", "Vertices", "Filter vertices", 0),
            ("Edges", "Edges", "Filter edges", 1),
            ("Faces", "Faces", "Filter faces", 2)
        ]

    def update_mode(self, context):
        cls = globals()[self.mode]
        while len(self.outputs) > 0:
            self.outputs.remove(self.outputs[0])
        for ocls, oname in cls.outputs:
            self.outputs.new(ocls, oname)
        if hasattr(cls, "default_submode"):
            self.submode = cls.default_submode
        else:
            self.submode = None
        updateNode(self, context)

    def update_submode(self, context):
        cls = globals()[self.mode]
        if hasattr(cls, "on_update_submode"):
            cls.on_update_submode(self)
        updateNode(self, context)

    mode = EnumProperty(name="Mode",
            items=modes,
            default='Vertices',
            update=update_mode)

    def get_submodes(self, context):
        cls = globals()[self.mode]
        if hasattr(cls, "submodes"):
            return cls.submodes
        else:
            return []

    submode = EnumProperty(name="Filter",
                items = get_submodes,
                update = update_submode)

    def draw_buttons(self, context, layout):
        layout.prop(self, 'mode', expand=True)
        cls = globals()[self.mode]
        if hasattr(cls, "submodes"):
            layout.prop(self, 'submode', expand=False)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Edges")
        self.inputs.new('StringsSocket', "Polygons")

        self.update_mode(context)
        self.update_submode(context)

    def process(self):

        if not any(output.is_linked for output in self.outputs):
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])

        cls = globals()[self.mode]
        results = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s])
        for vertices, edges, faces in zip(*meshes):
            bm = bmesh_from_pydata(vertices, edges, faces)
            bm.normal_update()
            outs = cls.process(bm, self.submode)
            results.append(outs)

        results = zip(*results)
        for (ocls,oname), result in zip(cls.outputs, results):
            if self.outputs[oname].is_linked:
                self.outputs[oname].sv_set(result)

def register():
    bpy.utils.register_class(SvMeshFilterNode)


def unregister():
    bpy.utils.unregister_class(SvMeshFilterNode)


