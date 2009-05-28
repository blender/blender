
import bpy

class ConstraintButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "object"

	def draw_constraint(self, con):
		layout = self.layout
		box = layout.template_constraint(con)

		if box:
			if con.type == "COPY_LOCATION":
				self.copy_location(box, con)

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
	
	def copy_location(self, layout, con):
		self.target_template(layout, con)
		
		row = layout.row(align=True)
		row.itemR(con, "locate_like_x", text="X", toggle=True)
		row.itemR(con, "invert_x", text="-", toggle=True)
		row.itemR(con, "locate_like_y", text="Y", toggle=True)
		row.itemR(con, "invert_y", text="-", toggle=True)
		row.itemR(con, "locate_like_z", text="Z", toggle=True)
		row.itemR(con, "invert_z", text="-", toggle=True)

		layout.itemR(con, "offset")

		self.space_template(layout, con)

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

