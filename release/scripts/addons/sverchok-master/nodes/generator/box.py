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
from bpy.props import IntProperty, FloatProperty
import bmesh
from mathutils import Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata

class SvBoxNode(bpy.types.Node, SverchCustomTreeNode):
    """
    Triggers: Box
    Tooltip: Generate a Box primitive.
    """

    bl_idname = 'SvBoxNode'
    bl_label = 'Box'
    bl_icon = 'MESH_CUBE'

    Divx = IntProperty(
        name='Divx', description='divisions x',
        default=1, min=1, options={'ANIMATABLE'},
        update=updateNode)

    Divy = IntProperty(
        name='Divy', description='divisions y',
        default=1, min=1, options={'ANIMATABLE'},
        update=updateNode)

    Divz = IntProperty(
        name='Divz', description='divisions z',
        default=1, min=1, options={'ANIMATABLE'},
        update=updateNode)

    Size = FloatProperty(
        name='Size', description='Size',
        default=1.0, options={'ANIMATABLE'},
        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Size").prop_name = 'Size'
        self.inputs.new('StringsSocket', "Divx").prop_name = 'Divx'
        self.inputs.new('StringsSocket', "Divy").prop_name = 'Divy'
        self.inputs.new('StringsSocket', "Divz").prop_name = 'Divz'
        self.outputs.new('VerticesSocket', "Vers")
        self.outputs.new('StringsSocket', "Edgs")
        self.outputs.new('StringsSocket', "Pols")

    def draw_buttons(self, context, layout):
        pass

    def makecube(self, size, divx, divy, divz):
        if 0 in (divx, divy, divz):
            return [], []

        b = size / 2.0

        verts = [
            [b, b, -b], [b, -b, -b], [-b, -b, -b],
            [-b, b, -b], [b, b, b], [b, -b, b],
            [-b, -b, b], [-b, b, b]
        ]

        faces = [[0, 1, 2, 3], [4, 7, 6, 5],
                 [0, 4, 5, 1], [1, 5, 6, 2],
                 [2, 6, 7, 3], [4, 0, 3, 7]]

        edges = [[0, 4], [4, 5], [5, 1], [1, 0],
                 [5, 6], [6, 2], [2, 1], [6, 7],
                 [7, 3], [3, 2], [7, 4], [0, 3]]

        if (divx, divy, divz) == (1, 1, 1):
            return verts, edges, faces

        bm = bmesh_from_pydata(verts, [], faces)
        dist = 0.0001
        section_dict = {0: divx, 1: divy, 2: divz}

        for axis in range(3):

            num_sections = section_dict[axis]
            if num_sections == 1:
                continue

            step = 1 / num_sections
            v1 = Vector(tuple((b if (i == axis) else 0) for i in [0, 1, 2]))
            v2 = Vector(tuple((-b if (i == axis) else 0) for i in [0, 1, 2]))

            for section in range(num_sections):
                mid_vec = v1.lerp(v2, section * step)
                plane_no = v2 - mid_vec
                plane_co = mid_vec
                visible_geom = bm.faces[:] + bm.verts[:] + bm.edges[:]

                bmesh.ops.bisect_plane(
                    bm, geom=visible_geom, dist=dist,
                    plane_co=plane_co, plane_no=plane_no,
                    use_snap_center=False,
                    clear_outer=False, clear_inner=False)

        indices = lambda i: [j.index for j in i.verts]

        verts = [list(v.co.to_tuple()) for v in bm.verts]
        faces = [indices(face) for face in bm.faces]
        edges = [indices(edge) for edge in bm.edges]
        return verts, edges, faces

    def process(self):
        inputs = self.inputs
        outputs = self.outputs

        # I think this is analoge to preexisting code, please verify.
        size = inputs['Size'].sv_get()[0]
        divx = int(inputs['Divx'].sv_get()[0][0])
        divy = int(inputs['Divy'].sv_get()[0][0])
        divz = int(inputs['Divz'].sv_get()[0][0])

        out = [a for a in (zip(*[self.makecube(s, divx, divy, divz) for s in size]))]

        # outputs, blindly using sv_set produces many print statements.
        outputs['Vers'].sv_set(out[0])
        outputs['Edgs'].sv_set(out[1])
        outputs['Pols'].sv_set(out[2])

        self.debug("hello from the box")


def register():
    bpy.utils.register_class(SvBoxNode)


def unregister():
    bpy.utils.unregister_class(SvBoxNode)

