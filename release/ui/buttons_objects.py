import bpy

class OBJECT_PT_transform(bpy.types.Panel):
	__label__ = "Transform"
	__context__ = "object"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		if not ob:
			return

		layout.row()
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

		layout.row()
		layout.itemR(ob, "pass_index")
		layout.itemR(ob, "parent")

		# layout.left_right()
		# layout.itemO("OBJECT_OT_add_group");

		for group in bpy.data.groups:
			if ob in group.objects:
				sub = layout.box()

				sub.split(number=2, lr=True)
				sub.sub(0).itemR(group, "name")
				# sub.sub(1).itemO("OBJECT_OT_remove_group")

				sub.row()
				sub.itemR(group, "layer")
				sub.itemR(group, "dupli_offset")

class OBJECT_PT_display(bpy.types.Panel):
	__label__ = "Display"
	__context__ = "object"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		if not ob:
			return

		layout.row()
		layout.itemR(ob, "max_draw_type", text="Type")
		layout.itemR(ob, "draw_bounds_type", text="Bounds")

		layout.column_flow()
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

		layout.column()
		layout.itemR(ob, "dupli_type", text="", expand=True)

		if ob.dupli_type == "FRAMES":
			layout.column_flow()
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

		layout.split(number=2)
		
		sub = layout.sub(0)
		sub.column()
		sub.itemL(text="Time Offset:")
		sub.itemR(ob, "time_offset_edit", text="Edit")
		sub.itemR(ob, "time_offset_particle", text="Particle")
		sub.itemR(ob, "time_offset_parent", text="Parent")
		sub.itemR(ob, "slow_parent")
		sub.itemR(ob, "time_offset", text="Offset:")
		
		sub = layout.sub(1)
		sub.column()
		sub.itemL(text="Tracking:")
		sub.itemR(ob, "track_axis", text="Axis")
		sub.itemR(ob, "up_axis", text="Up Axis")
		sub.itemR(ob, "track_rotation", text="Rotation")

bpy.ui.addPanel(OBJECT_PT_transform, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(OBJECT_PT_groups, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(OBJECT_PT_display, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(OBJECT_PT_duplication, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(OBJECT_PT_animation, "BUTTONS_WINDOW", "WINDOW")

