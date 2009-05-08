
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

			sub.row()
			sub.itemR(md, "expanded", text="")
			sub.itemR(md, "name", text="")

			sub.itemR(md, "render", text="")
			sub.itemR(md, "realtime", text="")
			sub.itemR(md, "editmode", text="")
			sub.itemR(md, "on_cage", text="")

			if (md.expanded):
				sub.row()
				sub.itemS()

				if (md.type == 'ARMATURE'):
					self.armature(sub, md)

	def armature(self, layout, md):
		layout.column()
		layout.itemR(md, "object")
		layout.row()
		layout.itemR(md, "vertex_group")
		layout.itemR(md, "invert")
		layout.column_flow()
		layout.itemR(md, "use_vertex_groups")
		layout.itemR(md, "use_bone_envelopes")
		layout.itemR(md, "quaternion")
		layout.itemR(md, "b_bone_rest")
		layout.itemR(md, "multi_modifier")
		
bpy.types.register(DATA_PT_modifiers)


