
import bpy
 
class BoneButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "bone"
	
	def poll(self, context):
		return (context.bone or context.edit_bone)

class BONE_PT_context_bone(BoneButtonsPanel):
	__idname__ = "BONE_PT_context_bone"
	__no_header__ = True

	def draw(self, context):
		layout = self.layout
		bone = context.bone
		if not bone:
			bone = context.edit_bone
		
		split = layout.split(percentage=0.06)
		split.itemL(text="", icon="ICON_BONE_DATA")
		split.itemR(bone, "name", text="")

class BONE_PT_transform(BoneButtonsPanel):
	__idname__ = "BONE_PT_transform"
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
			sub.itemS()
		else:
			pchan = ob.pose.pose_channels[context.bone.name]

			layout.itemR(pchan, "rotation_mode")

			row = layout.row()
			col = row.column()
			col.itemR(pchan, "location")
			col.active = not (bone.parent and bone.connected)

			col = row.column()
			if pchan.rotation_mode == 'QUATERNION':
				col.itemR(pchan, "rotation", text="Rotation")
			else:
				col.itemR(pchan, "euler_rotation", text="Rotation")

			row.column().itemR(pchan, "scale")

			if pchan.rotation_mode == 'QUATERNION':
				col = layout.column(align=True)
				col.itemL(text="Euler:")
				col.row().itemR(pchan, "euler_rotation", text="")

class BONE_PT_bone(BoneButtonsPanel):
	__idname__ = "BONE_PT_bone"
	__label__ = "Bone"


	def draw(self, context):
		layout = self.layout
		bone = context.bone
		arm = context.armature
		if not bone:
			bone = context.edit_bone

		split = layout.split()

		sub = split.column()
		sub.itemL(text="Parent:")
		if context.bone:
			sub.itemR(bone, "parent", text="")
		else:
			sub.item_pointerR(bone, "parent", arm, "edit_bones", text="")
		row = sub.row()
		row.itemR(bone, "connected")
		row.active = bone.parent != None

		sub.itemL(text="Layers:")
		sub.template_layers(bone, "layer")

		sub = split.column()

		sub.itemL(text="Inherit:")
		sub.itemR(bone, "hinge", text="Rotation")
		sub.itemR(bone, "inherit_scale", text="Scale")
		
		sub.itemL(text="Display:")
		sub.itemR(bone, "draw_wire", text="Wireframe")
		sub.itemR(bone, "hidden", text="Hide")

class BONE_PT_deform(BoneButtonsPanel):
	__idname__ = "BONE_PT_deform"
	__label__ = "Deform"

	def draw_header(self, context):
		layout = self.layout
		bone = context.bone
		if not bone:
			bone = context.edit_bone
			
		layout.itemR(bone, "deform", text="")

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

bpy.types.register(BONE_PT_context_bone)
bpy.types.register(BONE_PT_transform)
bpy.types.register(BONE_PT_bone)
bpy.types.register(BONE_PT_deform)

