
import bpy

class OUTLINER_HT_header(bpy.types.Header):
	__space_type__ = 'OUTLINER'

	def draw(self, context):
		so = context.space_data
		sce = context.scene
		layout = self.layout

		row = layout.row(align=True)
		row.template_header()

		if context.area.show_menus:
			sub = row.row(align=True)
			sub.itemM("OUTLINER_MT_view")
			
		row = layout.row()
		row.itemR(so, "display_mode", text="")
		
		if so.display_mode == 'DATABLOCKS':
			row = layout.row(align=True)
			row.itemO("anim.keyingset_add_new", text="", icon=31)
			# row.itemR(sce, "active_keyingset", text="KS: ")
			# ks = sce.keyingsets[sce.active_keyingset - 1]
			# row.itemR(ks, "name", text="")
			## row.itemR(sce, "keyingsets")
			
			row = layout.row()
			row.itemO("outliner.keyingset_add_selected", text="", icon=31)
			row.itemO("outliner.keyingset_remove_selected", text="", icon=32)
			
			row.itemO("anim.insert_keyframe", text="", icon=514)
			row.itemO("anim.delete_keyframe", text="", icon=513)
		

class OUTLINER_MT_view(bpy.types.Menu):
	__label__ = "View"

	def draw(self, context):
		layout = self.layout
		so = context.space_data

		col = layout.column()
		col.itemR(so, "show_restriction_columns")
		#layout.itemO("text.new")

bpy.types.register(OUTLINER_HT_header)
bpy.types.register(OUTLINER_MT_view)

