
import bpy

class ConstraintButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "object"

	def draw_constraint(self, con):
		layout = self.layout
		box = layout.template_constraint(con)

		if box:
			if con.type == "CHILD_OF":
				self.child_of(box, con)
			elif con.type == "TRACK_TO":
				self.track_to(box, con)
			#elif con.type == "IK":
			#	self.ik(box, con)
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
			#elif con.type == "ACTION":
			#	self.action(box, con)
			elif con.type == "LOCKED_TRACK":
				self.locked_track(box, con)
			elif con.type == "LIMIT_DISTANCE":
				self.limit_distance(box, con)
			elif con.type == "STRETCH_TO":
				self.stretch_to(box, con)
			elif con.type == "FLOOR":
				self.floor(box, con)
			#elif con.type == "RIGID_BODY_JOINT"
			#	self.rigid_body(box, con)
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
				row.itemL(icon=8) # XXX

			if owner:
				row.itemR(con, "owner_space", text="")
			
	def target_template(self, layout, con, subtargets=True):
		layout.itemR(con, "target") # XXX limiting settings for only 'curves' or some type of object
		
		if con.target and subtargets:
			if con.target.type == "ARMATURE":
				layout.itemR(con, "subtarget", text="Bone") # XXX autocomplete
				
				row = layout.row()
				row.itemL(text="Head/Tail:")
				row.itemR(con, "head_tail", text="")
			elif con.target.type in ("MESH", "LATTICE"):
				layout.itemR(con, "subtarget", text="Vertex Group") # XXX autocomplete
	
	def child_of(self, layout, con):
		self.target_template(layout, con)
		
		layout.itemL(text="Use Channel(s):")
		
		row = layout.row(align=True)
		row.itemR(con, "locationx", text="Loc X", toggle=True)
		row.itemR(con, "locationy", text="Loc Y", toggle=True)
		row.itemR(con, "locationz", text="Loc Z", toggle=True)
		
		row = layout.row(align=True)
		row.itemR(con, "rotationx", text="Rot X", toggle=True)
		row.itemR(con, "rotationy", text="Rot X", toggle=True)
		row.itemR(con, "rotationz", text="Rot X", toggle=True)
		
		row = layout.row(align=True)
		row.itemR(con, "sizex", text="Scale X", toggle=True)
		row.itemR(con, "sizey", text="Scale X", toggle=True)
		row.itemR(con, "sizez", text="Scale X", toggle=True)
		
		# Missing
		row = layout.row()
		row.itemL(text="SET OFFSET")
		row.itemL(text="CLEAR OFFSET")
		
	def track_to(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemL(text="Align:")
		row.itemR(con, "target_z", toggle=True)
		
		row = layout.row()
		row.itemL(text="To:")
		row.itemR(con, "track", expand=True)
		row.itemL(text="Up:")
		row.itemR(con, "up", expand=True)
		
		self.space_template(layout, con)
		
	#def ik(self, layout, con):
	
	def follow_path(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemR(con, "curve_follow", toggle=True)
		row.itemR(con, "offset")
		
		row = layout.row()
		row.itemL(text="Forward:")
		row.itemR(con, "forward", expand=True)
		row.itemL(text="Up:")
		row.itemR(con, "up", expand=True)
		
	def limit_rotation(self, layout, con):
		row = layout.row(align=True)
		row.itemR(con, "use_limit_x", toggle=True)
		row.itemR(con, "minimum_x", text="Min")
		row.itemR(con, "maximum_x", text="Max")
		
		row = layout.row(align=True)
		row.itemR(con, "use_limit_y", toggle=True)
		row.itemR(con, "minimum_y", text="Min")
		row.itemR(con, "maximum_y", text="Max")
		
		row = layout.row(align=True)
		row.itemR(con, "use_limit_z", toggle=True)
		row.itemR(con, "minimum_z", text="Min")
		row.itemR(con, "maximum_z", text="Max")
		
		row = layout.row()
		row.itemR(con, "limit_transform", toggle=True)
		row.itemL()
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
		
	def limit_location(self, layout, con):
		split = layout.split()
		
		col = split.column()
		sub = col.row(align=True)
		sub.itemR(con, "use_minimum_x", toggle=True)
		sub.itemR(con, "minimum_x", text="")
		sub = col.row(align=True)
		sub.itemR(con, "use_minimum_y", toggle=True)
		sub.itemR(con, "minimum_y", text="")
		sub = col.row(align=True)
		sub.itemR(con, "use_minimum_z", toggle=True)
		sub.itemR(con, "minimum_z", text="")
		
		col = split.column()
		sub = col.row(align=True)
		sub.itemR(con, "use_maximum_x", toggle=True)
		sub.itemR(con, "maximum_x", text="")
		sub = col.row(align=True)
		sub.itemR(con, "use_maximum_y", toggle=True)
		sub.itemR(con, "maximum_y", text="")
		sub = col.row(align=True)
		sub.itemR(con, "use_maximum_z", toggle=True)
		sub.itemR(con, "maximum_z", text="")
		
		row = layout.row()
		row.itemR(con, "limit_transform", toggle=True)
		row.itemL()
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
		
	def limit_scale(self, layout, con):
		split = layout.split()
		
		col = split.column()
		sub = col.row(align=True)
		sub.itemR(con, "use_minimum_x", toggle=True)
		sub.itemR(con, "minimum_x", text="")
		sub = col.row(align=True)
		sub.itemR(con, "use_minimum_y", toggle=True)
		sub.itemR(con, "minimum_y", text="")
		sub = col.row(align=True)
		sub.itemR(con, "use_minimum_z", toggle=True)
		sub.itemR(con, "minimum_z", text="")
		
		col = split.column()
		sub = col.row(align=True)
		sub.itemR(con, "use_maximum_x", toggle=True)
		sub.itemR(con, "maximum_x", text="")
		sub = col.row(align=True)
		sub.itemR(con, "use_maximum_y", toggle=True)
		sub.itemR(con, "maximum_y", text="")
		sub = col.row(align=True)
		sub.itemR(con, "use_maximum_z", toggle=True)
		sub.itemR(con, "maximum_z", text="")
		
		row = layout.row()
		row.itemR(con, "limit_transform", toggle=True)
		row.itemL()
		
		row = layout.row()
		row.itemL(text="Convert:")
		row.itemR(con, "owner_space", text="")
	
	def copy_rotation(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row(align=True)
		row.itemR(con, "rotate_like_x", text="X", toggle=True)
		row.itemR(con, "invert_x", text="-", toggle=True)
		row.itemR(con, "rotate_like_y", text="Y", toggle=True)
		row.itemR(con, "invert_y", text="-", toggle=True)
		row.itemR(con, "rotate_like_z", text="Z", toggle=True)
		row.itemR(con, "invert_z", text="-", toggle=True)

		layout.itemR(con, "offset", toggle=True)
		
		self.space_template(layout, con)
		
	def copy_location(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row(align=True)
		row.itemR(con, "locate_like_x", text="X", toggle=True)
		row.itemR(con, "invert_x", text="-", toggle=True)
		row.itemR(con, "locate_like_y", text="Y", toggle=True)
		row.itemR(con, "invert_y", text="-", toggle=True)
		row.itemR(con, "locate_like_z", text="Z", toggle=True)
		row.itemR(con, "invert_z", text="-", toggle=True)

		layout.itemR(con, "offset", toggle=True)
		
		self.space_template(layout, con)
		
	def copy_scale(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row(align=True)
		row.itemR(con, "size_like_x", text="X", toggle=True)
		row.itemR(con, "size_like_y", text="Y", toggle=True)
		row.itemR(con, "size_like_z", text="Z", toggle=True)

		layout.itemR(con, "offset", toggle=True)
		
		self.space_template(layout, con)
		
	#def sctipt(self, layout, con):
	#def action(self, layout, con):
	
	def locked_track(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemL(text="To:")
		row.itemR(con, "track", expand=True)
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
		row.itemR(con, "sticky", toggle=True)
		row.itemR(con, "use_rotation", toggle=True)
		
		layout.itemR(con, "offset")
		
		row = layout.row()
		row.itemL(text="Min/Max:")
		row.itemR(con, "floor_location", expand=True)
		
	#def rigid_body(self, layout, con):
	
	def clamp_to(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemL(text="Main Axis:")
		row.itemR(con, "main_axis", expand=True)
		
		row = layout.row()
		row.itemL(text="Options")
		row.itemR(con, "cyclic", toggle=True)
		
	def transform(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row()
		row.itemR(con, "extrapolate_motion", text="Extrapolate")
		row.itemL()
		
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
			row.itemR(con, "axis_x", toggle=True)
			row.itemR(con, "axis_y", toggle=True)
			row.itemR(con, "axis_z", toggle=True)
		
class OBJECT_PT_constraints(ConstraintButtonsPanel):
	__idname__ = "OBJECT_PT_constraints"
	__label__ = "Constraints"
	__context__ = "object"

	def poll(self, context):
		ob = context.active_object
		return (ob != None)
		
	def draw(self, context):
		ob = context.active_object
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
		ob = context.active_object
		return (ob and ob.type == "ARMATURE")
		
	def draw(self, context):
		ob = context.active_object
		pchan = ob.pose.pose_channels[0]
		layout = self.layout

		#row = layout.row()
		#row.item_menu_enumO("BONE_OT_constraint_add", "type")
		#row.itemL();

		for con in pchan.constraints:
			self.draw_constraint(con)

bpy.types.register(OBJECT_PT_constraints)
bpy.types.register(BONE_PT_constraints)