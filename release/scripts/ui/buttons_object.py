
import bpy

class ObjectButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "object"

class OBJECT_PT_context_object(ObjectButtonsPanel):
	__show_header__ = False

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		
		row = layout.row()
		row.itemL(text="", icon='ICON_OBJECT_DATA')
		row.itemR(ob, "name", text="")

class OBJECT_PT_transform(ObjectButtonsPanel):
	__label__ = "Transform"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		
		layout.itemR(ob, "rotation_mode")

		row = layout.row()
		
		row.column().itemR(ob, "location")
		if ob.rotation_mode == 'QUATERNION':
			row.column().itemR(ob, "rotation_quaternion", text="Rotation")
		elif ob.rotation_mode == 'AXIS_ANGLE':
			#row.column().itemL(text="Rotation")
			#row.column().itemR(pchan, "rotation_angle", text="Angle")
			#row.column().itemR(pchan, "rotation_axis", text="Axis")
			row.column().itemR(ob, "rotation_axis_angle", text="Rotation")
		else:
			row.column().itemR(ob, "rotation_euler", text="Rotation")
			
		row.column().itemR(ob, "scale")
		
class OBJECT_PT_transform_locks(ObjectButtonsPanel):
	__label__ = "Transform Locks"
	__default_closed__ = True
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		
		row = layout.row()
		
		col = row.column()
		col.itemR(ob, "lock_location")
		
		col = row.column()
		if ob.rotation_mode in ('QUATERNION', 'AXIS_ANGLE'):
			col.itemR(ob, "lock_rotations_4d", text="Lock Rotation")
			if ob.lock_rotations_4d:
				col.itemR(ob, "lock_rotation_w", text="W")
			col.itemR(ob, "lock_rotation", text="")
		else:
			col.itemR(ob, "lock_rotation", text="Rotation")
		
		row.column().itemR(ob, "lock_scale")

class OBJECT_PT_relations(ObjectButtonsPanel):
	__label__ = "Relations"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object

		split = layout.split()
		
		col = split.column()
		col.itemR(ob, "layers")
		col.itemS()
		col.itemR(ob, "pass_index")

		col = split.column()
		col.itemL(text="Parent:")
		col.itemR(ob, "parent", text="")

		sub = col.column()
		split = sub.split(percentage=0.3)
		split.itemL(text="Type:")
		split.itemR(ob, "parent_type", text="")
		parent = ob.parent
		if parent and ob.parent_type == 'BONE' and parent.type == 'ARMATURE':
			sub.item_pointerR(ob, "parent_bone", parent.data, "bones", text="")
		sub.active = parent != None

class OBJECT_PT_groups(ObjectButtonsPanel):
	__label__ = "Groups"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object

		split = layout.split()
		split.item_menu_enumO("object.group_add", "group", text="Add to Group")
		split.itemL()

		for group in bpy.data.groups:
			if ob.name in group.objects:
				col = layout.column(align=True)

				col.set_context_pointer("group", group)

				row = col.box().row()
				row.itemR(group, "name", text="")
				row.itemO("object.group_remove", text="", icon='VICON_X')

				split = col.box().split()
				split.column().itemR(group, "layer", text="Dupli")
				split.column().itemR(group, "dupli_offset", text="")

class OBJECT_PT_display(ObjectButtonsPanel):
	__label__ = "Display"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		
		split = layout.split()
		col = split.column()
		col.itemR(ob, "max_draw_type", text="Type")
		col = split.column()
		row = col.row()
		row.itemR(ob, "draw_bounds", text="Bounds")
		row.itemR(ob, "draw_bounds_type", text="")

		flow = layout.column_flow()
		flow.itemR(ob, "draw_name", text="Name")
		flow.itemR(ob, "draw_axis", text="Axis")
		flow.itemR(ob, "draw_wire", text="Wire")
		flow.itemR(ob, "draw_texture_space", text="Texture Space")
		flow.itemR(ob, "x_ray", text="X-Ray")
		flow.itemR(ob, "draw_transparent", text="Transparency")

class OBJECT_PT_duplication(ObjectButtonsPanel):
	__label__ = "Duplication"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object

		layout.itemR(ob, "dupli_type", expand=True)

		if ob.dupli_type == 'FRAMES':
			split = layout.split()
			
			col = split.column(align=True)
			col.itemR(ob, "dupli_frames_start", text="Start")
			col.itemR(ob, "dupli_frames_end", text="End")
			
			col = split.column(align=True)
			col.itemR(ob, "dupli_frames_on", text="On")
			col.itemR(ob, "dupli_frames_off", text="Off")
			
			layout.itemR(ob, "dupli_frames_no_speed", text="No Speed")

		elif ob.dupli_type == 'VERTS':
			layout.itemR(ob, "dupli_verts_rotation", text="Rotation")

		elif ob.dupli_type == 'FACES':
			row = layout.row()
			row.itemR(ob, "dupli_faces_scale", text="Scale")
			row.itemR(ob, "dupli_faces_inherit_scale", text="Inherit Scale")

		elif ob.dupli_type == 'GROUP':
			layout.itemR(ob, "dupli_group", text="Group")

class OBJECT_PT_animation(ObjectButtonsPanel):
	__label__ = "Animation"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Time Offset:")
		col.itemR(ob, "time_offset_edit", text="Edit")
		row = col.row()
		row.itemR(ob, "time_offset_particle", text="Particle")
		row.active = len(ob.particle_systems) != 0
		row = col.row()
		row.itemR(ob, "time_offset_parent", text="Parent")
		row.active = ob.parent != None
		row = col.row()
		row.itemR(ob, "slow_parent")
		row.active = ob.parent != None
		col.itemR(ob, "time_offset", text="Offset")

		col = split.column()
		col.itemL(text="Track:")
		col.itemR(ob, "track", text="")
		col.itemR(ob, "track_axis", text="Axis")
		col.itemR(ob, "up_axis", text="Up Axis")
		row = col.row()
		row.itemR(ob, "track_override_parent", text="Override Parent")
		row.active = ob.parent != None

bpy.types.register(OBJECT_PT_context_object)
bpy.types.register(OBJECT_PT_transform)
bpy.types.register(OBJECT_PT_transform_locks)
bpy.types.register(OBJECT_PT_relations)
bpy.types.register(OBJECT_PT_groups)
bpy.types.register(OBJECT_PT_display)
bpy.types.register(OBJECT_PT_duplication)
bpy.types.register(OBJECT_PT_animation)
