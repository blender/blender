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
import mathutils
from mathutils import Vector
from bpy.props import BoolProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, match_long_repeat)


class SvRayCastNode(bpy.types.Node, SverchCustomTreeNode):
    ''' RayCast Object '''
    bl_idname = 'SvRayCastObjectNode'
    bl_label = 'Object ID Raycast'
    bl_icon = 'OUTLINER_OB_EMPTY'

    mode = BoolProperty(name='input mode', default=False, update=updateNode)
    mode2 = BoolProperty(name='output mode', default=False, update=updateNode)

    def sv_init(self, context):
        si,so = self.inputs.new,self.outputs.new
        si('StringsSocket', 'Objects')
        si('VerticesSocket', 'start').use_prop = True
        si('VerticesSocket', 'end').use_prop = True
        so('VerticesSocket', "HitP")
        so('VerticesSocket', "HitNorm")
        so('StringsSocket', "FaceINDEX")

    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        row.prop(self,    "mode",   text="In Mode")
        row.prop(self,    "mode2",   text="Out Mode")

    def process(self):
        o,s,e = self.inputs
        P,N,I = self.outputs
        outfin,OutLoc,obj,sm1,sm2 = [],[],o.sv_get(),self.mode,self.mode2
        st, en = match_long_repeat([s.sv_get()[0], e.sv_get()[0]])
        for OB in obj:
            if sm1:
                obm = OB.matrix_local.inverted()
                outfin.append([OB.ray_cast(obm*Vector(i), obm*Vector(i2)) for i,i2 in zip(st,en)])
            else:
                outfin.append([OB.ray_cast(i,i2) for i,i2 in zip(st,en)])
        if sm2:
            if P.is_linked:
                for i,i2 in zip(obj,outfin):
                    omw = i.matrix_world
                    OutLoc.append([(omw*i[0])[:] for i in i2])
                P.sv_set(OutLoc)
        else:
            if P.is_linked:
                P.sv_set([[i[0][:] for i in i2] for i2 in outfin])
        if N.is_linked:
            N.sv_set([[i[1][:] for i in i2] for i2 in outfin])
        if I.is_linked:
            I.sv_set([[i[2] for i in i2] for i2 in outfin])

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvRayCastNode)


def unregister():
    bpy.utils.unregister_class(SvRayCastNode)
