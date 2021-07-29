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
from bpy.props import FloatProperty, BoolProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, second_as_first_cycle)


class SvPointOnMeshNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Point on Mesh '''
    bl_idname = 'SvPointOnMeshNode'
    bl_label = 'Object ID Point on Mesh'
    bl_icon = 'OUTLINER_OB_EMPTY'

    Mdist = FloatProperty(name='Max_Distance', default=10, update=updateNode)
    mode = BoolProperty(name='for in points', default=False, update=updateNode)
    mode2 = BoolProperty(name='for out points', default=True, update=updateNode)

    def sv_init(self, context):
        si,so = self.inputs.new,self.outputs.new
        si('StringsSocket', 'Objects')
        si('VerticesSocket', "point").use_prop = True
        si('StringsSocket', "max_dist").prop_name = "Mdist"
        so('VerticesSocket', "Point_on_mesh")
        so('VerticesSocket', "Normal_on_mesh")
        so('StringsSocket', "FaceINDEX")

    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        row.prop(self,    "mode",   text="In Mode")
        row.prop(self,    "mode2",   text="Out Mode")

    def process(self):
        o,p,md = self.inputs
        P,N,I = self.outputs
        Out,point,sm1,sm2 = [],p.sv_get()[0],self.mode,self.mode2
        obj = o.sv_get()
        max_dist = second_as_first_cycle(obj, md.sv_get()[0])
        for i,i2 in zip(obj,max_dist):
            if sm1:
                Out.append([i.closest_point_on_mesh(i.matrix_local.inverted()*Vector(p), i2) for p in point])
            else:
                Out.append([i.closest_point_on_mesh(p, i2) for p in point])
        if P.is_linked:
            if sm2:
                out =[]
                for i,i2 in zip(obj,Out):
                    out.append([(i.matrix_world*i3[0])[:] for i3 in i2])
                P.sv_set(out)
            else:
                P.sv_set([[i2[0][:] for i2 in o] for o in Out])
        if N.is_linked:
            N.sv_set([[i2[1][:] for i2 in o] for o in Out])
        if I.is_linked:
            I.sv_set([[i2[2] for i2 in o] for o in Out])

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvPointOnMeshNode)


def unregister():
    bpy.utils.unregister_class(SvPointOnMeshNode)
