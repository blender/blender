
import bpy
 
class BoneButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "bone"
	
	def poll(self, context):
		return (context.bone or context.edit_bone)

class BONE_PT_context(BoneButtonsPanel):
	__idname__ = "BONE_PT_context"
	__label__ = " "

	def draw(self, context):
		layout = self.layout
		bone = context.bone
		if not bone:
			bone = context.edit_bone
		
		split = layout.split(percentage=0.06)
		split.itemL(text="", icon="ICON_BONE_DATA")
		split.itemR(bone, "name", text="")

class BONE_PT_bone(BoneButtonsPanel):
	__idname__ = "BONE_PT_bone"
	__label__ = "Bone"


	def draw(self, context):
		layout = self.layout
		bone = context.bone
		if not bone:
			bone = context.edit_bone

		split = layout.split()

		sub = split.column()
		sub.itemR(bone, "parent")
		sub.itemR(bone, "connected")

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

		sub = split.column()
		sub.itemL(text="Envelope:")
		sub.itemR(bone, "envelope_distance", text="Distance")
		sub.itemR(bone, "envelope_weight", text="Weight")
		sub.itemR(bone, "multiply_vertexgroup_with_envelope", text="Multiply")
		sub = split.column()
		
		sub.itemL(text="Curved Bones:")
		sub.itemR(bone, "bbone_segments", text="Segments")
		sub.itemR(bone, "bbone_in", text="Ease In")
		sub.itemR(bone, "bbone_out", text="Ease Out")
		
		sub.itemR(bone, "cyclic_offset")


bpy.types.register(BONE_PT_context)
bpy.types.register(BONE_PT_bone)
bpy.types.register(BONE_PT_deform)
