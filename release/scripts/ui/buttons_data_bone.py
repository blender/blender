
import bpy
 
class BoneButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "bone"
	
	def poll(self, context):
		return (context.bone or context.edit_bone)

class BONE_PT_context_bone(BoneButtonsPanel):
	__show_header__ = False

	def draw(self, context):
		layout = self.layout
		
		bone = context.bone
		if not bone:
			bone = context.edit_bone
		
		row = layout.row()
		row.itemL(text="", icon='ICON_BONE_DATA')
		row.itemR(bone, "name", text="")

class BONE_PT_transform(BoneButtonsPanel):
	__label__ = "Transform"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		bone = context.bone
		if not bone:
			bone = context.edit_bone

			row = layout.row()
			row.column().itemR(bone, "head")
			row.column().itemR(bone, "tail")

			col = row.column()
			sub = col.column(align=True)
			sub.itemL(text="Roll:")
			sub.itemR(bone, "roll", text="")
			sub.itemL()
			sub.itemR(bone, "locked")

		else:
			pchan = ob.pose.pose_channels[context.bone.name]

			layout.itemR(pchan, "rotation_mode")

			row = layout.row()
			col = row.column()
			col.itemR(pchan, "location")
			col.active = not (bone.parent and bone.connected)

			col = row.column()
			if pchan.rotation_mode == 'QUATERNION':
				col.itemR(pchan, "rotation_quaternion", text="Rotation")
			elif pchan.rotation_mode == 'AXIS_ANGLE':
				#col.itemL(text="Rotation")
				#col.itemR(pchan, "rotation_angle", text="Angle")
				#col.itemR(pchan, "rotation_axis", text="Axis")
				col.itemR(pchan, "rotation_axis_angle", text="Rotation")
			else:
				col.itemR(pchan, "rotation_euler", text="Rotation")

			row.column().itemR(pchan, "scale")
				
class BONE_PT_transform_locks(BoneButtonsPanel):
	__label__ = "Transform Locks"
	__default_closed__ = True
	
	def poll(self, context):
		return context.bone
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		bone = context.bone
		pchan = ob.pose.pose_channels[context.bone.name]
		
		row = layout.row()
		col = row.column()
		col.itemR(pchan, "lock_location")
		col.active = not (bone.parent and bone.connected)
		
		col = row.column()
		if pchan.rotation_mode in ('QUATERNION', 'AXIS_ANGLE'):
			col.itemR(pchan, "lock_rotations_4d", text="Lock Rotation")
			if pchan.lock_rotations_4d:
				col.itemR(pchan, "lock_rotation_w", text="W")
			col.itemR(pchan, "lock_rotation", text="")
		else:
			col.itemR(pchan, "lock_rotation", text="Rotation")
		
		row.column().itemR(pchan, "lock_scale")

class BONE_PT_bone(BoneButtonsPanel):
	__label__ = "Bone"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		bone = context.bone
		arm = context.armature
		
		if not bone:
			bone = context.edit_bone
			pchan = None
		else:
			pchan = ob.pose.pose_channels[context.bone.name]

		split = layout.split()

		col = split.column()
		col.itemL(text="Parent:")
		if context.bone:
			col.itemR(bone, "parent", text="")
		else:
			col.item_pointerR(bone, "parent", arm, "edit_bones", text="")
		
		row = col.row()
		row.active = bone.parent != None
		row.itemR(bone, "connected")
		
		col.itemL(text="Layers:")
		col.itemR(bone, "layer", text="")
		
		col = split.column()
		col.itemL(text="Inherit:")
		col.itemR(bone, "hinge", text="Rotation")
		col.itemR(bone, "inherit_scale", text="Scale")
		col.itemL(text="Display:")
		col.itemR(bone, "draw_wire", text="Wireframe")
		col.itemR(bone, "hidden", text="Hide")
		
		if ob and pchan:
			split = layout.split()
			
			col = split.column()
			col.itemL(text="Bone Group:")
			col.item_pointerR(pchan, "bone_group", ob.pose, "bone_groups", text="")
			
			col = split.column()
			col.itemL(text="Custom Shape:")
			col.itemR(pchan, "custom_shape", text="")

class BONE_PT_inverse_kinematics(BoneButtonsPanel):
	__label__ = "Inverse Kinematics"
	__default_closed__ = True
	
	def poll(self, context):
		ob = context.object
		bone = context.bone

		if ob and context.bone:
			pchan = ob.pose.pose_channels[context.bone.name]
			return pchan.has_ik
		
		return False

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		bone = context.bone
		pchan = ob.pose.pose_channels[context.bone.name]

		row = layout.row()
		row.itemR(ob.pose, "ik_solver")

		split = layout.split(percentage=0.25)
		split.itemR(pchan, "ik_dof_x", text="X")
		row = split.row()
		row.itemR(pchan, "ik_stiffness_x", text="Stiffness", slider=True)
		row.active = pchan.ik_dof_x

		split = layout.split(percentage=0.25)
		row = split.row()
		row.itemR(pchan, "ik_limit_x", text="Limit")
		row.active = pchan.ik_dof_x
		row = split.row(align=True)
		row.itemR(pchan, "ik_min_x", text="")
		row.itemR(pchan, "ik_max_x", text="")
		row.active = pchan.ik_dof_x and pchan.ik_limit_x

		split = layout.split(percentage=0.25)
		split.itemR(pchan, "ik_dof_y", text="Y")
		row = split.row()
		row.itemR(pchan, "ik_stiffness_y", text="Stiffness", slider=True)
		row.active = pchan.ik_dof_y

		split = layout.split(percentage=0.25)
		row = split.row()
		row.itemR(pchan, "ik_limit_y", text="Limit")
		row.active = pchan.ik_dof_y
		row = split.row(align=True)
		row.itemR(pchan, "ik_min_y", text="")
		row.itemR(pchan, "ik_max_y", text="")
		row.active = pchan.ik_dof_y and pchan.ik_limit_y

		split = layout.split(percentage=0.25)
		split.itemR(pchan, "ik_dof_z", text="Z")
		row = split.row()
		row.itemR(pchan, "ik_stiffness_z", text="Stiffness", slider=True)
		row.active = pchan.ik_dof_z

		split = layout.split(percentage=0.25)
		row = split.row()
		row.itemR(pchan, "ik_limit_z", text="Limit")
		row.active = pchan.ik_dof_z
		row = split.row(align=True)
		row.itemR(pchan, "ik_min_z", text="")
		row.itemR(pchan, "ik_max_z", text="")
		row.active = pchan.ik_dof_z and pchan.ik_limit_z
		split = layout.split()
		split.itemR(pchan, "ik_stretch", text="Stretch", slider=True)
		split.itemL()

		if ob.pose.ik_solver == "ITASC":
			layout.itemL(text="Joint constraint:")
			split = layout.split(percentage=0.3)
			row = split.row()
			row.itemR(pchan, "ik_rot_control", text="Rotation")
			row = split.row()
			row.itemR(pchan, "ik_rot_weight", text="Weight", slider=True)
			row.active = pchan.ik_rot_control
			# not supported yet
			#split = layout.split(percentage=0.3)
			#row = split.row()
			#row.itemR(pchan, "ik_lin_control", text="Size")
			#row = split.row()
			#row.itemR(pchan, "ik_lin_weight", text="Weight", slider=True)
			#row.active = pchan.ik_lin_control

class BONE_PT_deform(BoneButtonsPanel):
	__label__ = "Deform"
	__default_closed__ = True

	def draw_header(self, context):
		bone = context.bone
		
		if not bone:
			bone = context.edit_bone
			
		self.layout.itemR(bone, "deform", text="")

	def draw(self, context):
		layout = self.layout
		
		bone = context.bone
		
		if not bone:
			bone = context.edit_bone
	
		layout.active = bone.deform
			
		split = layout.split()

		col = split.column()
		col.itemL(text="Envelope:")
		
		sub = col.column(align=True)
		sub.itemR(bone, "envelope_distance", text="Distance")
		sub.itemR(bone, "envelope_weight", text="Weight")
		col.itemR(bone, "multiply_vertexgroup_with_envelope", text="Multiply")

		sub = col.column(align=True)
		sub.itemL(text="Radius:")
		sub.itemR(bone, "head_radius", text="Head")
		sub.itemR(bone, "tail_radius", text="Tail")

		col = split.column()
		col.itemL(text="Curved Bones:")
		
		sub = col.column(align=True)
		sub.itemR(bone, "bbone_segments", text="Segments")
		sub.itemR(bone, "bbone_in", text="Ease In")
		sub.itemR(bone, "bbone_out", text="Ease Out")
		
		col.itemL(text="Offset:")
		col.itemR(bone, "cyclic_offset")

class BONE_PT_iksolver_itasc(BoneButtonsPanel):
	__idname__ = "BONE_PT_iksolver_itasc"
	__label__ = "iTaSC parameters"
	__default_closed__ = True
	
	def poll(self, context):
		ob = context.object
		bone = context.bone

		if ob and context.bone:
			pchan = ob.pose.pose_channels[context.bone.name]
			return pchan.has_ik and ob.pose.ik_solver == "ITASC" and ob.pose.ik_param
		
		return False

	def draw(self, context):
		layout = self.layout

		ob = context.object
		itasc = ob.pose.ik_param

		layout.row().itemR(itasc, "simulation")
		if itasc.simulation:
			split = layout.split()
			row = split.row()
			row.itemR(itasc, "reiteration")
			row = split.row()
			if itasc.reiteration:
				itasc.initial_reiteration = True
			row.itemR(itasc, "initial_reiteration")
			row.active = not itasc.reiteration
		
		flow = layout.column_flow()
		flow.itemR(itasc, "precision")
		flow.itemR(itasc, "num_iter")
		flow.active = not itasc.simulation or itasc.initial_reiteration or itasc.reiteration

		if itasc.simulation:		
			layout.itemR(itasc, "auto_step")
			row = layout.row()
			if itasc.auto_step:
				row.itemR(itasc, "min_step")
				row.itemR(itasc, "max_step")
			else:
				row.itemR(itasc, "num_step")
			
		layout.itemR(itasc, "solver")
		if itasc.simulation:
			layout.itemR(itasc, "feedback")
			layout.itemR(itasc, "max_velocity")
		if itasc.solver == "DLS":
			row = layout.row()
			row.itemR(itasc, "dampmax")
			row.itemR(itasc, "dampeps")

bpy.types.register(BONE_PT_context_bone)
bpy.types.register(BONE_PT_transform)
bpy.types.register(BONE_PT_transform_locks)
bpy.types.register(BONE_PT_bone)
bpy.types.register(BONE_PT_deform)
bpy.types.register(BONE_PT_inverse_kinematics)
bpy.types.register(BONE_PT_iksolver_itasc)
