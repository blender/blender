
import bpy

class ParticleButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "particle"

	def poll(self, context):
		ob = context.active_object
		return (ob and ob.active_particle_system)

class PARTICLE_PT_particles(ParticleButtonsPanel):
	__idname__= "PARTICLE_PT_particles"
	__label__ = "Particles"

	def draw(self, context):
		layout = self.layout

		psys = context.active_object.active_particle_system
		part = psys.settings

		layout.itemR(part, "amount")

bpy.types.register(PARTICLE_PT_particles)

