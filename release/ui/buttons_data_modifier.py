
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"

	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type in ('MESH', 'CURVE', 'SURFACE', 'TEXT', 'LATTICE'))
		
class DATA_PT_modifiers(DataButtonsPanel):
	__idname__ = "DATA_PT_modifiers"
	__label__ = "Modifiers"

	def draw(self, context):
		ob = context.active_object
		layout = self.layout

		if not ob:
			return

		layout.row()
		layout.item_menu_enumO("OBJECT_OT_modifier_add", "type")

		for md in ob.modifiers:
			sub = layout.box()

			row = sub.row()
			row.itemR(md, "expanded", text="")
			row.itemR(md, "name", text="")

			row.itemR(md, "render", text="")
			row.itemR(md, "realtime", text="")
			row.itemR(md, "editmode", text="")
			row.itemR(md, "on_cage", text="")

			if md.expanded:
				sub.itemS()

				if (md.type == 'ARMATURE'):
					self.armature(sub, md)

	def armature(self, layout, md):
		layout.itemR(md, "object")
		row = layout.row()
		row.itemR(md, "vertex_group")
		row.itemR(md, "invert")
		flow = layout.column_flow()
		flow.itemR(md, "use_vertex_groups")
		flow.itemR(md, "use_bone_envelopes")
		flow.itemR(md, "quaternion")
		flow.itemR(md, "b_bone_rest")
		flow.itemR(md, "multi_modifier")
		
bpy.types.register(DATA_PT_modifiers)


