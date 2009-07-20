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
	__label__ = "Game Player"

	def draw(self, context):
		layout = self.layout
		gs = context.scene.game_data
		row = layout.row()
		row.itemR(gs, "fullscreen")

		split = layout.split()
		col = split.column(align=True)
		col.itemR(gs, "resolution_x", slider=False, text="Res. X")
		col.itemR(gs, "resolution_y", slider=False, text="Res. Y")

		col = split.column(align=True)
		col.itemR(gs, "depth", slider=False)
		col.itemR(gs, "frequency", slider=False)
		

		# framing:
		col = layout.column()
		col.itemL(text="Framing Options:")
		col.row().itemR(gs, "framing_type", expand=True)

		colsub = col.column()
		colsub.itemR(gs, "framing_color", text="")

class LOGIC_PT_stereo(bpy.types.Panel):
	__space_type__ = "LOGIC_EDITOR"
	__region_type__ = "UI"
	__label__ = "Stereo and Dome"

	def draw(self, context):
		layout = self.layout
		gs = context.scene.game_data


		# stereo options:
		col= layout.column()
		col.itemL(text="Stereo Options:")
		row = col.row()
		row.itemR(gs, "stereo", expand=True)
 
		stereo_mode = gs.stereo
 

		# stereo:
		if stereo_mode == 'STEREO':
			col = layout.column(align=True)
			row = col.row()
			row.item_enumR(gs, "stereo_mode", "QUADBUFFERED")
			row.item_enumR(gs, "stereo_mode", "ABOVEBELOW")
			
			row = col.row()
			row.item_enumR(gs, "stereo_mode", "INTERLACED")
			row.item_enumR(gs, "stereo_mode", "ANAGLYPH")
			
			row = col.row()
			row.item_enumR(gs, "stereo_mode", "SIDEBYSIDE")
			row.item_enumR(gs, "stereo_mode", "VINTERLACE")

#			row = layout.column_flow()
#			row.itemR(gs, "stereo_mode")

		# dome:
		if stereo_mode == 'DOME':
			row = layout.row()
			row.itemR(gs, "dome_mode")

			split=layout.split()
			col=split.column(align=True)
			col.itemR(gs, "dome_angle", slider=True)
			col.itemR(gs, "dome_tesselation", text="Tessel.")
			col=split.column(align=True)
			col.itemR(gs, "dome_tilt")
			col.itemR(gs, "dome_buffer_resolution", text="Res.", slider=True)
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
			flow.itemR(gs, "physics_gravity")
 
			split = layout.split()
			col = split.column()
			col.itemR(gs, "use_occlusion_culling", text="Enable Occlusion Culling")
	
			sub = split.column()
			sub.active = gs.use_occlusion_culling
			sub.itemR(gs, "occlusion_culling_resolution", text="resol.")

# Activity Culling is deprecated I think. Let's leave it commented by now
"""
			split = layout.split()
			col = split.column()
			col.itemR(gs, "activity_culling")
			
			sub = split.column()
			sub.active = gs.activity_culling
			sub.itemR(gs, "activity_culling_box_radius")
"""

			split = layout.split()
			col = split.column(align=True)
			col.itemR(gs, "physics_step_max", text="phys")
			col.itemR(gs, "physics_step_sub", text="sub")
			
			col = split.column(align=True)
			col.itemR(gs, "fps", text="fps")
			col.itemR(gs, "logic_step_max", text="log")

		else:
			split = layout.split()
			col = split.column(align=True)
			col.itemR(gs, "fps", text="fps")
			col = split.column(align=True)
			col.itemR(gs, "logic_step_max", text="log")

bpy.types.register(LOGIC_PT_properties)
bpy.types.register(LOGIC_PT_player)
bpy.types.register(LOGIC_PT_stereo)
bpy.types.register(LOGIC_PT_physics)