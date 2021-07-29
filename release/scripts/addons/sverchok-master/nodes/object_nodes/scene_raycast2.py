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
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, match_long_repeat)


class SvSCNRayCastNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' RayCast Scene '''
    bl_idname = 'SvSCNRayCastNodeMK2'
    bl_label = 'Scene Raycast MK2' #new is nonsense name
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        si,so = self.inputs.new,self.outputs.new
        si('VerticesSocket', 'origin').use_prop = True
        si('VerticesSocket', 'direction').use_prop = True
        so('VerticesSocket', "HitP")
        so('VerticesSocket', "HitNorm")
        so('StringsSocket', "Succes")
        so('StringsSocket', "FaceIndex")
        so("SvObjectSocket", "Objects")
        so("MatrixSocket", "hited object matrix")

    def process(self):
        P,N,S,I,O,M = self.outputs
        rc = []
        st = self.inputs['origin'].sv_get()[0]
        en = self.inputs['direction'].sv_get()[0]
        st, en = match_long_repeat([st, en])
        for i,i2 in zip(st,en):
            rc.append(bpy.context.scene.ray_cast(i, i2))
        if P.is_linked:
            P.sv_set([[i[1][:] for i in rc]])
        if N.is_linked:
            N.sv_set([[i[2][:] for i in rc]])
        if S.is_linked:
            S.sv_set([[i[0] for i in rc]])
        if I.is_linked:
            I.sv_set([[i[3] for i in rc]])
        if O.is_linked:
            O.sv_set([i[4] for i in rc])
        if M.is_linked:
            M.sv_set([[v[:] for v in i[5]] for i in rc])

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvSCNRayCastNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvSCNRayCastNodeMK2)
