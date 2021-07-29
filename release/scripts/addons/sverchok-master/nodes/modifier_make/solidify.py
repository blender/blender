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
from sverchok.data_structure import updateNode, repeat_last, fullList
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
# by Linus Yng


def solidify(vertices, faces, t):

    if not faces or not vertices:
        return False

    if len(faces[0]) == 2:
        return False

    verlen = set(range(len(vertices)))

    bm = bmesh_from_pydata(vertices, [], faces, normal_update=True)
    geom_in = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.solidify(bm, geom=geom_in, thickness=t[0])

    edges = []
    faces = []
    newpols = []
    bm.verts.index_update()
    bm.edges.index_update()
    bm.faces.index_update()
    for edge in bm.edges[:]:
        edges.append([v.index for v in edge.verts[:]])
    verts = [vert.co[:] for vert in bm.verts[:]]
    for face in bm.faces:
        indexes = [v.index for v in face.verts[:]]
        faces.append(indexes)
        if not verlen.intersection(indexes):
            newpols.append(indexes)
    bm.clear()
    bm.free()
    return (verts, edges, faces, newpols)


class SvSolidifyNode(bpy.types.Node, SverchCustomTreeNode):
    '''Soldifies geometry'''
    bl_idname = 'SvSolidifyNode'
    bl_label = 'Solidify'
    bl_icon = 'MOD_SOLIDIFY'

    thickness = FloatProperty(
        name='Thickness', description='Shell thickness',
        default=0.1, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'thickness').prop_name = 'thickness'
        self.inputs.new('VerticesSocket', 'vertices')
        self.inputs.new('StringsSocket', 'polygons')

        self.outputs.new('VerticesSocket', 'vertices')
        self.outputs.new('StringsSocket', 'edges')
        self.outputs.new('StringsSocket', 'polygons')
        self.outputs.new('StringsSocket', 'newpols')


    def process(self):
        if not any((s.is_linked for s in self.outputs)):
            return

        if not (self.inputs['vertices'].is_linked and self.inputs['polygons'].is_linked):
            return

        verts = self.inputs['vertices'].sv_get()
        polys = self.inputs['polygons'].sv_get()
        thickness = self.inputs['thickness'].sv_get()

        verts_out = []
        edges_out = []
        polys_out = []
        newpo_out = []
        fullList(thickness, len(verts))
        for v, p, t in zip(verts, polys, thickness):
            res = solidify(v, p, t)

            if not res:
                return
            verts_out.append(res[0])
            edges_out.append(res[1])
            polys_out.append(res[2])
            newpo_out.append(res[3])

        self.outputs['vertices'].sv_set(verts_out)
        self.outputs['edges'].sv_set(edges_out)
        self.outputs['polygons'].sv_set(polys_out)
        self.outputs['newpols'].sv_set(newpo_out)


def register():
    bpy.utils.register_class(SvSolidifyNode)


def unregister():
    bpy.utils.unregister_class(SvSolidifyNode)
