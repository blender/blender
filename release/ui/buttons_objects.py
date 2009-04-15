import bpy

class OBJECT_PT_transform(bpy.types.Panel):
	__label__ = "Transform"
	__context__ = "object"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		if not ob:
			return

		layout.template_row()
		layout.itemR(ob, "location")
		layout.itemR(ob, "rotation")
		layout.itemR(ob, "scale")

class OBJECT_PT_groups(bpy.types.Panel):
	__label__ = "Groups"
	__context__ = "object"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		if not ob:
			return

		layout.template_row()
		layout.itemR(ob, "pass_index")
		layout.itemR(ob, "parent")

		# layout.template_left_right()
		# layout.itemO("OBJECT_OT_add_group");

		for group in bpy.data.groups:
			if ob in group.objects:
				sublayout = layout.template_stack()

				sublayout.template_left_right()
				sublayout.itemR(group, "name")
				# sublayout.itemO("OBJECT_OT_remove_group")

				sublayout.template_row()
				sublayout.itemR(group, "layer")
				sublayout.itemR(group, "dupli_offset")

class OBJECT_PT_display(bpy.types.Panel):
	__label__ = "Display"
	__context__ = "object"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		if not ob:
			return

		layout.template_row()
		layout.itemR(ob, "max_draw_type", text="Type")
		layout.itemR(ob, "draw_bounds_type", text="Bounds")

		layout.template_column_flow(2)
		layout.itemR(ob, "draw_name", text="Name")
		layout.itemR(ob, "draw_axis", text="Axis")
		layout.itemR(ob, "draw_wire", text="Wire")
		layout.itemR(ob, "draw_texture_space", text="Texture Space")
		layout.itemR(ob, "x_ray", text="X-Ray")
		layout.itemR(ob, "draw_transparent", text="Transparency")

class OBJECT_PT_duplication(bpy.types.Panel):
	__label__ = "Duplication"
	__context__ = "object"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		if not ob:
			return

		layout.template_column()
		layout.itemR(ob, "dupli_type", text="")

		if ob.dupli_type == "FRAMES":
			layout.template_column_flow(2)
			layout.itemR(ob, "dupli_frames_start", text="Start:")
			layout.itemR(ob, "dupli_frames_end", text="End:")
			layout.itemR(ob, "dupli_frames_on", text="On:")
			layout.itemR(ob, "dupli_frames_off", text="Off:")

class OBJECT_PT_animation(bpy.types.Panel):
	__label__ = "Animation"
	__context__ = "object"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		if not ob:
			return

		layout.template_column()
		
		layout.template_slot("COLUMN_1")
		layout.itemL(text="Time Offset:")
		layout.itemR(ob, "time_offset_edit", text="Edit")
		layout.itemR(ob, "time_offset_particle", text="Particle")
		layout.itemR(ob, "time_offset_parent", text="Parent")
		layout.itemR(ob, "slow_parent")
		layout.itemR(ob, "time_offset", text="Offset:")
		
		layout.template_slot("COLUMN_2")
		layout.itemL(text="Tracking:")
		layout.itemR(ob, "track_axis", text="Axis")
		layout.itemR(ob, "up_axis", text="Up Axis")
		layout.itemR(ob, "track_rotation", text="Rotation")

bpy.ui.addPanel(OBJECT_PT_transform, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(OBJECT_PT_groups, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(OBJECT_PT_display, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(OBJECT_PT_duplication, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(OBJECT_PT_animation, "BUTTONS_WINDOW", "WINDOW")

