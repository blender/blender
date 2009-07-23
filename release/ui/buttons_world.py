
import bpy

class WorldButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "world"

	def poll(self, context):
		rd = context.scene.render_data
		return (context.world != None) and (not rd.use_game_engine)

class WORLD_PT_preview(WorldButtonsPanel):
	__label__ = "Preview"

	def draw(self, context):
		layout = self.layout
		world = context.world
		
		layout.template_preview(world)
	
class WORLD_PT_context_world(WorldButtonsPanel):
	__no_header__ = True

	def poll(self, context):
		rd = context.scene.render_data
		return (context.scene != None) and (not rd.use_game_engine)

	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		world = context.world
		space = context.space_data

		split = layout.split(percentage=0.65)

		if scene:
			split.template_ID(scene, "world", new="world.new")
		elif world:
			split.template_ID(space, "pin_id")

class WORLD_PT_world(WorldButtonsPanel):
	__label__ = "World"

	def draw(self, context):
		layout = self.layout

		world = context.world

		if world:
		
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

		layout.itemR(world.mist, "falloff")
		
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
		flow.itemR(world.stars, "color_randomization", text="Colors")
		flow.itemR(world.stars, "min_distance", text="Min. Dist")
		flow.itemR(world.stars, "average_separation", text="Separation")
		
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

		split = layout.split()
		
		col = split.column()
		col.itemL(text="Attenuation:")
		col.itemR(ao, "distance")
		col.itemR(ao, "falloff")
		sub = col.row()
		sub.active = ao.falloff
		sub.itemR(ao, "falloff_strength", text="Strength")
	
		if ao.gather_method == 'RAYTRACE':
			col = split.column()
			
			col.itemL(text="Sampling:")
			col.itemR(ao, "sample_method", text="")

			sub = col.column(align=True)
			sub.itemR(ao, "samples")

			if ao.sample_method == 'ADAPTIVE_QMC':
				sub.itemR(ao, "threshold")
				sub.itemR(ao, "adapt_to_speed")
			elif ao.sample_method == 'CONSTANT_JITTERED':
				sub.itemR(ao, "bias")
						
		if ao.gather_method == 'APPROXIMATE':
			col = split.column()
			
			col.itemL(text="Sampling:")
			col.itemR(ao, "error_tolerance", text="Error")
			col.itemR(ao, "pixel_cache")
			col.itemR(ao, "correction")
			
		col = layout.column(align=True)
		col.itemL(text="Influence:")
		row = col.row()
		row.itemR(ao, "blend_mode", text="")
		row.itemR(ao, "color", text="")
		row.itemR(ao, "energy", text="")

bpy.types.register(WORLD_PT_context_world)	
bpy.types.register(WORLD_PT_preview)
bpy.types.register(WORLD_PT_world)
bpy.types.register(WORLD_PT_ambient_occlusion)
bpy.types.register(WORLD_PT_mist)
bpy.types.register(WORLD_PT_stars)

