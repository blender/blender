
import bpy

class TIME_HT_header(bpy.types.Header):
	__space_type__ = "TIMELINE"

	def draw(self, context):
		layout = self.layout
		
		st = context.space_data
		scene = context.scene

		layout.template_header()

		if context.area.show_menus:
			row = layout.row()
			#row.itemM("TIME_MT_view")
			#row.itemM("TIME_MT_frame")
			#row.itemM("TIME_MT_playback")

		layout.itemR(scene, "use_preview_range", text="PR", toggle=True)
		
		layout.itemS()
		
		row = layout.row(align=True)
		if not scene.use_preview_range:
			row.itemR(scene, "start_frame", text="Start")
			row.itemR(scene, "end_frame", text="End")
		else:
			row.itemR(scene, "preview_range_start_frame", text="Start")
			row.itemR(scene, "preview_range_end_frame", text="End")
		
		layout.itemS()
		layout.itemR(scene, "current_frame")
		
		layout.itemS()
		
		# XXX: Pause Button
		row = layout.row(align=True)
		row.itemO("screen.frame_jump", text="", icon='ICON_REW')
		row.itemO("screen.keyframe_jump", text="", icon='ICON_PREV_KEYFRAME')
		row.item_booleanO("screen.animation_play", "reverse", True, text="", icon='ICON_PLAY_REVERSE')
		row.itemO("screen.animation_play", text="", icon='ICON_PLAY')
		row.item_booleanO("screen.keyframe_jump", "next", True, text="", icon='ICON_NEXT_KEYFRAME')
		row.item_booleanO("screen.frame_jump", "end", True, text="", icon='ICON_FF')

"""
class TIME_MT_view(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout
		
		st = context.space_data
		
class TIME_MT_frame(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "Frame"

	def draw(self, context):
		layout = self.layout
		
		st = context.space_data

class TIME_MT_playback(bpy.types.Menu):
	__space_type__ = "TEXT_EDITOR"
	__label__ = "Playback"

	def draw(self, context):
		layout = self.layout
		
		st = context.space_data
"""

bpy.types.register(TIME_HT_header)
#bpy.types.register(TIME_MT_view)
#bpy.types.register(TIME_MT_frame)
#bpy.types.register(TIME_MT_playback)
