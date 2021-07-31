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
from bpy.props import FloatProperty, BoolProperty
import bmesh

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, Vector_generate, repeat_last
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

def wireframe(vertices, faces, t, self):
    if not faces or not vertices:
        return False

    if len(faces[0]) == 2:
        return False

    bm = bmesh_from_pydata(vertices, [], faces, normal_update=True)
    bmesh.ops.wireframe(
        bm, faces=bm.faces[:],
        thickness=t,
        offset=self.offset,
        use_replace=self.replace,
        use_boundary=self.boundary,
        use_even_offset=self.even_offset,
        use_relative_offset=self.relative_offset)

    return pydata_from_bmesh(bm)


class SvWireframeNode(bpy.types.Node, SverchCustomTreeNode):
    '''Wireframe'''
    bl_idname = 'SvWireframeNode'
    bl_label = 'Wireframe'
    bl_icon = 'MOD_WIREFRAME'

    thickness = FloatProperty(
        name='thickness', description='thickness',
        default=0.01, min=0.0,
        update=updateNode)

    offset = FloatProperty(
        name='offset', description='offset',
        default=0.01, min=0.0,
        update=updateNode)

    replace = BoolProperty(
        name='replace', description='replace',
        default=True,
        update=updateNode)

    even_offset = BoolProperty(
        name='even_offset', description='even_offset',
        default=True,
        update=updateNode)

    relative_offset = BoolProperty(
        name='relative_offset', description='even_offset',
        default=False,
        update=updateNode)

    boundary = BoolProperty(
        name='boundary', description='boundry',
        default=True,
        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'thickness').prop_name = 'thickness'
        self.inputs.new('StringsSocket', 'Offset').prop_name = 'offset'
        self.inputs.new('VerticesSocket', 'vertices')
        self.inputs.new('StringsSocket', 'polygons')

        self.outputs.new('VerticesSocket', 'vertices')
        self.outputs.new('StringsSocket', 'edges')
        self.outputs.new('StringsSocket', 'polygons')

    def draw_buttons(self, context, layout):
        layout.prop(self, 'boundary', text="Boundary")
        layout.prop(self, 'even_offset', text="Offset even")
        layout.prop(self, 'relative_offset', text="Offset relative")
        layout.prop(self, 'replace', text="Replace")

    def process(self):
        inputs, outputs = self.inputs, self.outputs

        if not all(s.is_linked for s in [inputs['vertices'], inputs['polygons']]):
            return

        poly_or_edge_linked = (outputs['edges'].is_linked or outputs['polygons'].is_linked)
        if not (outputs['vertices'].is_linked and poly_or_edge_linked):
            # doesn't make a lot of sense to process or even
            # output edges/polygons without the assocated vertex locations
            return

        verts = Vector_generate(inputs['vertices'].sv_get())
        polys = inputs['polygons'].sv_get()
        thickness = inputs['thickness'].sv_get()[0]

        verts_out = []
        edges_out = []
        polys_out = []
        for v, p, t in zip(verts, polys, repeat_last(thickness)):
            res = wireframe(v, p, t, self)
            if not res:
                return

            verts_out.append(res[0])
            edges_out.append(res[1])
            polys_out.append(res[2])

        outputs['vertices'].sv_set(verts_out)
        outputs['edges'].sv_set(edges_out)
        outputs['polygons'].sv_set(polys_out)


def register():
    bpy.utils.register_class(SvWireframeNode)


def unregister():
    bpy.utils.unregister_class(SvWireframeNode)
