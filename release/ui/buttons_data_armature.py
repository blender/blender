
import bpy
 
class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.armature != None)

class DATA_PT_skeleton(DataButtonsPanel):
	__idname__ = "DATA_PT_skeleton"
	__label__ = "Skeleton"
	
	def poll(self, context):
		return (context.object.type == 'ARMATURE' or context.armature)

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		arm = context.armature
		space = context.space_data

		split = layout.split(percentage=0.65)

		if ob:
			split.template_ID(ob, "data")
			split.itemS()
		elif arm:
			split.template_ID(space, "pin_id")
			split.itemS()

		if arm:
			layout.itemS()
			layout.itemR(arm, "rest_position")

			split = layout.split()

			sub = split.column()
			sub.itemL(text="Deform:")
			sub.itemR(arm, "deform_vertexgroups", text="Vertes Groups")
			sub.itemR(arm, "deform_envelope", text="Envelopes")
			sub.itemR(arm, "deform_quaternion", text="Quaternion")
			sub.itemR(arm, "deform_bbone_rest", text="B-Bones Rest")
			#sub.itemR(arm, "x_axis_mirror")
			#sub.itemR(arm, "auto_ik")
			
			sub = split.column()
			sub.itemL(text="Layers:")
			sub.template_layers(arm, "layer")
			sub.itemL(text="Protected Layers:")
			sub.template_layers(arm, "layer_protection")

class DATA_PT_display(DataButtonsPanel):
	__idname__ = "DATA_PT_display"
	__label__ = "Display"
	
	def draw(self, context):
		layout = self.layout
		arm = context.armature

		split = layout.split()

		sub = split.column()
		sub.itemR(arm, "drawtype", text="Style")
		sub.itemR(arm, "delay_deform", text="Delay Refresh")

		sub = split.column()
		sub.itemR(arm, "draw_names", text="Names")
		sub.itemR(arm, "draw_axes", text="Axes")
		sub.itemR(arm, "draw_custom_bone_shapes", text="Shapes")
		sub.itemR(arm, "draw_group_colors", text="Colors")

class DATA_PT_paths(DataButtonsPanel):
	__idname__ = "DATA_PT_paths"
	__label__ = "Paths"

	def draw(self, context):
		layout = self.layout
		arm = context.armature

		split = layout.split()
		
		sub = split.column()
		sub.itemR(arm, "paths_show_around_current_frame", text="Around Frame")
		if (arm.paths_show_around_current_frame):
			sub.itemR(arm, "path_before_current", text="Before")
			sub.itemR(arm, "path_after_current", text="After")
		else:
			sub.itemR(arm, "path_start_frame", text="Start")
			sub.itemR(arm, "path_end_frame", text="End")

		sub.itemR(arm, "path_size", text="Step")	
		sub.itemR(arm, "paths_calculate_head_positions", text="Head")
		
		sub = split.column()
		sub.itemL(text="Show:")
		sub.itemR(arm, "paths_show_frame_numbers", text="Frame Numbers")
		sub.itemR(arm, "paths_highlight_keyframes", text="Keyframes")
		sub.itemR(arm, "paths_show_keyframe_numbers", text="Keyframe Numbers")

class DATA_PT_ghost(DataButtonsPanel):
	__idname__ = "DATA_PT_ghost"
	__label__ = "Ghost"

	def draw(self, context):
		layout = self.layout
		arm = context.armature

		split = layout.split()

		sub = split.column()
		sub.itemR(arm, "ghost_type", text="Scope")
		if arm.ghost_type == 'RANGE':
			sub.itemR(arm, "ghost_start_frame", text="Start")
			sub.itemR(arm, "ghost_end_frame", text="End")
			sub.itemR(arm, "ghost_size", text="Step")
		elif arm.ghost_type == 'CURRENT_FRAME':
			sub.itemR(arm, "ghost_step", text="Range")
			sub.itemR(arm, "ghost_size", text="Step")

		sub = split.column()
		sub.itemR(arm, "ghost_only_selected", text="Selected Only")

bpy.types.register(DATA_PT_skeleton)
bpy.types.register(DATA_PT_display)
bpy.types.register(DATA_PT_paths)
bpy.types.register(DATA_PT_ghost)