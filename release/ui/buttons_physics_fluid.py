
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def poll(self, context):
		ob = context.object
		return (ob and ob.type == 'MESH')
		
class PHYSICS_PT_fluid(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_fluid"
	__label__ = "Fluid"

	def draw(self, context):
		layout = self.layout
		md = context.fluid
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
			split.item_enumO("OBJECT_OT_modifier_add", "type", "FLUID_SIMULATION", text="Add")
			split.itemL()

		if md:
			fluid = md.settings

			col = layout.column(align=True)
			row = col.row()
			row.item_enumR(fluid, "type", "DOMAIN")
			row.item_enumR(fluid, "type", "FLUID")
			row.item_enumR(fluid, "type", "OBSTACLE")
			row = col.row()
			row.item_enumR(fluid, "type", "INFLOW")
			row.item_enumR(fluid, "type", "OUTFLOW")
			row.item_enumR(fluid, "type", "PARTICLE")
			row.item_enumR(fluid, "type", "CONTROL")

			if fluid.type == "DOMAIN":
				layout.itemO("FLUID_OT_bake", text="BAKE")

				col = layout.column(align=True)

				col.itemL(text="Req. Mem.: " + fluid.memory_estimate)
				col.itemR(fluid, "resolution")
				col.itemR(fluid, "preview_resolution")
			
bpy.types.register(PHYSICS_PT_fluid)

