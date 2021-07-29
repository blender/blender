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
from bpy.props import StringProperty, EnumProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode

class SvParticlesNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Particles input node '''
    bl_idname = 'SvParticlesNode'
    bl_label = 'Particles'
    bl_icon = 'PARTICLES'

    def sv_init(self, context):
        self.inputs.new('SvObjectSocket', "Object", "Object")
        self.outputs.new('VerticesSocket', "Vertices", "Vertices")

    def avail_objects(self, context):
        items = [('','','')]
        if self.inputs and self.inputs[0].is_linked:
            objects = self.inputs[0].sv_get()
            items = [(obj.name, obj.name, '') for obj in objects]
        return items

    def avail_particles(self, context):
        items = [('','','')]
        if self.inputs and self.inputs[0].is_linked:
            obj = bpy.data.objects[self.objects]
            if obj.particle_systems:
                items = [(p.name, p.name, "") for p in obj.particle_systems]
        return items

    objects = EnumProperty(items=avail_objects, name="Objects",
        description="Choose Objects", update=updateNode)
    particles = EnumProperty(items=avail_particles, name="Particles",
        description="Choose Particles to load", update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, 'particles', 'particles')


    def update(self):
        pass

    def process(self):
        if self.inputs and self.inputs[0].is_linked:
            obj = bpy.data.objects[self.objects]
            if not self.particles:
                print ('!!! for node:',self.name,'!!! object',self.objects,'have no particle system')
                if self.outputs and self.outputs[0].is_linked:
                    self.outputs[0].sv_set([[]])
                return
            particles = obj.particle_systems[self.particles].particles
            locs = []
            add_loc = locs.append
            for i in range(len(particles)):
                pt = particles[i]
                if pt.is_exist and pt.alive_state == 'ALIVE':
                    add_loc(pt.location[:])

            if self.outputs and self.outputs[0].is_linked:
                self.outputs[0].sv_set([locs])


def register():
    bpy.utils.register_class(SvParticlesNode)


def unregister():
    bpy.utils.unregister_class(SvParticlesNode)

if __name__ == '__main__':
    register()
