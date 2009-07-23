
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

		layout.itemR(game, "physics_type")
		layout.itemS()
		
		split = layout.split()
		col = split.column()
		
		col.itemR(game, "actor")
		
		col.itemR(game, "ghost")
		col.itemR(ob, "restrict_render", text="Invisible") # out of place but useful
		col = split.column()
		col.itemR(game, "do_fh", text="Use Material Physics")
		col.itemR(game, "rotation_fh", text="Rotate From Normal")
		col.itemR(game, "no_sleeping")
		
		layout.itemS()
		split = layout.split()
		col = split.column()
		col.itemL(text="Attributes:")
		colsub = col.column(align=True)
		colsub.itemR(game, "mass")
		colsub.itemR(game, "radius")
		colsub.itemR(game, "form_factor")
		col.itemS()
		col.itemL(text="Damping:")
		colsub = col.column(align=True)
		colsub.itemR(game, "damping", text="Translation", slider=True)
		colsub.itemR(game, "rotation_damping", text="Rotation", slider=True)
		
		col = split.column()
		
		col.itemL(text="Velocity:")
		colsub = col.column(align=True)
		colsub.itemR(game, "minimum_velocity", text="Minimum")
		colsub.itemR(game, "maximum_velocity", text="Maximum")
		col.itemS()
		col.itemR(game, "anisotropic_friction")
		
		colsub = col.column()
		colsub.active = game.anisotropic_friction
		colsub.itemR(game, "friction_coefficients", text="", slider=True)
		
		layout.itemS()
		split = layout.split()
		sub = split.column()
		sub.itemL(text="Lock Translation:")
		sub.itemR(game, "lock_x_axis", text="X")
		sub.itemR(game, "lock_y_axis", text="Y")
		sub.itemR(game, "lock_z_axis", text="Z")
		sub = split.column()
		sub.itemL(text="Lock Rotation:")
		sub.itemR(game, "lock_x_rot_axis", text="X")
		sub.itemR(game, "lock_y_rot_axis", text="Y")
		sub.itemR(game, "lock_z_rot_axis", text="Z")


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
		layout.active = game.use_collision_bounds
		
		
		
		layout.itemR(game, "collision_bounds", text="Bounds")
		
		split = layout.split()
		sub = split.column()
		sub.itemR(game, "collision_compound", text="Compound")
		sub = split.column()
		sub.itemR(game, "collision_margin", text="Margin", slider=True)


bpy.types.register(GAME_PT_context_game)
bpy.types.register(GAME_PT_data)
bpy.types.register(GAME_PT_physics)
bpy.types.register(GAME_PT_collision_bounds)