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
from bpy.props import EnumProperty
import mathutils
from mathutils import Vector, Matrix
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, second_as_first_cycle as safc, enum_item as e)


class SvMatrixNormalNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Construct a Matirx from Normal '''
    bl_idname = 'SvMatrixNormalNode'
    bl_label = 'Matrix normal'
    bl_icon = 'OUTLINER_OB_EMPTY'

    F = ['X', 'Y', 'Z', '-X', '-Y', '-Z']
    S = ['X', 'Y', 'Z']
    track = EnumProperty(name="track", default=F[4], items=e(F), update=updateNode)
    up = EnumProperty(name="up", default=S[2], items=e(S), update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Location").use_prop = True
        self.inputs.new('VerticesSocket', "Normal").use_prop = True
        self.outputs.new('MatrixSocket', "Matrix")

    def draw_buttons(self, context, layout):
        layout.prop(self, "track", "track")
        layout.prop(self, "up", "up")

    def process(self):
        Ma = self.outputs[0]
        if not Ma.is_linked:
            return
        L, N = self.inputs
        out = []
        loc = L.sv_get()[0]
        nor = [Vector(i) for i in N.sv_get()[0]]
        nor = safc(loc, nor)
        T, U = self.track, self.up
        for V, N in zip(loc, nor):
            n = N.to_track_quat(T, U)
            m = Matrix.Translation(V) * n.to_matrix().to_4x4()
            out.append([i[:] for i in m])
        Ma.sv_set(out)


def register():
    bpy.utils.register_class(SvMatrixNormalNode)


def unregister():
    bpy.utils.unregister_class(SvMatrixNormalNode)
