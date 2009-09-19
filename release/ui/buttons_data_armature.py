
import bpy
 
class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "data"
	
	def poll(self, context):
		return (context.armature)

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
		
		layout.itemR(arm, "pose_position", expand=True)
		
		split = layout.split()

		col = split.column()
		col.itemL(text="Layers:")
		col.itemR(arm, "layer", text="")
		col.itemL(text="Protected Layers:")
		col.itemR(arm, "layer_protection", text="")
		col.itemL(text="Edit Options:")
		col.itemR(arm, "x_axis_mirror")
		col.itemR(arm, "auto_ik")
		
		col = split.column()
		col.itemL(text="Deform:")
		col.itemR(arm, "deform_vertexgroups", text="Vertex Groups")
		col.itemR(arm, "deform_envelope", text="Envelopes")
		col.itemR(arm, "deform_quaternion", text="Quaternion")
		col.itemR(arm, "deform_bbone_rest", text="B-Bones Rest")
		
class DATA_PT_display(DataButtonsPanel):
	__label__ = "Display"
	
	def draw(self, context):
		layout = self.layout
		
		arm = context.armature

		layout.row().itemR(arm, "drawtype", expand=True)

		flow = layout.column_flow()
		flow.itemR(arm, "draw_names", text="Names")
		flow.itemR(arm, "draw_axes", text="Axes")
		flow.itemR(arm, "draw_custom_bone_shapes", text="Shapes")
		flow.itemR(arm, "draw_group_colors", text="Colors")
		flow.itemR(arm, "delay_deform", text="Delay Refresh")

class DATA_PT_bone_groups(DataButtonsPanel):
	__label__ = "Bone Groups"
	
	def poll(self, context):
		return (context.object and context.object.type=='ARMATURE' and context.object.pose)

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		pose = ob.pose
		
		row = layout.row()
		row.template_list(pose, "bone_groups", pose, "active_bone_group_index")
		
		col = row.column(align=True)
		col.active = (ob.proxy == None)
		col.itemO("pose.group_add", icon='ICON_ZOOMIN', text="")
		col.itemO("pose.group_remove", icon='ICON_ZOOMOUT', text="")
		
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
		row.active = (ob.proxy == None)
		
		row.itemO("pose.group_assign", text="Assign")
		row.itemO("pose.group_remove", text="Remove") #row.itemO("pose.bone_group_remove_from", text="Remove")
		#row.itemO("object.bone_group_select", text="Select")
		#row.itemO("object.bone_group_deselect", text="Deselect")

class DATA_PT_paths(DataButtonsPanel):
	__label__ = "Paths"

	def draw(self, context):
		layout = self.layout
		
		arm = context.armature
		
		layout.itemR(arm, "paths_type", expand=True)
		
		split = layout.split()
		
		col = split.column()
		
		sub = col.column(align=True)
		if (arm.paths_type == 'CURRENT_FRAME'):
			sub.itemR(arm, "path_before_current", text="Before")
			sub.itemR(arm, "path_after_current", text="After")
		elif (arm.paths_type == 'RANGE'):
			sub.itemR(arm, "path_start_frame", text="Start")
			sub.itemR(arm, "path_end_frame", text="End")

		sub.itemR(arm, "path_size", text="Step")
		col.row().itemR(arm, "paths_location", expand=True)
		
		col = split.column()
		col.itemL(text="Display:")
		col.itemR(arm, "paths_show_frame_numbers", text="Frame Numbers")
		col.itemR(arm, "paths_highlight_keyframes", text="Keyframes")
		col.itemR(arm, "paths_show_keyframe_numbers", text="Keyframe Numbers")
		
		layout.itemS()
		
		row = layout.row()
		row.itemO("pose.paths_calculate", text="Calculate Paths")
		row.itemO("pose.paths_clear", text="Clear Paths")

class DATA_PT_ghost(DataButtonsPanel):
	__label__ = "Ghost"

	def draw(self, context):
		layout = self.layout
		
		arm = context.armature
		
		layout.itemR(arm, "ghost_type", expand=True)
		
		split = layout.split()

		col = split.column()

		sub = col.column(align=True)
		if arm.ghost_type == 'RANGE':
			sub.itemR(arm, "ghost_start_frame", text="Start")
			sub.itemR(arm, "ghost_end_frame", text="End")
			sub.itemR(arm, "ghost_size", text="Step")
		elif arm.ghost_type == 'CURRENT_FRAME':
			sub.itemR(arm, "ghost_step", text="Range")
			sub.itemR(arm, "ghost_size", text="Step")

		col = split.column()
		col.itemL(text="Display:")
		col.itemR(arm, "ghost_only_selected", text="Selected Only")

bpy.types.register(DATA_PT_context_arm)
bpy.types.register(DATA_PT_skeleton)
bpy.types.register(DATA_PT_display)
bpy.types.register(DATA_PT_bone_groups)
bpy.types.register(DATA_PT_paths)
bpy.types.register(DATA_PT_ghost)
