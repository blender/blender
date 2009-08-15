
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
			row.itemM("TIME_MT_frame")
			row.itemM("TIME_MT_playback")

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

		layout.itemR(scene, "active_keyingset")
		
		row = layout.row(align=True)
		row.itemO("anim.insert_keyframe", text="", icon="ICON_KEY_HLT")
		row.itemO("anim.delete_keyframe", text="", icon="ICON_KEY_DEHLT")

"""
class TIME_MT_view(bpy.types.Menu):
	__space_type__ = "TIMELINE"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout
		
		st = context.space_data

"""

class TIME_MT_frame(bpy.types.Menu):
	__space_type__ = "TIMELINE"
	__label__ = "Frame"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("marker.add", text="Add Marker")
		layout.itemO("marker.duplicate", text="Duplicate Marker")
		layout.itemO("marker.move", text="Grab/Move Marker")
		layout.itemO("marker.delete", text="Delete Marker")
		layout.itemL(text="ToDo: Name Marker")
		
		layout.itemS()
		
		layout.itemO("time.start_frame_set")
		layout.itemO("time.end_frame_set")


class TIME_MT_playback(bpy.types.Menu):
	__space_type__ = "TIMELINE"
	__label__ = "Playback"

	def draw(self, context):
		layout = self.layout
		
		st = context.space_data
		time = st.XXX #ToDo
		
		layout.itemR(time, "play_top_left")
		layout.itemR(time, "play_all_3d")
		layout.itemR(time, "play_anim")
		layout.itemR(time, "play_buttons")
		layout.itemR(time, "play_image")
		layout.itemR(time, "play_sequencer")
		
		layout.itemS()
		
		layout.itemR(time, "continue_physics")


bpy.types.register(TIME_HT_header)
#bpy.types.register(TIME_MT_view)
bpy.types.register(TIME_MT_frame)
bpy.types.register(TIME_MT_playback)
