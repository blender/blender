
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
		
		row = layout.row()
		row.itemR(world, "blend_sky")
		row.itemR(world, "paper_sky")
		row.itemR(world, "real_sky")
		
		row = layout.row()
		row.column().itemR(world, "horizon_color")
		row.column().itemR(world, "zenith_color")
		row.column().itemR(world, "ambient_color")
		
class WORLD_PT_color_correction(WorldButtonsPanel):
	__label__ = "Color Correction"

	def draw(self, context):
		world = context.scene.world
		layout = self.layout

		row = layout.row()
		row.itemR(world, "exposure")
		row.itemR(world, "range")
	
class WORLD_PT_mist(WorldButtonsPanel):
	__label__ = "Mist"

	def draw(self, context):
		world = context.scene.world
		layout = self.layout

		row = layout.row()
		row.itemR(world.mist, "enabled", text="Enable")
		if (world.mist.enabled):
	
			flow = layout.column_flow()
			flow.itemR(world.mist, "start")
			flow.itemR(world.mist, "depth")
			flow.itemR(world.mist, "height")
			flow.itemR(world.mist, "intensity")
			col = layout.column()
			col.itemL(text="Fallof:")
			col.row().itemR(world.mist, "falloff", expand=True)
		
class WORLD_PT_stars(WorldButtonsPanel):
	__label__ = "Stars"

	def draw(self, context):
		world = context.scene.world
		layout = self.layout

		row = layout.row()
		row.itemR(world.stars, "enabled", text="Enable")
		if (world.stars.enabled):

			flow = layout.column_flow()
			flow.itemR(world.stars, "size")
			flow.itemR(world.stars, "min_distance", text="MinDist")
			flow.itemR(world.stars, "average_separation", text="StarDist")
			flow.itemR(world.stars, "color_randomization", text="Colnoise")
		
class WORLD_PT_ambient_occlusion(WorldButtonsPanel):
	__label__ = "Ambient Occlusion"

	def draw(self, context):
		world = context.scene.world
		layout = self.layout

		ao = world.ambient_occlusion
		
		row = layout.row()
		row.itemR(ao, "enabled", text="Enable")
		if (ao.enabled):

			row = layout.row()
			row.itemR(ao, "gather_method", expand=True)
			
			if ao.gather_method == 'RAYTRACE':
				row = layout.row()
				row.itemR(ao, "samples")
				row.itemR(ao, "distance")
				
				row = layout.row()
				row.itemR(ao, "sample_method")
				if ao.sample_method == 'ADAPTIVE_QMC':
					row = layout.row()
					row.itemR(ao, "threshold")
					row.itemR(ao, "adapt_to_speed")
					
				if ao.sample_method == 'CONSTANT_JITTERED':
					row = layout.row()
					row.itemR(ao, "bias")
							
			if ao.gather_method == 'APPROXIMATE':
				row = layout.row()
				row.itemR(ao, "passes")
				row.itemR(ao, "error_tolerance")
				
				row = layout.row()
				row.itemR(ao, "correction")
				row.itemR(ao, "pixel_cache")

			row = layout.row()
			row.itemS()
				
			row = layout.row()
			row.itemR(ao, "falloff")	
			row.itemR(ao, "strength")
			
			col = layout.column()
			col.row().itemR(ao, "blend_mode", expand=True)
			col.row().itemR(ao, "color", expand=True)
			col.itemR(ao, "energy")
	
bpy.types.register(WORLD_PT_world)
bpy.types.register(WORLD_PT_mist)
bpy.types.register(WORLD_PT_stars)
bpy.types.register(WORLD_PT_ambient_occlusion)
bpy.types.register(WORLD_PT_color_correction)

