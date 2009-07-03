
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def poll(self, context):
		return (context.object != None)
		
class PHYSICS_PT_softbody(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_softbody"
	__label__ = "Soft Body"

	def draw(self, context):
		layout = self.layout
		md = context.soft_body
		ob = context.object

		split = layout.split()
		split.operator_context = "EXEC_DEFAULT"

		if md:
			# remove modifier + settings
			split.set_context_pointer("modifier", md)
			split.itemO("OBJECT_OT_modifier_remove", text="Remove")

			row = split.row(align=True)
			row.itemR(md, "render", text="")
			row.itemR(md, "realtime", text="")
		else:
			# add modifier
			split.item_enumO("OBJECT_OT_modifier_add", "type", "SOFTBODY", text="Add")
			split.itemL()

		if md:
			softbody = md.settings

			split = layout.split()
			
bpy.types.register(PHYSICS_PT_softbody)

