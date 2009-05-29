
import bpy

class OUTLINER_HT_header(bpy.types.Header):
	__space_type__ = "OUTLINER"
	__idname__ = "OUTLINER_HT_header"

	def draw(self, context):
		so = context.space_data
		layout = self.layout

		layout.template_header(context)

		if context.area.show_menus:
			row = layout.row(align=True)
			row.itemM(context, "OUTLINER_MT_view")
			
		row = layout.row(align=True)
		row.itemR(so, "display_mode", text="")

class OUTLINER_MT_view(bpy.types.Menu):
	__space_type__ = "OUTLINER"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout
		so = context.space_data

		layout.column()
		row.itemR(so, "show_restriction_columns")
		#layout.itemO("TEXT_OT_new")

bpy.types.register(OUTLINER_HT_header)
bpy.types.register(OUTLINER_MT_view)

