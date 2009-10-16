
import bpy

class OUTLINER_HT_header(bpy.types.Header):
	__space_type__ = 'OUTLINER'

	def draw(self, context):
		layout = self.layout
		
		space = context.space_data
		scene = context.scene
		ks = context.scene.active_keying_set

		row = layout.row(align=True)
		row.template_header()

		if context.area.show_menus:
			sub = row.row(align=True)
			sub.itemM("OUTLINER_MT_view")

		layout.itemR(space, "display_mode", text="")
		
		layout.itemS()
		
		if space.display_mode == 'DATABLOCKS':
			row = layout.row(align=True)
			row.itemO("anim.keying_set_add", icon='ICON_ZOOMIN', text="")
			row.itemO("anim.keying_set_remove", icon='ICON_ZOOMOUT', text="")
			if ks:
				row.item_pointerR(scene, "active_keying_set", scene, "keying_sets", text="")
				
				row = layout.row(align=True)
				row.itemO("anim.insert_keyframe", text="", icon='ICON_KEY_HLT')
				row.itemO("anim.delete_keyframe", text="", icon='ICON_KEY_DEHLT')
			else:
				row.itemL(text="No Keying Set active")

class OUTLINER_MT_view(bpy.types.Menu):
	__label__ = "View"

	def draw(self, context):
		layout = self.layout
		
		space = context.space_data

		col = layout.column()
		if space.display_mode not in ('DATABLOCKS', 'USER_PREFERENCES', 'KEYMAPS'):
			col.itemR(space, "show_restriction_columns")
			col.itemS()
			col.itemO("outliner.show_active")

		col.itemO("outliner.show_one_level")
		col.itemO("outliner.show_hierarchy")

bpy.types.register(OUTLINER_HT_header)
bpy.types.register(OUTLINER_MT_view)
