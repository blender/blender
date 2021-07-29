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
from bpy.props import FloatProperty
import bmesh

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, Vector_generate, repeat_last


#
# Remove Doubles
# by Linus Yng


def remove_doubles(vertices, faces, d, find_doubles=False):

    if faces:
        EdgeMode = (len(faces[0]) == 2)

    bm = bmesh.new()
    bm_verts = [bm.verts.new(v) for v in vertices]

    if faces:
        if EdgeMode:
            for edge in faces:
                bm.edges.new([bm_verts[i] for i in edge])
        else:
            for face in faces:
                bm.faces.new([bm_verts[i] for i in face])

    if find_doubles:
        res = bmesh.ops.find_doubles(bm, verts=bm_verts, dist=d)
        doubles = [vert.co[:] for vert in res['targetmap'].keys()]
    else:
        doubles = []

    bmesh.ops.remove_doubles(bm, verts=bm_verts, dist=d)
    edges = []
    faces = []
    bm.verts.index_update()
    verts = [vert.co[:] for vert in bm.verts[:]]

    bm.edges.index_update()
    bm.faces.index_update()
    for edge in bm.edges[:]:
        edges.append([v.index for v in edge.verts[:]])
    for face in bm.faces:
        faces.append([v.index for v in face.verts[:]])

    bm.clear()
    bm.free()
    return (verts, edges, faces, doubles)


class SvRemoveDoublesNode(bpy.types.Node, SverchCustomTreeNode):
    '''Remove doubles'''
    bl_idname = 'SvRemoveDoublesNode'
    bl_label = 'Remove Doubles'
    bl_icon = 'OUTLINER_OB_EMPTY'

    distance = FloatProperty(
        name='Distance', description='Remove distance',
        default=0.001, precision=3, min=0, update=updateNode
    )

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Distance').prop_name = 'distance'
        self.inputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.inputs.new('StringsSocket', 'PolyEdge', 'PolyEdge')

        self.outputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges', 'Edges')
        self.outputs.new('StringsSocket', 'Polygons', 'Polygons')
        self.outputs.new('VerticesSocket', 'Doubles', 'Doubles')

    def draw_buttons(self, context, layout):
        #layout.prop(self, 'distance', text="Distance")
        pass

    def process(self):
        if not any(s.is_linked for s in self.outputs):
            return

        if not self.inputs['Vertices'].is_linked:
            return

        verts = Vector_generate(self.inputs['Vertices'].sv_get())
        polys = self.inputs['PolyEdge'].sv_get(default=[[]])
        distance = self.inputs['Distance'].sv_get(default=[self.distance])[0]
        has_double_out = self.outputs['Doubles'].is_linked

        verts_out = []
        edges_out = []
        polys_out = []
        d_out = []

        for v, p, d in zip(verts, polys, repeat_last(distance)):
            res = remove_doubles(v, p, d, has_double_out)
            if not res:
                return
            verts_out.append(res[0])
            edges_out.append(res[1])
            polys_out.append(res[2])
            d_out.append(res[3])

        self.outputs['Vertices'].sv_set(verts_out)

        # restrict setting this output when there is no such input
        if self.inputs['PolyEdge'].is_linked:
            self.outputs['Edges'].sv_set(edges_out)
            self.outputs['Polygons'].sv_set(polys_out)

        if self.outputs['Doubles'].is_linked:
            self.outputs['Doubles'].sv_set(d_out)


def register():
    bpy.utils.register_class(SvRemoveDoublesNode)


def unregister():
    bpy.utils.unregister_class(SvRemoveDoublesNode)
