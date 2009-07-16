
import bpy

class ConstraintButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "constraint"

	def draw_constraint(self, con):
		layout = self.layout
		box = layout.template_constraint(con)

		if box:
			if con.type == "CHILD_OF":
				self.child_of(box, con)
			elif con.type == "TRACK_TO":
				self.track_to(box, con)
			elif con.type == "IK":
				self.ik(box, con)
			elif con.type == "FOLLOW_PATH":
				self.follow_path(box, con)
			elif con.type == "LIMIT_ROTATION":
				self.limit_rotation(box, con)
			elif con.type == "LIMIT_LOCATION":
				self.limit_location(box, con)
			elif con.type == "LIMIT_SCALE":
				self.limit_scale(box, con)
			elif con.type == "COPY_ROTATION":
				self.copy_rotation(box, con)
			elif con.type == "COPY_LOCATION":
				self.copy_location(box, con)
			elif con.type == "COPY_SCALE":
				self.copy_scale(box, con)
			#elif con.type == "SCRIPT":
			#	self.script(box, con)
			elif con.type == "ACTION":
				self.action(box, con)
			elif con.type == "LOCKED_TRACK":
				self.locked_track(box, con)
			elif con.type == "LIMIT_DISTANCE":
				self.limit_distance(box, con)
			elif con.type == "STRETCH_TO":
				self.stretch_to(box, con)
			elif con.type == "FLOOR":
				self.floor(box, con)
			elif con.type == "RIGID_BODY_JOINT":
				self.rigid_body(box, con)
			elif con.type == "CLAMP_TO":
				self.clamp_to(box, con)
			elif con.type == "TRANSFORM":
				self.transform(box, con)
			elif con.type == "SHRINKWRAP":
				self.shrinkwrap(box, con)
				
			# show/key buttons here are most likely obsolete now, with
			# keyframing functionality being part of every button
			if con.type not in ("RIGID_BODY_JOINT", "NULL"):
				box.itemR(con, "influence")
	
	def space_template(self, layout, con, target=True, owner=True):
		if target or owner:
			row = layout.row()

			row.itemL(text="Convert:")

			if target:
				row.itemR(con, "target_space", text="")

			if target and owner:
				row.itemL(icon="ICON_ARROW_LEFTRIGHT")

			if owner:
				row.itemR(con, "owner_space", text="")
			
	def target_template(self, layout, con, subtargets=True):
		layout.itemR(con, "target") # XXX limiting settings for only 'curves' or some type of object
		
		if con.target and subtargets:
			if con.target.type == "ARMATURE":
				layout.item_pointerR(con, "subtarget", con.target.data, "bones", text="Bone")
				
				if con.type == 'COPY_LOCATION':
					row = layout.row()
					row.itemL(text="Head/Tail:")
					row.itemR(con, "head_tail", text="")
			elif con.target.type in ("MESH", "LATTICE"):
				layout.item_pointerR(con, "subtarget", con.target, "vertex_groups", text="Vertex Group")
	
	def child_of(self, layout, con):
		self.target_template(layout, con)

		split = layout.split()
		
		sub = split.column()
		sub.itemL(text="Location:")
		sub.itemR(con, "locationx", text="X")
		sub.itemR(con, "locationy", text="Y")
		sub.itemR(con, "locationz", text="Z")
		
		sub = split.column()
		sub.itemL(text="Rotation:")
		sub.itemR(con, "rotationx", text="X")
		sub.itemR(con, "rotationy", text="Y")
		sub.itemR(con, "rotationz", text="Z")
		
		sub = split.column()
		sub.itemL(text="Scale:")
		sub.itemR(con, "sizex", text="X")
		sub.itemR(con, "sizey", text="Y")
		sub.itemR(con, "sizez", text="Z")
		
		row = layout.row()
		row.itemO("CONSTRAINT_OT_childof_set_inverse")
		row.itemO("CONSTRAINT_OT_childof_clear_inverse")
		
	def track_to(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemL(text="To:")
		row.itemR(con, "track", expand=True)
		
		row = layout.row()
		#row.itemR(con, "up", text="Up", expand=True) # XXX: up and expand don't play nice together
		row.itemR(con, "up", text="Up")
		row.itemR(con, "target_z")
		
		self.space_template(layout, con)
		
	def ik(self, layout, con):
		self.target_template(layout, con)
		
		layout.itemR(con, "pole_target")
		if con.pole_target and con.pole_target.type == "ARMATURE":
			layout.item_pointerR(con, "pole_subtarget", con.pole_target.data, "bones", text="Bone")
		
		col = layout.column_flow()
		col.itemR(con, "iterations")
		col.itemR(con, "pole_angle")
		col.itemR(con, "weight")
		col.itemR(con, "orient_weight")
		col.itemR(con, "chain_length")
		
		col = layout.column_flow()
		col.itemR(con, "tail")
		col.itemR(con, "rotation")
		col.itemR(con, "targetless")
		col.itemR(con, "stretch")
		
	def follow_path(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemR(con, "curve_follow")
		row.itemR(con, "offset")
		
		row = layout.row()
		row.itemL(text="Forward:")
		row.itemR(con, "forward", expand=True)
		
		row = layout.row()
		row.itemR(con, "up", text="Up")
		row.itemL()
		
	def limit_rotation(self, layout, con):
		
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "use_limit_x")
		colsub = col.column()
		colsub.active = con.use_limit_x
		colsub.itemR(con, "minimum_x", text="Min")
		colsub.itemR(con, "maximum_x", text="Max")
		
		col = split.column()
		col.itemR(con, "use_limit_y")
		colsub = col.column()
		colsub.active = con.use_limit_y
		colsub.itemR(con, "minimum_y", text="Min")
		colsub.itemR(con, "maximum_y", text="Max")
		
		col = split.column()
		col.itemR(con, "use_limit_z")
		colsub = col.column()
		colsub.active = con.use_limit_z
		colsub.itemR(con, "minimum_z", text="Min")
		colsub.itemR(con, "maximum_z", text="Max")
		
		row = layout.row()
		row.itemR(con, "limit_transform")
		row.itemL()
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
		
	def limit_location(self, layout, con):
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "use_minimum_x")
		colsub = col.column()
		colsub.active = con.use_minimum_x
		colsub.itemR(con, "minimum_x", text="")
		col.itemR(con, "use_maximum_x")
		colsub = col.column()
		colsub.active = con.use_maximum_x
		colsub.itemR(con, "maximum_x", text="")
		
		col = split.column()
		col.itemR(con, "use_minimum_y")
		colsub = col.column()
		colsub.active = con.use_minimum_y
		colsub.itemR(con, "minimum_y", text="")
		col.itemR(con, "use_maximum_y")
		colsub = col.column()
		colsub.active = con.use_maximum_y
		colsub.itemR(con, "maximum_y", text="")
		
		col = split.column()
		col.itemR(con, "use_minimum_z")
		colsub = col.column()
		colsub.active = con.use_minimum_z
		colsub.itemR(con, "minimum_z", text="")
		col.itemR(con, "use_maximum_z")
		colsub = col.column()
		colsub.active = con.use_maximum_z
		colsub.itemR(con, "maximum_z", text="")
	
		row = layout.row()
		row.itemR(con, "limit_transform")
		row.itemL()
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
		
	def limit_scale(self, layout, con):
		split = layout.split()

		col = split.column()
		col.itemR(con, "use_minimum_x")
		colsub = col.column()
		colsub.active = con.use_minimum_x
		colsub.itemR(con, "minimum_x", text="")
		col.itemR(con, "use_maximum_x")
		colsub = col.column()
		colsub.active = con.use_maximum_x
		colsub.itemR(con, "maximum_x", text="")
		
		col = split.column()
		col.itemR(con, "use_minimum_y")
		colsub = col.column()
		colsub.active = con.use_minimum_y
		colsub.itemR(con, "minimum_y", text="")
		col.itemR(con, "use_maximum_y")
		colsub = col.column()
		colsub.active = con.use_maximum_y
		colsub.itemR(con, "maximum_y", text="")
		
		col = split.column()
		col.itemR(con, "use_minimum_z")
		colsub = col.column()
		colsub.active = con.use_minimum_z
		colsub.itemR(con, "minimum_z", text="")
		col.itemR(con, "use_maximum_z")
		colsub = col.column()
		colsub.active = con.use_maximum_z
		colsub.itemR(con, "maximum_z", text="")
		
		row = layout.row()
		row.itemR(con, "limit_transform")
		row.itemL()
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
	
	def copy_rotation(self, layout, con):
		self.target_template(layout, con)
		
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "rotate_like_x", text="X")
		colsub = col.column()
		colsub.active = con.rotate_like_x
		colsub.itemR(con, "invert_x", text="Invert")
		
		col = split.column()
		col.itemR(con, "rotate_like_y", text="Y")
		colsub = col.column()
		colsub.active = con.rotate_like_y
		colsub.itemR(con, "invert_y", text="Invert")
		
		col = split.column()
		col.itemR(con, "rotate_like_z", text="Z")
		colsub = col.column()
		colsub.active = con.rotate_like_z
		colsub.itemR(con, "invert_z", text="Invert")

		layout.itemR(con, "offset")
		
		self.space_template(layout, con)
		
	def copy_location(self, layout, con):
		self.target_template(layout, con)
		
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "locate_like_x", text="X")
		colsub = col.column()
		colsub.active = con.locate_like_x
		colsub.itemR(con, "invert_x", text="Invert")
		
		col = split.column()
		col.itemR(con, "locate_like_y", text="Y")
		colsub = col.column()
		colsub.active = con.locate_like_y
		colsub.itemR(con, "invert_y", text="Invert")
		
		col = split.column()
		col.itemR(con, "locate_like_z", text="Z")
		colsub = col.column()
		colsub.active = con.locate_like_z
		colsub.itemR(con, "invert_z", text="Invert")

		layout.itemR(con, "offset")
			
		self.space_template(layout, con)
		
	def copy_scale(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row(align=True)
		row.itemR(con, "size_like_x", text="X")
		row.itemR(con, "size_like_y", text="Y")
		row.itemR(con, "size_like_z", text="Z")

		layout.itemR(con, "offset")
		
		self.space_template(layout, con)
		
	#def script(self, layout, con):
	
	def action(self, layout, con):
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
	
	def locked_track(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemL(text="To:")
		row.itemR(con, "track", expand=True)
		
		row = layout.row()
		row.itemL(text="Lock:")
		row.itemR(con, "locked", expand=True)
		
	def limit_distance(self, layout, con):
		self.target_template(layout, con)
		
		layout.itemR(con, "distance")
		
		row = layout.row()
		row.itemL(text="Clamp Region:")
		row.itemR(con, "limit_mode", text="")
		#Missing: Recalculate Button
		
	def stretch_to(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemR(con, "original_length", text="Rest Length")
		row.itemR(con, "bulge", text="Volume Variation")
		
		row = layout.row()
		row.itemL(text="Volume:")
		row.itemR(con, "volume", expand=True)
		row.itemL(text="Plane:")
		row.itemR(con, "keep_axis", expand=True)
		#Missing: Recalculate Button
		
	def floor(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemR(con, "sticky")
		row.itemR(con, "use_rotation")
		
		layout.itemR(con, "offset")
		
		row = layout.row()
		row.itemL(text="Min/Max:")
		row.itemR(con, "floor_location", expand=True)
		
	def rigid_body(self, layout, con):
		self.target_template(layout, con)
		
		layout.itemR(con, "pivot_type")
		layout.itemR(con, "child")
		
		row = layout.row()
		row.itemR(con, "disable_linked_collision", text="No Collision")
		row.itemR(con, "draw_pivot")
		
		split = layout.split()
		
		col = split.column()
		col.itemR(con, "pivot_x")
		col.itemR(con, "pivot_y")
		col.itemR(con, "pivot_z")
		
		col = split.column()
		col.itemR(con, "axis_x")
		col.itemR(con, "axis_y")
		col.itemR(con, "axis_z")
		
		#Missing: Limit arrays (not wrapped in RNA yet) 
	
	def clamp_to(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemL(text="Main Axis:")
		row.itemR(con, "main_axis", expand=True)
		
		row = layout.row()
		row.itemR(con, "cyclic")
		
	def transform(self, layout, con):
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
		
	def shrinkwrap (self, layout, con):
		self.target_template(layout, con)
		
		layout.itemR(con, "distance")
		layout.itemR(con, "shrinkwrap_type")
		
		if con.shrinkwrap_type == "PROJECT":
			row = layout.row(align=True)
			row.itemR(con, "axis_x")
			row.itemR(con, "axis_y")
			row.itemR(con, "axis_z")
		
class OBJECT_PT_constraints(ConstraintButtonsPanel):
	__idname__ = "OBJECT_PT_constraints"
	__label__ = "Constraints"
	__context__ = "constraint"

	def poll(self, context):
		return (context.object != None)
		
	def draw(self, context):
		ob = context.object
		layout = self.layout

		row = layout.row()
		row.item_menu_enumO("OBJECT_OT_constraint_add", "type")
		row.itemL();

		for con in ob.constraints:
			self.draw_constraint(con)

class BONE_PT_constraints(ConstraintButtonsPanel):
	__idname__ = "BONE_PT_constraints"
	__label__ = "Constraints"
	__context__ = "bone"

	def poll(self, context):
		ob = context.object
		return (ob and ob.type == "ARMATURE" and context.bone)
		
	def draw(self, context):
		ob = context.object
		pchan = ob.pose.pose_channels[context.bone.name]
		layout = self.layout

		row = layout.row()
		row.item_menu_enumO("POSE_OT_constraint_add", "type")
		row.itemL();

		for con in pchan.constraints:
			self.draw_constraint(con)

bpy.types.register(OBJECT_PT_constraints)
bpy.types.register(BONE_PT_constraints)
