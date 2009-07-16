
import bpy

class INFO_HT_header(bpy.types.Header):
	__space_type__ = "USER_PREFERENCES"
	__idname__ = "INFO_HT_header"

	def draw(self, context):
		st = context.space_data
		layout = self.layout
		
		layout.template_header()

		if context.area.show_menus:
			row = layout.row()
			row.itemM("INFO_MT_file")
			row.itemM("INFO_MT_add")
			row.itemM("INFO_MT_timeline")
			row.itemM("INFO_MT_game")
			row.itemM("INFO_MT_render")
			row.itemM("INFO_MT_help")

		layout.template_ID(context.window, "screen") #, new="SCREEN_OT_new", open="SCREEN_OT_unlink")
		layout.template_ID(context.screen, "scene") #, new="SCENE_OT_new", unlink="SCENE_OT_unlink")

		layout.itemS()

		layout.template_operator_search()
		layout.template_running_jobs()
			
class INFO_MT_file(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "File"

	def draw(self, context):
		layout = self.layout

		layout.operator_context = "EXEC_AREA"
		layout.itemO("WM_OT_read_homefile")
		layout.operator_context = "INVOKE_AREA"
		layout.itemO("WM_OT_open_mainfile")

		layout.itemS()

		layout.operator_context = "EXEC_AREA"
		layout.itemO("WM_OT_save_mainfile")
		layout.operator_context = "INVOKE_AREA"
		layout.itemO("WM_OT_save_as_mainfile")

		layout.itemS()

		layout.itemM("INFO_MT_file_external_data")

class INFO_MT_file_external_data(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "External Data"

	def draw(self, context):
		layout = self.layout

		layout.itemO("FILE_OT_pack_all", text="Pack into .blend file")
		layout.itemO("FILE_OT_unpack_all", text="Unpack into Files...")

		layout.itemS()

		layout.itemO("FILE_OT_make_paths_relative")
		layout.itemO("FILE_OT_make_paths_absolute")
		layout.itemO("FILE_OT_report_missing_files")
		layout.itemO("FILE_OT_find_missing_files")

class INFO_MT_add(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Add"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_MT_timeline(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Timeline"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_MT_game(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Game"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_MT_render(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Render"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_MT_help(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Help"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="Nothing yet")

class INFO_PT_tabs(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__no_header__ = True

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences

		layout.itemR(userpref, "active_section", expand=True)

class INFO_PT_view(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "View"
	__no_header__ = True

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'VIEW_CONTROLS')

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences
		view = userpref.view

		split = layout.split()
		col = split.column()
		col.itemL(text="Display:")
		col.itemR(view, "tooltips")
		col.itemR(view, "display_object_info", text="Object Info")
		col.itemR(view, "use_large_cursors")
		col.itemR(view, "show_view_name", text="View Name")
		col.itemR(view, "show_playback_fps", text="Playback FPS")
		col.itemR(view, "global_scene")
		col.itemR(view, "pin_floating_panels")
		col.itemR(view, "object_center_size")
		col.itemS()
		col.itemS()
		
		col.itemL(text="Menus:")
		col.itemR(view, "open_mouse_over")
		col.itemL(text="Menu Open Delay:")
		col.itemR(view, "open_toplevel_delay", text="Top Level")
		col.itemR(view, "open_sublevel_delay", text="Sub Level")
		
		
		col = split.column()
		
		col.itemL(text="View Manipulation:")
		col.itemR(view, "auto_depth")
		col.itemR(view, "global_pivot")
		col.itemR(view, "zoom_to_mouse")
		col.itemL(text="Zoom Style:")
		row = col.row()
		row.itemR(view, "viewport_zoom_style", expand=True)
		col.itemL(text="Orbit Style:")
		row = col.row()
		row.itemR(view, "view_rotation", expand=True)
		col.itemR(view, "perspective_orthographic_switch")
		col.itemR(view, "smooth_view")
		col.itemR(view, "rotation_angle")
		col.itemL(text="NDOF Device:")
		col.itemR(view, "ndof_pan_speed", text="Pan Speed")
		col.itemR(view, "ndof_rotate_speed", text="Orbit Speed")
		
		col = split.column()
		col.itemL(text="Snap:")
		col.itemR(view, "snap_translate", text="Translate")
		col.itemR(view, "snap_rotate", text="Rotate")
		col.itemR(view, "snap_scale", text="Scale")
		col.itemS()
		col.itemS()
		
		col.itemL(text="Mouse Buttons:")
		col.itemR(view, "left_mouse_button_select")
		col.itemR(view, "right_mouse_button_select")
		col.itemR(view, "emulate_3_button_mouse")
		col.itemR(view, "use_middle_mouse_paste")
		col.itemR(view, "middle_mouse_rotate")
		col.itemR(view, "middle_mouse_pan")
		col.itemR(view, "wheel_invert_zoom")
		col.itemR(view, "wheel_scroll_lines")
		
		
		col = split.column()
		#Axis
		col.itemL(text="Mini Axis:")
		col.itemR(view, "show_mini_axis")
		colsub = col.column()
		colsub.enabled = view.show_mini_axis
		colsub.itemR(view, "mini_axis_size")
		colsub.itemR(view, "mini_axis_brightness")
		col.itemS()
		col.itemS()
		#manipulator
		col.itemL(text="Manipulator:")
		col.itemR(view, "use_manipulator")
		colsub = col.column()
		colsub.enabled = view.use_manipulator
		colsub.itemR(view, "manipulator_size", text="Size")
		colsub.itemR(view, "manipulator_handle_size", text="Handle Size")
		colsub.itemR(view, "manipulator_hotspot", text="Hotspot")	
		col.itemS()
		col.itemS()
				
		col.itemL(text="Toolbox:")
		col.itemR(view, "use_column_layout")
		col.itemL(text="Open Toolbox Delay:")
		col.itemR(view, "open_left_mouse_delay", text="Hold LMB")
		col.itemR(view, "open_right_mouse_delay", text="Hold RMB")

		
class INFO_PT_edit(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Edit"
	__no_header__ = True

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'EDIT_METHODS')

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences
		edit = userpref.edit
	
		split = layout.split()
		col = split.column()
		#Materials
		col.itemL(text="Materials:")
		col.itemR(edit, "material_linked_object", text="Linked to Object")
		col.itemR(edit, "material_linked_obdata", text="Linked to ObData")
		col.itemS()
		col.itemS()
	
		#New Objects
		col.itemL(text="New Objects:")
		col.itemR(edit, "enter_edit_mode")
		col.itemR(edit, "align_to_view")
		col.itemS()
		col.itemS()
		
		#Tranform
		col.itemL(text="Transform:")
		col.itemR(edit, "drag_immediately")
		col.itemS()
		col.itemS()
		
		#undo
		col.itemL(text="Undo:")
		col.itemR(edit, "global_undo")
		col.itemR(edit, "undo_steps", text="Steps")
		col.itemR(edit, "undo_memory_limit", text="Memory Limit")
		
		col = split.column()
		#keying
		col.itemL(text="Keyframing:")
		col.itemR(edit, "use_visual_keying")
		col.itemR(edit, "new_interpolation_type")
		col.itemR(edit, "auto_keying_enable", text="Auto Keyframing")
		colsub = col.column()
		colsub.enabled = edit.auto_keying_enable
		row = colsub.row()
		row.itemR(edit, "auto_keying_mode", expand=True)
		colsub.itemR(edit, "auto_keyframe_insert_available", text="Only Insert Available")
		colsub.itemR(edit, "auto_keyframe_insert_needed", text="Only Insert Needed")
		col.itemS()
		col.itemS()
		#greasepencil
		col.itemL(text="Grease Pencil:")
		col.itemR(edit, "grease_pencil_manhattan_distance", text="Manhattan Distance")
		col.itemR(edit, "grease_pencil_euclidean_distance", text="Euclidean Distance")
		col.itemR(edit, "grease_pencil_smooth_stroke", text="Smooth Stroke")
		col.itemR(edit, "grease_pencil_simplify_stroke", text="Simplify Stroke")
		col.itemR(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
		col = split.column()
		#Diplicate
		col.itemL(text="Duplicate:")
		col.itemR(edit, "duplicate_mesh", text="Mesh")
		col.itemR(edit, "duplicate_surface", text="Surface")
		col.itemR(edit, "duplicate_curve", text="Curve")
		col.itemR(edit, "duplicate_text", text="Text")
		col.itemR(edit, "duplicate_metaball", text="Metaball")
		col.itemR(edit, "duplicate_armature", text="Armature")
		col.itemR(edit, "duplicate_lamp", text="Lamp")
		col.itemR(edit, "duplicate_material", text="Material")
		col.itemR(edit, "duplicate_texture", text="Texture")
		col.itemR(edit, "duplicate_ipo", text="F-Curve")
		col.itemR(edit, "duplicate_action", text="Action")
		
class INFO_PT_system(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "System"
	__no_header__ = True

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'SYSTEM_OPENGL')

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences
		system = userpref.system
		lan = userpref.language
		
		split = layout.split()
		col = split.column()
		
		col.itemR(system, "emulate_numpad")	
		col.itemS()
		col.itemS()
		#Weight Colors
		col.itemL(text="Weight Colors:")
		col.itemR(system, "use_weight_color_range", text="Use Custom Range")
		col.itemR(system, "weight_color_range")
		col.itemS()
		col.itemS()
		
		#sequencer
		col.itemL(text="Sequencer:")
		col.itemR(system, "prefetch_frames")
		col.itemR(system, "memory_cache_limit")
		
		col = split.column()	
		
		#System
		col.itemL(text="System:")
		col.itemR(lan, "dpi")
		col.itemR(system, "enable_all_codecs")
		col.itemR(system, "auto_run_python_scripts")
		col.itemR(system, "frame_server_port")
		col.itemR(system, "game_sound")
		col.itemR(system, "filter_file_extensions")
		col.itemR(system, "hide_dot_files_datablocks")
		col.itemR(system, "audio_mixing_buffer")
		
		col = split.column()
		
		#OpenGL
		col.itemL(text="OpenGL:")
		col.itemR(system, "clip_alpha", slider=True)
		col.itemR(system, "use_mipmaps")
		col.itemL(text="Windom Draw Method:")
		row = col.row()
		row.itemR(system, "window_draw_method", expand=True)
		col.itemL(text="Textures:")
		col.itemR(system, "gl_texture_limit", text="Limit")
		col.itemR(system, "texture_time_out", text="Time Out")
		col.itemR(system, "texture_collection_rate", text="Collection Rate")		
		
class INFO_PT_filepaths(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "File Paths"
	__no_header__ = True

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'FILE_PATHS')

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences
		paths = userpref.filepaths
		
		split = layout.split()
		col = split.column()
		col.itemR(paths, "use_relative_paths")
		col.itemR(paths, "compress_file")
		col.itemR(paths, "fonts_directory")
		col.itemR(paths, "textures_directory")
		col.itemR(paths, "texture_plugin_directory")
		col.itemR(paths, "sequence_plugin_directory")
		col.itemR(paths, "render_output_directory")
		col.itemR(paths, "python_scripts_directory")
		col.itemR(paths, "sounds_directory")
		col.itemR(paths, "temporary_directory")

class INFO_PT_autosave(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Auto Save"
	__no_header__ = True

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'AUTO_SAVE')

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences
		save = userpref.autosave
		
		split = layout.split()
		col = split.column()
		col.itemR(save, "save_version")
		col.itemR(save, "recent_files")
		col.itemR(save, "save_preview_images")
		
		col = split.column()
		col.itemR(save, "auto_save_temporary_files")
		colsub = col.column()
		colsub.enabled = save.auto_save_temporary_files
		colsub.itemR(save, "auto_save_time")

class INFO_PT_language(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Language"
	__no_header__ = True

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'LANGUAGE_COLORS')

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences
		lan = userpref.language
		
		split = layout.split()
		col = split.column()
		
		col.itemR(lan, "language")
		col.itemR(lan, "translate_tooltips")
		col.itemR(lan, "translate_buttons")
		col.itemR(lan, "translate_toolbox")
		col.itemR(lan, "use_textured_fonts")
		
class INFO_PT_bottombar(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = " "
	__no_header__ = True

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences
	
		split = layout.split(percentage=0.8)
		split.itemL(text="")
		split.itemO("WM_OT_save_homefile", text="Save As Default")


bpy.types.register(INFO_HT_header)
bpy.types.register(INFO_MT_file)
bpy.types.register(INFO_MT_file_external_data)
bpy.types.register(INFO_MT_add)
bpy.types.register(INFO_MT_timeline)
bpy.types.register(INFO_MT_game)
bpy.types.register(INFO_MT_render)
bpy.types.register(INFO_MT_help)
bpy.types.register(INFO_PT_tabs)
bpy.types.register(INFO_PT_view)
bpy.types.register(INFO_PT_edit)
bpy.types.register(INFO_PT_system)
bpy.types.register(INFO_PT_filepaths)
bpy.types.register(INFO_PT_autosave)
bpy.types.register(INFO_PT_language)
bpy.types.register(INFO_PT_bottombar)

