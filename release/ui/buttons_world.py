
import bpy

class WorldButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "world"

	def poll(self, context):
		return (context.scene.world != None)
	
class WORLD_PT_world(WorldButtonsPanel):
	__label__ = "World"

	def draw(self, context):
		world = context.scene.world
		layout = self.layout
		
		layout.row()
		layout.itemR(world, "blend_sky")
		layout.itemR(world, "paper_sky")
		layout.itemR(world, "real_sky")
		
		layout.row()
		layout.itemR(world, "horizon_color")
		layout.itemR(world, "zenith_color")
		layout.itemR(world, "ambient_color")
		
class WORLD_PT_color_correction(WorldButtonsPanel):
	__label__ = "Color Correction"

	def draw(self, context):
		world = context.scene.world
		layout = self.layout

		layout.row()
		layout.itemR(world, "exposure")
		layout.itemR(world, "range")
	
class WORLD_PT_mist(WorldButtonsPanel):
	__label__ = "Mist"

	def draw(self, context):
		world = context.scene.world
		layout = self.layout

		layout.row()
		layout.itemR(world.mist, "enabled", text="Enable")
		if (world.mist.enabled):
	
			layout.column_flow()
			layout.itemR(world.mist, "start")
			layout.itemR(world.mist, "depth")
			layout.itemR(world.mist, "height")
			layout.itemR(world.mist, "intensity")
			layout.column()
			layout.itemL(text="Fallof:")
			layout.itemR(world.mist, "falloff", expand=True)
		
class WORLD_PT_stars(WorldButtonsPanel):
	__label__ = "Stars"

	def draw(self, context):
		world = context.scene.world
		layout = self.layout

		layout.row()
		layout.itemR(world.stars, "enabled", text="Enable")
		if (world.stars.enabled):

			layout.column_flow()
			layout.itemR(world.stars, "size")
			layout.itemR(world.stars, "min_distance", text="MinDist")
			layout.itemR(world.stars, "average_separation", text="StarDist")
			layout.itemR(world.stars, "color_randomization", text="Colnoise")
		
class WORLD_PT_ambient_occlusion(WorldButtonsPanel):
	__label__ = "Ambient Occlusion"

	def draw(self, context):
		world = context.scene.world
		layout = self.layout

		ao = world.ambient_occlusion
		
		layout.row()
		layout.itemR(ao, "enabled", text="Enable")
		if (ao.enabled):

			layout.row()
			layout.itemR(ao, "gather_method", expand=True)
			
			if ao.gather_method == 'RAYTRACE':
				layout.row()
				layout.itemR(ao, "samples")
				layout.itemR(ao, "distance")
				
				layout.row()
				layout.itemR(ao, "sample_method")
				if ao.sample_method == 'ADAPTIVE_QMC':
					layout.row()
					layout.itemR(ao, "threshold")
					layout.itemR(ao, "adapt_to_speed")
					
				if ao.sample_method == 'CONSTANT_JITTERED':
					layout.row()
					layout.itemR(ao, "bias")
							
			if ao.gather_method == 'APPROXIMATE':
				layout.row()
				layout.itemR(ao, "passes")
				layout.itemR(ao, "error_tolerance")
				
				layout.row()
				layout.itemR(ao, "correction")
				layout.itemR(ao, "pixel_cache")

			layout.row()
			layout.itemS()
				
			layout.row()
			layout.itemR(ao, "falloff")	
			layout.itemR(ao, "strength")
			
			layout.column()
			layout.itemR(ao, "blend_mode", expand=True)
			layout.itemR(ao, "color", expand=True)
			layout.itemR(ao, "energy")
	
bpy.types.register(WORLD_PT_world)
bpy.types.register(WORLD_PT_mist)
bpy.types.register(WORLD_PT_stars)
bpy.types.register(WORLD_PT_ambient_occlusion)
bpy.types.register(WORLD_PT_color_correction)

