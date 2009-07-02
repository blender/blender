
import bpy

class WorldButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "world"

	def poll(self, context):
		return (context.world != None)

class WORLD_PT_preview(WorldButtonsPanel):
	__label__ = "Preview"

	def poll(self, context):
		return (context.scene or context.world)

	def draw(self, context):
		layout = self.layout
		world = context.world
		
		layout.template_preview(world)
	
class WORLD_PT_world(WorldButtonsPanel):
	__label__ = "World"

	def poll(self, context):
		return (context.scene != None)

	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		world = context.world
		space = context.space_data

		split = layout.split(percentage=0.65)

		if scene:
			split.template_ID(context, scene, "world", new="WORLD_OT_new")
		elif world:
			split.template_ID(context, space, "pin_id")

		split.itemS()

		if world:
			layout.itemS()
			
			row = layout.row()
			row.itemR(world, "blend_sky")
			row.itemR(world, "paper_sky")
			row.itemR(world, "real_sky")
			
			row = layout.row()
			row.column().itemR(world, "horizon_color")
			col = row.column()
			col.itemR(world, "zenith_color")
			col.active = world.blend_sky
			row.column().itemR(world, "ambient_color")
		
class WORLD_PT_color_correction(WorldButtonsPanel):
	__label__ = "Color Correction"

	def draw(self, context):
		layout = self.layout
		world = context.world

		row = layout.row()
		row.itemR(world, "exposure")
		row.itemR(world, "range")
	
class WORLD_PT_mist(WorldButtonsPanel):
	__label__ = "Mist"

	def draw_header(self, context):
		layout = self.layout
		world = context.world

		layout.itemR(world.mist, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		world = context.world

		layout.active = world.mist.enabled

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

	def draw_header(self, context):
		layout = self.layout
		world = context.world

		layout.itemR(world.stars, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		world = context.world

		layout.active = world.stars.enabled

		flow = layout.column_flow()
		flow.itemR(world.stars, "size")
		flow.itemR(world.stars, "min_distance", text="Min. Dist")
		flow.itemR(world.stars, "average_separation", text="Separation")
		flow.itemR(world.stars, "color_randomization", text="Random")
		
class WORLD_PT_ambient_occlusion(WorldButtonsPanel):
	__label__ = "Ambient Occlusion"

	def draw_header(self, context):
		layout = self.layout
		world = context.world

		layout.itemR(world.ambient_occlusion, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		ao = context.world.ambient_occlusion
		
		layout.active = ao.enabled
		
		layout.itemR(ao, "gather_method", expand=True)
		
		if ao.gather_method == 'RAYTRACE':
			split = layout.split()
			
			col = split.column()
			col.itemR(ao, "samples")
			col.itemR(ao, "distance")
			
			col = split.column()
			col.itemR(ao, "falloff")
			colsub = col.column()
			colsub.active = ao.falloff
			colsub.itemR(ao, "strength")
			
			layout.itemR(ao, "sample_method")
			if ao.sample_method == 'ADAPTIVE_QMC':
				row = layout.row()
				row.itemR(ao, "threshold")
				row.itemR(ao, "adapt_to_speed")
				
			if ao.sample_method == 'CONSTANT_JITTERED':
				row = layout.row()
				row.itemR(ao, "bias")
						
		if ao.gather_method == 'APPROXIMATE':
			split = layout.split()
			
			col = split.column()
			col.itemR(ao, "passes")
			col.itemR(ao, "error_tolerance", text="Error")
			col.itemR(ao, "correction")
			
			col = split.column() 
			col.itemR(ao, "falloff")
			colsub = col.column()
			colsub.active = ao.falloff
			colsub.itemR(ao, "strength")
			col.itemR(ao, "pixel_cache")

		col = layout.column()
		col.row().itemR(ao, "blend_mode", expand=True)
		col.row().itemR(ao, "color", expand=True)
		col.itemR(ao, "energy")
	
bpy.types.register(WORLD_PT_preview)
bpy.types.register(WORLD_PT_world)
bpy.types.register(WORLD_PT_ambient_occlusion)
bpy.types.register(WORLD_PT_mist)
bpy.types.register(WORLD_PT_stars)
bpy.types.register(WORLD_PT_color_correction)
