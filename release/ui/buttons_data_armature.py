
import bpy
 
class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.armature != None)

class DATA_PT_context_arm(DataButtonsPanel):
	__show_header__ = False
	
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

class DATA_PT_skeleton(DataButtonsPanel):
	__label__ = "Skeleton"
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		arm = context.armature
		space = context.space_data


		if arm:
			split = layout.split()

			col = split.column()
			col.itemR(arm, "rest_position")

			sub = col.column()
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
	__label__ = "Display"
	
	def draw(self, context):
		layout = self.layout
		arm = context.armature

		layout.row().itemR(arm, "drawtype", expand=True)

		sub = layout.column_flow()
		sub.itemR(arm, "draw_names", text="Names")
		sub.itemR(arm, "draw_axes", text="Axes")
		sub.itemR(arm, "draw_custom_bone_shapes", text="Shapes")
		sub.itemR(arm, "draw_group_colors", text="Colors")
		sub.itemR(arm, "delay_deform", text="Delay Refresh")

class DATA_PT_bone_groups(DataButtonsPanel):
	__label__ = "Bone Groups"
	
	def poll(self, context):
		return (context.object and context.object.type=='ARMATURE' and context.object.pose)

	def draw(self, context):
		layout = self.layout
		ob = context.object
		pose= ob.pose
		
		row = layout.row()
		
		row.template_list(pose, "bone_groups", pose, "active_bone_group_index")
		
		col = row.column(align=True)
		col.active = (ob.proxy == None)
		col.itemO("pose.group_add", icon="ICON_ZOOMIN", text="")
		col.itemO("pose.group_remove", icon="ICON_ZOOMOUT", text="")
		
		group = pose.active_bone_group
		if group:
			col = layout.column()
			col.active= (ob.proxy == None)
			col.itemR(group, "name")
			
			split = layout.split(0.5)
			split.active= (ob.proxy == None)
			split.itemR(group, "color_set")
			if group.color_set:
				split.template_triColorSet(group, "colors")
		
		row = layout.row(align=True)
		row.active= (ob.proxy == None)
		
		row.itemO("pose.group_assign", text="Assign")
		row.itemO("pose.group_remove", text="Remove") #row.itemO("pose.bone_group_remove_from", text="Remove")
		#row.itemO("object.bone_group_select", text="Select")
		#row.itemO("object.bone_group_deselect", text="Deselect")

class DATA_PT_paths(DataButtonsPanel):
	__label__ = "Paths"

	def draw(self, context):
		layout = self.layout
		arm = context.armature

		split = layout.split()
		
		sub = split.column()
		sub.itemR(arm, "paths_show_around_current_frame", text="Around Frame")
		col = sub.column(align=True)
		if (arm.paths_show_around_current_frame):
			col.itemR(arm, "path_before_current", text="Before")
			col.itemR(arm, "path_after_current", text="After")
		else:
			col.itemR(arm, "path_start_frame", text="Start")
			col.itemR(arm, "path_end_frame", text="End")

		col.itemR(arm, "path_size", text="Step")	
		sub.itemR(arm, "paths_calculate_head_positions", text="Head")
		
		sub = split.column()
		sub.itemL(text="Show:")
		sub.itemR(arm, "paths_show_frame_numbers", text="Frame Numbers")
		sub.itemR(arm, "paths_highlight_keyframes", text="Keyframes")
		sub.itemR(arm, "paths_show_keyframe_numbers", text="Keyframe Numbers")

class DATA_PT_ghost(DataButtonsPanel):
	__label__ = "Ghost"

	def draw(self, context):
		layout = self.layout
		arm = context.armature

		split = layout.split()

		sub = split.column()
		sub.itemR(arm, "ghost_type", text="Scope")

		col = sub.column(align=True)
		if arm.ghost_type == 'RANGE':
			col.itemR(arm, "ghost_start_frame", text="Start")
			col.itemR(arm, "ghost_end_frame", text="End")
			col.itemR(arm, "ghost_size", text="Step")
		elif arm.ghost_type == 'CURRENT_FRAME':
			col.itemR(arm, "ghost_step", text="Range")
			col.itemR(arm, "ghost_size", text="Step")

		sub = split.column()
		sub.itemR(arm, "ghost_only_selected", text="Selected Only")

bpy.types.register(DATA_PT_context_arm)
bpy.types.register(DATA_PT_skeleton)
bpy.types.register(DATA_PT_display)
bpy.types.register(DATA_PT_bone_groups)
bpy.types.register(DATA_PT_paths)
bpy.types.register(DATA_PT_ghost)
