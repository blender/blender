
import bpy
 
class GameButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "game"

class GAME_PT_context_game(GameButtonsPanel):
	__idname__ = "GAME_PT_context_game"
	__no_header__ = True

	def draw(self, context):
		layout = self.layout
		ob = context.object
		game = context.game

		split = layout.split(percentage=0.06)
		split.itemL(text="", icon="ICON_GAME")
		split.itemR(game, "name", text="")

class GAME_PT_data(GameButtonsPanel):
	__idname__ = "GAME_PT_data"
	__label__ = "Data"

	def draw(self, context):
		layout = self.layout
		ob = context.object
		game = context.game

class GAME_PT_physics(GameButtonsPanel):
	__idname__ = "GAME_PT_physics"
	__label__ = "Physics"

	def poll(self, context):
		ob = context.active_object
		return ob and ob.game

	def draw(self, context):
		layout = self.layout
		ob = context.active_object
		
		game = ob.game

		flow = layout.column_flow()
		flow.active = True
		flow.itemR(game, "physics_type")
		flow.itemR(game, "actor")
		
		row = layout.row()
		row.itemR(game, "ghost")
		row.itemR(ob, "restrict_render", text="Invisible") # out of place but useful
		
		flow = layout.column_flow()
		flow.itemR(game, "mass")
		flow.itemR(game, "radius")
		flow.itemR(game, "no_sleeping")
		flow.itemR(game, "damping")
		flow.itemR(game, "rotation_damping")
		flow.itemR(game, "minimum_velocity")
		flow.itemR(game, "maximum_velocity")
		
		row = layout.row()
		row.itemR(game, "do_fh")
		row.itemR(game, "rotation_fh")
		
		flow = layout.column_flow()
		flow.itemR(game, "form_factor")
		flow.itemR(game, "anisotropic_friction")
		
		flow = layout.column_flow()
		flow.active = game.anisotropic_friction
		flow.itemR(game, "friction_coefficients")
		
		split = layout.split()
		sub = split.column()
		sub.itemR(game, "lock_x_axis")
		sub.itemR(game, "lock_y_axis")
		sub.itemR(game, "lock_z_axis")
		sub = split.column()
		sub.itemR(game, "lock_x_rot_axis")
		sub.itemR(game, "lock_y_rot_axis")
		sub.itemR(game, "lock_z_rot_axis")


class GAME_PT_collision_bounds(GameButtonsPanel):
	__idname__ = "GAME_PT_collision_bounds"
	__label__ = "Collision Bounds"

	def poll(self, context):
		ob = context.active_object
		return ob and ob.game
	
	def draw_header(self, context):
		layout = self.layout
		ob = context.active_object
		game = ob.game

		layout.itemR(game, "use_collision_bounds", text="")
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.scene.objects[0]
		game = ob.game
		
		flow = layout.column_flow()
		flow.active = game.use_collision_bounds
		flow.itemR(game, "collision_bounds")
		flow.itemR(game, "collision_compound")
		flow.itemR(game, "collision_margin")



bpy.types.register(GAME_PT_context_game)
bpy.types.register(GAME_PT_data)
bpy.types.register(GAME_PT_physics)
bpy.types.register(GAME_PT_collision_bounds)