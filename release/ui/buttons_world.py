
import bpy

class WorldButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "world"
	# COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here
	
	def poll(self, context):
		rd = context.scene.render_data
		return (context.world) and (not rd.use_game_engine) and (rd.engine in self.COMPAT_ENGINES)

class WORLD_PT_preview(WorldButtonsPanel):
	__label__ = "Preview"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])
	
	def draw(self, context):
		self.layout.template_preview(context.world)
	
class WORLD_PT_context_world(WorldButtonsPanel):
	__show_header__ = False
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def poll(self, context):
		rd = context.scene.render_data
		return (not rd.use_game_engine) and (rd.engine in self.COMPAT_ENGINES)

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
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw(self, context):
		layout = self.layout

		world = context.world

		row = layout.row()
		row.itemR(world, "paper_sky")
		row.itemR(world, "blend_sky")
		row.itemR(world, "real_sky")
			
		row = layout.row()
		row.column().itemR(world, "horizon_color")
		col = row.column()
		col.itemR(world, "zenith_color")
		col.active = world.blend_sky
		row.column().itemR(world, "ambient_color")
		
class WORLD_PT_mist(WorldButtonsPanel):
	__label__ = "Mist"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw_header(self, context):
		world = context.world

		self.layout.itemR(world.mist, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		
		world = context.world

		layout.active = world.mist.enabled

		flow = layout.column_flow()
		flow.itemR(world.mist, "intensity", slider=True)
		flow.itemR(world.mist, "start")
		flow.itemR(world.mist, "depth")
		flow.itemR(world.mist, "height")

		layout.itemR(world.mist, "falloff")
		
class WORLD_PT_stars(WorldButtonsPanel):
	__label__ = "Stars"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw_header(self, context):
		world = context.world

		self.layout.itemR(world.stars, "enabled", text="")

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
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw_header(self, context):
		world = context.world

		self.layout.itemR(world.ambient_occlusion, "enabled", text="")

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

			sub = col.column()
			sub.itemR(ao, "samples")

			if ao.sample_method == 'ADAPTIVE_QMC':
				sub.itemR(ao, "threshold")
				sub.itemR(ao, "adapt_to_speed", slider=True)
			elif ao.sample_method == 'CONSTANT_JITTERED':
				sub.itemR(ao, "bias")
						
		if ao.gather_method == 'APPROXIMATE':
			col = split.column()
			
			col.itemL(text="Sampling:")
			col.itemR(ao, "error_tolerance", text="Error")
			col.itemR(ao, "pixel_cache")
			col.itemR(ao, "correction")
			
		col = layout.column()
		col.itemL(text="Influence:")
		
		col.row().itemR(ao, "blend_mode", expand=True)
		
		split = layout.split()
		
		col = split.column()
		col.itemR(ao, "energy")
		
		col = split.column()
		colsub = col.split(percentage=0.3)
		colsub.itemL(text="Color:")
		colsub.itemR(ao, "color", text="")
		

bpy.types.register(WORLD_PT_context_world)	
bpy.types.register(WORLD_PT_preview)
bpy.types.register(WORLD_PT_world)
bpy.types.register(WORLD_PT_ambient_occlusion)
bpy.types.register(WORLD_PT_mist)
bpy.types.register(WORLD_PT_stars)
