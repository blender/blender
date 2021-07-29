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
import numpy as np
from bpy.props import BoolProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, second_as_first_cycle as safc


class SvParticlesMK2Node(bpy.types.Node, SverchCustomTreeNode):
    ''' Particles input node new '''
    bl_idname = 'SvParticlesMK2Node'
    bl_label = 'ParticlesMK2'
    bl_icon = 'PARTICLES'

    Filt_D = BoolProperty(default=True, update=updateNode)

    def draw_buttons_ext(self, context,   layout):
        layout.prop(self, "Filt_D", text="filter death")

    def sv_init(self, context):
        self.inputs.new('SvObjectSocket', "Object")
        self.inputs.new('VerticesSocket', "Velocity")
        self.inputs.new('VerticesSocket', "Location")
        self.inputs.new('StringsSocket',  "Size")
        self.outputs.new('VerticesSocket', "outLocation")
        self.outputs.new('VerticesSocket', "outVelocity")

    def process(self):
        O, V, L, S = self.inputs
        outL, outV = self.outputs
        listobj = [i.particle_systems.active.particles for i in O.sv_get() if i.particle_systems]
        if V.is_linked:
            for i, i2 in zip(listobj, V.sv_get()):
                i.foreach_set('velocity', np.array(safc(i, i2)).flatten())
        if S.is_linked:
            for i, i2 in zip(listobj, S.sv_get()):
                i.foreach_set('size', safc(i, i2))
        if L.is_linked:
            for i, i2 in zip(listobj, L.sv_get()):
                i.foreach_set('location', np.array(safc(i, i2)).flatten())
        if outL.is_linked:
            if self.Filt_D:
                outL.sv_set([[i.location[:] for i in Plist if i.alive_state == 'ALIVE'] for Plist in listobj])
            else:
                outL.sv_set([[i.location[:] for i in Plist] for Plist in listobj])
        if outV.is_linked:
            outV.sv_set([[i.velocity[:] for i in Plist] for Plist in listobj])


def register():
    bpy.utils.register_class(SvParticlesMK2Node)


def unregister():
    bpy.utils.unregister_class(SvParticlesMK2Node)
