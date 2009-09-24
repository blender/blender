
import bpy

class ConstraintButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "constraint"

	def draw_constraint(self, context, con):
		layout = self.layout
		
		box = layout.template_constraint(con)

		if box:
			# match enum type to our functions, avoids a lookup table.
			getattr(self, con.type)(context, box, con)
	
			# show/key buttons here are most likely obsolete now, with
			# keyframing functionality being part of every button
			if con.type not in ('RIGID_BODY_JOINT', 'NULL'):
				box.itemR(con, "influence")
	
	def space_template(self, layout, con, target=True, owner=True):
		if target or owner:
			row = layout.row()

			row.itemL(text="Convert:")

			if target:
				row.itemR(con, "target_space", text="")

			if target and owner:
				row.itemL(icon='ICON_ARROW_LEFTRIGHT')

			if owner:
				row.itemR(con, "owner_space", text="")
			
	def target_template(self, layout, con, subtargets=True):
		layout.itemR(con, "target") # XXX limiting settings for only 'curves' or some type of object
		
		if con.target and subtargets:
			if con.target.type == 'ARMATURE':
				layout.item_pointerR(con, "subtarget", con.target.data, "bones", text="Bone")
				
				if con.type == 'COPY_LOCATION':
					row = layout.row()
					row.itemL(text="Head/Tail:")
					row.itemR(con, "head_tail", text="")
			elif con.target.type in ('MESH', 'LATTICE'):
				layout.item_pointerR(con, "subtarget", con.target, "vertex_groups", text="Vertex Group")

	def ik_template(self, layout, con):
		layout.itemR(con, "pole_target")
	
		if con.pole_target and con.pole_target.type == 'ARMATURE':
			layout.item_pointerR(con, "pole_subtarget", con.pole_target.data, "bones", text="Bone")
		
		if con.pole_target:
			row = layout.row()
			row.itemL()
			row.itemR(con, "pole_angle")
		
		split = layout.split()
		col = split.column()
		col.itemR(con, "tail")
		col.itemR(con, "stretch")

		col = split.column()
		col.itemR(con, "iterations")
		col.itemR(con, "chain_length")
		
	
	def CHILD_OF(self, context, layout, con):
		self.target_template(layout, con)

		split = layout.split()
		
		col = split.column()
		col.itemL(text="Location:")
		col.itemR(con, "locationx", text="X")
		col.itemR(con, "locationy", text="Y")
		col.itemR(con, "locationz", text="Z")
		
		col = split.column()
		col.itemL(text="Rotation:")
		col.itemR(con, "rotationx", text="X")
		col.itemR(con, "rotationy", text="Y")
		col.itemR(con, "rotationz", text="Z")
		
		col = split.column()
		col.itemL(text="Scale:")
		col.itemR(con, "sizex", text="X")
		col.itemR(con, "sizey", text="Y")
		col.itemR(con, "sizez", text="Z")
		
		row = layout.row()
		row.itemO("constraint.childof_set_inverse")
		row.itemO("constraint.childof_clear_inverse")
		
	def TRACK_TO(self, context, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemL(text="To:")
		row.itemR(con, "track", expand=True)
		
		row = layout.row()
		#row.itemR(con, "up", text="Up", expand=True) # XXX: up and expand don't play nice together
		row.itemR(con, "up", text="Up")
		row.itemR(con, "target_z")
		
		self.space_template(layout, con)
		
	def IK(self, context, layout, con):
		if context.object.pose.ik_solver == "ITASC":
			layout.itemR(con, "ik_type")
			getattr(self, "IK_"+con.ik_type)(context, layout, con)
		else:
			self.IK_COPY_POSE(context, layout, con)

	def IK_COPY_POSE(self, context, layout, con):
		self.target_template(layout, con)
		self.ik_template(layout, con)

		split = layout.split()
		col = split.column()
		col.itemL()
		col.itemR(con, "targetless")
		col.itemR(con, "rotation")

		col = split.column()
		col.itemL(text="Weight:")
		col.itemR(con, "weight", text="Position", slider=True)
		sub = col.column()
		sub.active = con.rotation
		sub.itemR(con, "orient_weight", text="Rotation", slider=True)
		
	def IK_DISTANCE(self, context, layout, con):
		self.target_template(layout, con)
		self.ik_template(layout, con)

		layout.itemR(con, "limit_mode")
		row = layout.row()
		row.itemR(con, "weight", text="Weight", slider=True)
		row.itemR(con, "distance", text="Distance", slider=True)

	def FOLLOW_PATH(self, context, layout, con):
		self.target_template(layout, con)
		
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "use_curve_follow")
		col.itemR(con, "use_curve_radius")
		
		col = split.column()
		col.itemR(con, "use_fixed_position")
		if con.use_fixed_position:
			col.itemR(con, "offset_factor", text="Offset")
		else:
			col.itemR(con, "offset")
		
		row = layout.row()
		row.itemL(text="Forward:")
		row.itemR(con, "forward", expand=True)
		
		row = layout.row()
		row.itemR(con, "up", text="Up")
		row.itemL()
		
	def LIMIT_ROTATION(self, context, layout, con):
		
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "use_limit_x")
		sub = col.column()
		sub.active = con.use_limit_x
		sub.itemR(con, "minimum_x", text="Min")
		sub.itemR(con, "maximum_x", text="Max")
		
		col = split.column()
		col.itemR(con, "use_limit_y")
		sub = col.column()
		sub.active = con.use_limit_y
		sub.itemR(con, "minimum_y", text="Min")
		sub.itemR(con, "maximum_y", text="Max")
		
		col = split.column()
		col.itemR(con, "use_limit_z")
		sub = col.column()
		sub.active = con.use_limit_z
		sub.itemR(con, "minimum_z", text="Min")
		sub.itemR(con, "maximum_z", text="Max")
		
		row = layout.row()
		row.itemR(con, "limit_transform")
		row.itemL()
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
		
	def LIMIT_LOCATION(self, context, layout, con):
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "use_minimum_x")
		sub = col.column()
		sub.active = con.use_minimum_x
		sub.itemR(con, "minimum_x", text="")
		col.itemR(con, "use_maximum_x")
		sub = col.column()
		sub.active = con.use_maximum_x
		sub.itemR(con, "maximum_x", text="")
		
		col = split.column()
		col.itemR(con, "use_minimum_y")
		sub = col.column()
		sub.active = con.use_minimum_y
		sub.itemR(con, "minimum_y", text="")
		col.itemR(con, "use_maximum_y")
		sub = col.column()
		sub.active = con.use_maximum_y
		sub.itemR(con, "maximum_y", text="")
		
		col = split.column()
		col.itemR(con, "use_minimum_z")
		sub = col.column()
		sub.active = con.use_minimum_z
		sub.itemR(con, "minimum_z", text="")
		col.itemR(con, "use_maximum_z")
		sub = col.column()
		sub.active = con.use_maximum_z
		sub.itemR(con, "maximum_z", text="")
	
		row = layout.row()
		row.itemR(con, "limit_transform")
		row.itemL()
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
		
	def LIMIT_SCALE(self, context, layout, con):
		split = layout.split()

		col = split.column()
		col.itemR(con, "use_minimum_x")
		sub = col.column()
		sub.active = con.use_minimum_x
		sub.itemR(con, "minimum_x", text="")
		col.itemR(con, "use_maximum_x")
		sub = col.column()
		sub.active = con.use_maximum_x
		sub.itemR(con, "maximum_x", text="")
		
		col = split.column()
		col.itemR(con, "use_minimum_y")
		sub = col.column()
		sub.active = con.use_minimum_y
		sub.itemR(con, "minimum_y", text="")
		col.itemR(con, "use_maximum_y")
		sub = col.column()
		sub.active = con.use_maximum_y
		sub.itemR(con, "maximum_y", text="")
		
		col = split.column()
		col.itemR(con, "use_minimum_z")
		sub = col.column()
		sub.active = con.use_minimum_z
		sub.itemR(con, "minimum_z", text="")
		col.itemR(con, "use_maximum_z")
		sub = col.column()
		sub.active = con.use_maximum_z
		sub.itemR(con, "maximum_z", text="")
		
		row = layout.row()
		row.itemR(con, "limit_transform")
		row.itemL()
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
	
	def COPY_ROTATION(self, context, layout, con):
		self.target_template(layout, con)
		
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "rotate_like_x", text="X")
		sub = col.column()
		sub.active = con.rotate_like_x
		sub.itemR(con, "invert_x", text="Invert")
		
		col = split.column()
		col.itemR(con, "rotate_like_y", text="Y")
		sub = col.column()
		sub.active = con.rotate_like_y
		sub.itemR(con, "invert_y", text="Invert")
		
		col = split.column()
		col.itemR(con, "rotate_like_z", text="Z")
		sub = col.column()
		sub.active = con.rotate_like_z
		sub.itemR(con, "invert_z", text="Invert")

		layout.itemR(con, "offset")
		
		self.space_template(layout, con)
		
	def COPY_LOCATION(self, context, layout, con):
		self.target_template(layout, con)
		
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "locate_like_x", text="X")
		sub = col.column()
		sub.active = con.locate_like_x
		sub.itemR(con, "invert_x", text="Invert")
		
		col = split.column()
		col.itemR(con, "locate_like_y", text="Y")
		sub = col.column()
		sub.active = con.locate_like_y
		sub.itemR(con, "invert_y", text="Invert")
		
		col = split.column()
		col.itemR(con, "locate_like_z", text="Z")
		sub = col.column()
		sub.active = con.locate_like_z
		sub.itemR(con, "invert_z", text="Invert")

		layout.itemR(con, "offset")
			
		self.space_template(layout, con)
		
	def COPY_SCALE(self, context, layout, con):
		self.target_template(layout, con)
		
		row = layout.row(align=True)
		row.itemR(con, "size_like_x", text="X")
		row.itemR(con, "size_like_y", text="Y")
		row.itemR(con, "size_like_z", text="Z")

		layout.itemR(con, "offset")
		
		self.space_template(layout, con)
		
	#def SCRIPT(self, context, layout, con):
	
	def ACTION(self, context, layout, con):
		self.target_template(layout, con)
		
		layout.itemR(con, "action")
		layout.itemR(con, "transform_channel")

		split = layout.split()
	
		col = split.column(align=True)
		col.itemR(con, "start_frame", text="Start")
		col.itemR(con, "end_frame", text="End")
		
		col = split.column(align=True)
		col.itemR(con, "minimum", text="Min")
		col.itemR(con, "maximum", text="Max")
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
	
	def LOCKED_TRACK(self, context, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemL(text="To:")
		row.itemR(con, "track", expand=True)
		
		row = layout.row()
		row.itemL(text="Lock:")
		row.itemR(con, "locked", expand=True)
		
	def LIMIT_DISTANCE(self, context, layout, con):
		self.target_template(layout, con)
		
		col = layout.column(align=True);
		col.itemR(con, "distance")
		col.itemO("constraint.limitdistance_reset")
		
		row = layout.row()
		row.itemL(text="Clamp Region:")
		row.itemR(con, "limit_mode", text="")
		
	def STRETCH_TO(self, context, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemR(con, "original_length", text="Rest Length")
		row.itemO("constraint.stretchto_reset", text="Reset")
		
		col = layout.column()
		col.itemR(con, "bulge", text="Volume Variation")
		
		row = layout.row()
		row.itemL(text="Volume:")
		row.itemR(con, "volume", expand=True)
		row.itemL(text="Plane:")
		row.itemR(con, "keep_axis", expand=True)
		
	def FLOOR(self, context, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemR(con, "sticky")
		row.itemR(con, "use_rotation")
		
		layout.itemR(con, "offset")
		
		row = layout.row()
		row.itemL(text="Min/Max:")
		row.itemR(con, "floor_location", expand=True)
		
	def RIGID_BODY_JOINT(self, context, layout, con):
		self.target_template(layout, con)
		
		layout.itemR(con, "pivot_type")
		layout.itemR(con, "child")
		
		row = layout.row()
		row.itemR(con, "disable_linked_collision", text="No Collision")
		row.itemR(con, "draw_pivot", text="Display Pivot")
		
		split = layout.split()
		
		col = split.column(align=True)
		col.itemL(text="Pivot:")
		col.itemR(con, "pivot_x", text="X")
		col.itemR(con, "pivot_y", text="Y")
		col.itemR(con, "pivot_z", text="Z")
		
		col = split.column(align=True)
		col.itemL(text="Axis:")
		col.itemR(con, "axis_x", text="X")
		col.itemR(con, "axis_y", text="Y")
		col.itemR(con, "axis_z", text="Z")
		
		#Missing: Limit arrays (not wrapped in RNA yet) 
	
	def CLAMP_TO(self, context, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemL(text="Main Axis:")
		row.itemR(con, "main_axis", expand=True)
		
		row = layout.row()
		row.itemR(con, "cyclic")
		
	def TRANSFORM(self, context, layout, con):
		self.target_template(layout, con)
		
		layout.itemR(con, "extrapolate_motion", text="Extrapolate")
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Source:")
		col.row().itemR(con, "map_from", expand=True)
		
		sub = col.row(align=True)
		sub.itemL(text="X:")
		sub.itemR(con, "from_min_x", text="")
		sub.itemR(con, "from_max_x", text="")
		
		sub = col.row(align=True)
		sub.itemL(text="Y:")
		sub.itemR(con, "from_min_y", text="")
		sub.itemR(con, "from_max_y", text="")
		
		sub = col.row(align=True)
		sub.itemL(text="Z:")
		sub.itemR(con, "from_min_z", text="")
		sub.itemR(con, "from_max_z", text="")
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Destination:")
		col.row().itemR(con, "map_to", expand=True)

		sub = col.row(align=True)
		sub.itemR(con, "map_to_x_from", text="")
		sub.itemR(con, "to_min_x", text="")
		sub.itemR(con, "to_max_x", text="")
		
		sub = col.row(align=True)
		sub.itemR(con, "map_to_y_from", text="")
		sub.itemR(con, "to_min_y", text="")
		sub.itemR(con, "to_max_y", text="")
		
		sub = col.row(align=True)
		sub.itemR(con, "map_to_z_from", text="")
		sub.itemR(con, "to_min_z", text="")
		sub.itemR(con, "to_max_z", text="")
		
		self.space_template(layout, con)
		
	def SHRINKWRAP (self, context, layout, con):
		self.target_template(layout, con)
		
		layout.itemR(con, "distance")
		layout.itemR(con, "shrinkwrap_type")
		
		if con.shrinkwrap_type == 'PROJECT':
			row = layout.row(align=True)
			row.itemR(con, "axis_x")
			row.itemR(con, "axis_y")
			row.itemR(con, "axis_z")
		
class OBJECT_PT_constraints(ConstraintButtonsPanel):
	__label__ = "Constraints"
	__context__ = "constraint"

	def poll(self, context):
		return (context.object)
		
	def draw(self, context):
		layout = self.layout
		ob = context.object

		row = layout.row()
		row.item_menu_enumO("object.constraint_add", "type")
		row.itemL();

		for con in ob.constraints:
			self.draw_constraint(context, con)

class BONE_PT_constraints(ConstraintButtonsPanel):
	__label__ = "Constraints"
	__context__ = "bone_constraint"

	def poll(self, context):
		ob = context.object
		return (ob and ob.type == 'ARMATURE' and context.bone)
		
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		pchan = ob.pose.pose_channels[context.bone.name]

		row = layout.row()
		row.item_menu_enumO("pose.constraint_add", "type")
		row.itemL();

		for con in pchan.constraints:
			self.draw_constraint(context, con)

bpy.types.register(OBJECT_PT_constraints)
bpy.types.register(BONE_PT_constraints)
