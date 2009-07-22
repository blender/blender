import bpy

class LOGIC_PT_properties(bpy.types.Panel):
	__space_type__ = "LOGIC_EDITOR"
	__region_type__ = "UI"
	__label__ = "Properties"

	def poll(self, context):
		ob = context.active_object
		return ob and ob.game

	def draw(self, context):
		layout = self.layout
		ob = context.active_object
		game = ob.game
		
		for prop in game.properties:
			flow = layout.row()
			flow.itemR(prop, "name", text="")
			flow.itemR(prop, "type", text="")
			flow.itemR(prop, "value", text="") # we dont care about the type. rna will display correctly
			flow.itemR(prop, "debug")

"""
class WORLD_PT_game(WorldButtonsPanel):
	__space_type__ = "LOGIC_EDITOR"
	__region_type__ = "UI"
	__label__ = "Game Settings"

	def draw(self, context):
		layout = self.layout
		world = context.world
		
		flow = layout.column_flow()
		flow.itemR(world, "physics_engine")
		flow.itemR(world, "physics_gravity")
		
		flow.itemR(world, "game_fps")
		flow.itemR(world, "game_logic_step_max")
		flow.itemR(world, "game_physics_substep")
		flow.itemR(world, "game_physics_step_max")
		
		flow.itemR(world, "game_use_occlusion_culling", text="Enable Occlusion Culling")
		flow.itemR(world, "game_occlusion_culling_resolution")
"""
class LOGIC_PT_player(bpy.types.Panel):
	__space_type__ = "LOGIC_EDITOR"
	__region_type__ = "UI"
	__label__ = "Player"

	def draw(self, context):
		layout = self.layout
		gs = context.scene.game_data
		row = layout.row()
		row.itemR(gs, "fullscreen")

		split = layout.split()
		col = split.column()
		col.itemL(text="Resolution:")
		colsub = col.column(align=True)
		colsub.itemR(gs, "resolution_x", slider=False, text="X")
		colsub.itemR(gs, "resolution_y", slider=False, text="Y")

		col = split.column()
		col.itemL(text="Quality:")
		colsub = col.column(align=True)
		colsub.itemR(gs, "depth", text="Bit Depth", slider=False)
		colsub.itemR(gs, "frequency", text="FPS", slider=False)
		

		# framing:
		col = layout.column()
		col.itemL(text="Framing:")
		col.row().itemR(gs, "framing_type", expand=True)

		colsub = col.column()
		colsub.itemR(gs, "framing_color", text="")

class LOGIC_PT_stereo(bpy.types.Panel):
	__space_type__ = "LOGIC_EDITOR"
	__region_type__ = "UI"
	__label__ = "Stereo"

	def draw(self, context):
		layout = self.layout
		gs = context.scene.game_data


		# stereo options:
		col= layout.column()
		row = col.row()
		row.itemR(gs, "stereo", expand=True)
 
		stereo_mode = gs.stereo
 

		# stereo:
		if stereo_mode == 'STEREO':

			row = layout.row()
			row.itemR(gs, "stereo_mode")

		# dome:
		if stereo_mode == 'DOME':
			row = layout.row()
			row.itemR(gs, "dome_mode", text="Dome Type")

			split=layout.split()
			col=split.column()
			col.itemR(gs, "dome_angle", slider=True)
			col.itemR(gs, "dome_tesselation", text="Tesselation")
			col=split.column()
			col.itemR(gs, "dome_tilt")
			col.itemR(gs, "dome_buffer_resolution", text="Resolution", slider=True)
			col=layout.column()
			col.itemR(gs, "dome_text")


class LOGIC_PT_physics(bpy.types.Panel):
	__space_type__ = "LOGIC_EDITOR"
	__region_type__ = "UI"
	__label__ = "World Physics"
 
	def draw(self, context):
		layout = self.layout
		gs = context.scene.game_data
		flow = layout.column_flow()
		flow.itemR(gs, "physics_engine")
		if gs.physics_engine != "NONE":
			flow.itemR(gs, "physics_gravity", text="Gravity")
 
			split = layout.split()
			col = split.column()
			col.itemL(text="Physics Steps:")
			colsub = col.column(align=True)
			colsub.itemR(gs, "physics_step_max", text="Max")
			colsub.itemR(gs, "physics_step_sub", text="Substeps")
			col.itemR(gs, "fps", text="FPS")
			
			col = split.column()
			col.itemL(text="Logic Steps:")
			col.itemR(gs, "logic_step_max", text="Max")
			col.itemS()
			col.itemR(gs, "use_occlusion_culling", text="Occlusion Culling")
			colsub = col.column()
			colsub.active = gs.use_occlusion_culling
			colsub.itemR(gs, "occlusion_culling_resolution", text="Resolution")
			

		else:
			split = layout.split()
			col = split.column()
			col.itemL(text="Physics Steps:")
			col.itemR(gs, "fps", text="FPS")
			col = split.column()
			col.itemL(text="Logic Steps:")
			col.itemR(gs, "logic_step_max", text="Max")

bpy.types.register(LOGIC_PT_properties)
bpy.types.register(LOGIC_PT_player)
bpy.types.register(LOGIC_PT_stereo)
bpy.types.register(LOGIC_PT_physics)