
import bpy
 
class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "bone"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type == 'ARMATURE')

class DATA_PT_bone(DataButtonsPanel):
	__idname__ = "DATA_PT_bone"
	__label__ = "Bone"

	def draw(self, context):
		bone = context.active_object.data.bones[0]
		layout = self.layout

		split = layout.split()

		sub = split.column()
		sub.itemR(bone, "name")
		sub.itemR(bone, "parent")
		sub.itemR(bone, "connected")
		sub.itemR(bone, "deform")

		sub.itemL(text="Inherit:")
		sub.itemR(bone, "hinge")
		sub.itemR(bone, "inherit_scale")

		sub.itemL(text="Envelope:")
		sub.itemR(bone, "envelope_distance", text="Distance")
		sub.itemR(bone, "envelope_weight", text="Weight")
		sub.itemR(bone, "multiply_vertexgroup_with_envelope", text="Multiply")

		sub = split.column()
		#sub.itemR(bone, "layer")
		sub.itemL(text="Display:")
		sub.itemR(bone, "draw_wire", text="Wireframe")
		sub.itemR(bone, "editmode_hidden", text="Hide (EditMode)")
		sub.itemR(bone, "pose_channel_hidden", text="Hide (PoseMode)")

		sub.itemL(text="Curved Bones:")
		sub.itemR(bone, "bbone_segments", text="Segments")
		sub.itemR(bone, "bbone_in", text="Ease In")
		sub.itemR(bone, "bbone_out", text="Ease Out")
		
		sub.itemR(bone, "cyclic_offset")

class DATA_PT_constraints(DataButtonsPanel):
	__idname__ = "DATA_PT_constraints"
	__label__ = "Constraints"
	
	def draw(self, context):
		bone = context.active_object.data.bones[0]
		layout = self.layout
		split = layout.split()
		
		sub = split.column()

bpy.types.register(DATA_PT_bone)
#bpy.types.register(DATA_PT_constraints)