
import bpy

class INFO_HT_header(bpy.types.Header):
	__space_type__ = "USER_PREFERENCES"
	__idname__ = "INFO_HT_header"

	def draw(self, context):
		st = context.space_data
		rd = context.scene.render_data
		layout = self.layout
		
		layout.template_header()

		if context.area.show_menus:
			row = layout.row()
			row.itemM("INFO_MT_file")
			row.itemM("INFO_MT_add")
			if rd.use_game_engine:
				row.itemM("INFO_MT_game")
			else:
				row.itemM("INFO_MT_render")
			row.itemM("INFO_MT_help")

		layout.template_ID(context.window, "screen") #, new="screen.new", open="scene.unlink")
		layout.template_ID(context.screen, "scene") #, new="screen.new", unlink="scene.unlink")

		if rd.multiple_engines:
			layout.itemR(rd, "engine", text="")

		layout.itemS()

		layout.template_operator_search()
		layout.template_running_jobs()
			
class INFO_MT_file(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "File"

	def draw(self, context):
		layout = self.layout

		layout.operator_context = "EXEC_AREA"
		layout.itemO("wm.read_homefile", text="New")
		layout.operator_context = "INVOKE_AREA"
		layout.itemO("wm.open_mainfile", text="Open...")
		layout.item_menu_enumO("wm.open_recentfile", "file", text="Open Recent")
		layout.itemO("wm.recover_last_session")

		layout.itemS()

		layout.operator_context = "EXEC_AREA"
		layout.itemO("wm.save_mainfile", text="Save")
		layout.operator_context = "INVOKE_AREA"
		layout.itemO("wm.save_as_mainfile", text="Save As...")

		layout.itemS()

		layout.itemM("INFO_MT_file_import")
		layout.itemM("INFO_MT_file_export")

		layout.itemS()

		layout.itemM("INFO_MT_file_external_data")

		layout.itemS()

		layout.operator_context = "EXEC_AREA"
		layout.itemO("wm.exit_blender", text="Quit")

class INFO_MT_file_import(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Import"

	def draw(self, context):
		layout = self.layout

class INFO_MT_file_export(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Export"

	def draw(self, context):
		layout = self.layout

		layout.itemO("export.ply", text="PLY")

class INFO_MT_file_external_data(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "External Data"

	def draw(self, context):
		layout = self.layout

		layout.itemO("file.pack_all", text="Pack into .blend file")
		layout.itemO("file.unpack_all", text="Unpack into Files...")

		layout.itemS()

		layout.itemO("file.make_paths_relative")
		layout.itemO("file.make_paths_absolute")
		layout.itemO("file.report_missing_files")
		layout.itemO("file.find_missing_files")

class INFO_MT_add(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Add"

	def draw(self, context):
		layout = self.layout

		layout.operator_context = "EXEC_SCREEN"

		layout.item_menu_enumO( "OBJECT_OT_mesh_add", "type", text="Mesh", icon="ICON_OUTLINER_OB_MESH")
		layout.item_menu_enumO( "OBJECT_OT_curve_add", "type", text="Curve", icon="ICON_OUTLINER_OB_CURVE")
		layout.item_menu_enumO( "OBJECT_OT_surface_add", "type", text="Surface", icon="ICON_OUTLINER_OB_SURFACE")
		layout.item_enumO("OBJECT_OT_object_add", "type", "META", icon="ICON_OUTLINER_OB_META")
		layout.itemO("OBJECT_OT_text_add", text="Text", icon="ICON_OUTLINER_OB_FONT")

		layout.itemS()

		layout.itemO("OBJECT_OT_armature_add", text="Armature", icon="ICON_OUTLINER_OB_ARMATURE")
		layout.item_enumO("OBJECT_OT_object_add", "type", "LATTICE", icon="ICON_OUTLINER_OB_LATTICE")
		layout.item_enumO("OBJECT_OT_object_add", "type", "EMPTY", icon="ICON_OUTLINER_OB_EMPTY")

		layout.itemS()

		layout.item_enumO("OBJECT_OT_object_add", "type", "CAMERA", icon="ICON_OUTLINER_OB_CAMERA")
		layout.item_enumO("OBJECT_OT_object_add", "type", "LAMP", icon="ICON_OUTLINER_OB_LAMP")

class INFO_MT_game(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Game"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.game_start")

class INFO_MT_render(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Render"

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data

		layout.itemO("screen.render", text="Render Image")
		layout.item_booleanO("screen.render", "animation", True, text="Render Animation")

		layout.itemS()

		layout.itemO("screen.render_view_show")

class INFO_MT_help(bpy.types.Menu):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Help"

	def draw(self, context):
		layout = self.layout

		layout.itemO("help.manual")
		layout.itemO("help.release_logs")

		layout.itemS()

		layout.itemO("help.blender_website")
		layout.itemO("help.blender_eshop")
		layout.itemO("help.developer_community")
		layout.itemO("help.user_community")

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
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		colsplitcol.itemL(text="Display:")
		colsplitcol.itemR(view, "tooltips")
		colsplitcol.itemR(view, "display_object_info", text="Object Info")
		colsplitcol.itemR(view, "use_large_cursors")
		colsplitcol.itemR(view, "show_view_name", text="View Name")
		colsplitcol.itemR(view, "show_playback_fps", text="Playback FPS")
		colsplitcol.itemR(view, "global_scene")
		colsplitcol.itemR(view, "pin_floating_panels")
		colsplitcol.itemR(view, "object_center_size")
		colsplitcol.itemS()
		colsplitcol.itemS()
		colsplitcol.itemS()
		
		colsplitcol.itemR(view, "show_mini_axis")
		colsub = colsplitcol.column()
		colsub.enabled = view.show_mini_axis
		colsub.itemR(view, "mini_axis_size")
		colsub.itemR(view, "mini_axis_brightness")
		

		
		
		col = split.column()
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		colsplitcol.itemL(text="View Manipulation:")
		colsplitcol.itemR(view, "auto_depth")
		colsplitcol.itemR(view, "global_pivot")
		colsplitcol.itemR(view, "zoom_to_mouse")
		colsplitcol.itemR(view, "rotate_around_selection")
		colsplitcol.itemL(text="Zoom Style:")
		row = colsplitcol.row()
		row.itemR(view, "viewport_zoom_style", expand=True)
		colsplitcol.itemL(text="Orbit Style:")
		row = colsplitcol.row()
		row.itemR(view, "view_rotation", expand=True)
		colsplitcol.itemR(view, "perspective_orthographic_switch")
		colsplitcol.itemR(view, "smooth_view")
		colsplitcol.itemR(view, "rotation_angle")
		colsplitcol.itemL(text="NDOF Device:")
		colsplitcol.itemR(view, "ndof_pan_speed", text="Pan Speed")
		colsplitcol.itemR(view, "ndof_rotate_speed", text="Orbit Speed")
		
		col = split.column()
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		colsplitcol.itemL(text="Mouse Buttons:")
		colsplitcol.itemR(view, "left_mouse_button_select")
		colsplitcol.itemR(view, "right_mouse_button_select")
		colsplitcol.itemR(view, "emulate_3_button_mouse")
		colsplitcol.itemR(view, "use_middle_mouse_paste")
		colsplitcol.itemR(view, "middle_mouse_rotate")
		colsplitcol.itemR(view, "middle_mouse_pan")
		colsplitcol.itemR(view, "wheel_invert_zoom")
		colsplitcol.itemR(view, "wheel_scroll_lines")
		colsplitcol.itemS()
		colsplitcol.itemS()
		colsplitcol.itemS()
		
		colsplitcol.itemL(text="Menus:")
		colsplitcol.itemR(view, "open_mouse_over")
		colsplitcol.itemL(text="Menu Open Delay:")
		colsplitcol.itemR(view, "open_toplevel_delay", text="Top Level")
		colsplitcol.itemR(view, "open_sublevel_delay", text="Sub Level")

		
		col = split.column()
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		#manipulator
		colsplitcol.itemR(view, "use_manipulator")
		colsub = colsplitcol.column()
		colsub.enabled = view.use_manipulator
		colsub.itemR(view, "manipulator_size", text="Size")
		colsub.itemR(view, "manipulator_handle_size", text="Handle Size")
		colsub.itemR(view, "manipulator_hotspot", text="Hotspot")	
		colsplitcol.itemS()
		colsplitcol.itemS()
		colsplitcol.itemS()
				
		colsplitcol.itemL(text="Toolbox:")
		colsplitcol.itemR(view, "use_column_layout")
		colsplitcol.itemL(text="Open Toolbox Delay:")
		colsplitcol.itemR(view, "open_left_mouse_delay", text="Hold LMB")
		colsplitcol.itemR(view, "open_right_mouse_delay", text="Hold RMB")

		
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
		view = userpref.view
		
		split = layout.split()
		col = split.column()
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()

		colsplitcol.itemL(text="Materials:")
		colsplitcol.itemR(edit, "material_linked_object", text="Linked to Object")
		colsplitcol.itemR(edit, "material_linked_obdata", text="Linked to ObData")
		colsplitcol.itemS()
		colsplitcol.itemS()
		colsplitcol.itemS()
		
		colsplitcol.itemL(text="New Objects:")
		colsplitcol.itemR(edit, "enter_edit_mode")
		colsplitcol.itemR(edit, "align_to_view")
		colsplitcol.itemS()
		colsplitcol.itemS()
		colsplitcol.itemS()
		
		colsplitcol.itemL(text="Transform:")
		colsplitcol.itemR(edit, "drag_immediately")
		
		col = split.column()
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		colsplitcol.itemL(text="Snap:")
		colsplitcol.itemR(edit, "snap_translate", text="Translate")
		colsplitcol.itemR(edit, "snap_rotate", text="Rotate")
		colsplitcol.itemR(edit, "snap_scale", text="Scale")
		colsplitcol.itemS()
		colsplitcol.itemS()
		colsplitcol.itemS()
		
		colsplitcol.itemL(text="Grease Pencil:")
		colsplitcol.itemR(edit, "grease_pencil_manhattan_distance", text="Manhattan Distance")
		colsplitcol.itemR(edit, "grease_pencil_euclidean_distance", text="Euclidean Distance")
		colsplitcol.itemR(edit, "grease_pencil_smooth_stroke", text="Smooth Stroke")
		colsplitcol.itemR(edit, "grease_pencil_simplify_stroke", text="Simplify Stroke")
		colsplitcol.itemR(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
		
		
		col = split.column()
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()

		colsplitcol.itemL(text="Keyframing:")
		colsplitcol.itemR(edit, "use_visual_keying")
		colsplitcol.itemR(edit, "new_interpolation_type")
		colsplitcol.itemR(edit, "auto_keying_enable", text="Auto Keyframing")
		colsub = colsplitcol.column()
		colsub.enabled = edit.auto_keying_enable
		row = colsub.row()
		row.itemR(edit, "auto_keying_mode", expand=True)
		colsub.itemR(edit, "auto_keyframe_insert_available", text="Only Insert Available")
		colsub.itemR(edit, "auto_keyframe_insert_needed", text="Only Insert Needed")
		colsplitcol.itemS()
		colsplitcol.itemS()
		colsplitcol.itemS()
		
		colsplitcol.itemL(text="Undo:")
		colsplitcol.itemR(edit, "global_undo")
		colsplitcol.itemR(edit, "undo_steps", text="Steps")
		colsplitcol.itemR(edit, "undo_memory_limit", text="Memory Limit")
		colsplitcol.itemS()
		colsplitcol.itemS()
		colsplitcol.itemS()

		col = split.column()
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		colsplitcol.itemL(text="Duplicate:")
		colsplitcol.itemR(edit, "duplicate_mesh", text="Mesh")
		colsplitcol.itemR(edit, "duplicate_surface", text="Surface")
		colsplitcol.itemR(edit, "duplicate_curve", text="Curve")
		colsplitcol.itemR(edit, "duplicate_text", text="Text")
		colsplitcol.itemR(edit, "duplicate_metaball", text="Metaball")
		colsplitcol.itemR(edit, "duplicate_armature", text="Armature")
		colsplitcol.itemR(edit, "duplicate_lamp", text="Lamp")
		colsplitcol.itemR(edit, "duplicate_material", text="Material")
		colsplitcol.itemR(edit, "duplicate_texture", text="Texture")
		colsplitcol.itemR(edit, "duplicate_ipo", text="F-Curve")
		colsplitcol.itemR(edit, "duplicate_action", text="Action")
		
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
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		colsplitcol.itemR(system, "emulate_numpad")	
		colsplitcol.itemS()
		colsplitcol.itemS()
		#Weight Colors
		colsplitcol.itemL(text="Weight Colors:")
		colsplitcol.itemR(system, "use_weight_color_range", text="Use Custom Range")
		colsplitcol.itemR(system, "weight_color_range")
		colsplitcol.itemS()
		colsplitcol.itemS()
		
		#sequencer
		colsplitcol.itemL(text="Sequencer:")
		colsplitcol.itemR(system, "prefetch_frames")
		colsplitcol.itemR(system, "memory_cache_limit")
		
		col = split.column()	
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		#System
		colsplitcol.itemL(text="System:")
		colsplitcol.itemR(lan, "dpi")
		colsplitcol.itemR(system, "enable_all_codecs")
		colsplitcol.itemR(system, "auto_run_python_scripts")
		colsplitcol.itemR(system, "frame_server_port")
		colsplitcol.itemR(system, "game_sound")
		colsplitcol.itemR(system, "filter_file_extensions")
		colsplitcol.itemR(system, "hide_dot_files_datablocks")
		colsplitcol.itemR(system, "audio_mixing_buffer")
		
		col = split.column()
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		#OpenGL
		colsplitcol.itemL(text="OpenGL:")
		colsplitcol.itemR(system, "clip_alpha", slider=True)
		colsplitcol.itemR(system, "use_mipmaps")
		colsplitcol.itemL(text="Windom Draw Method:")
		row = colsplitcol.row()
		row.itemR(system, "window_draw_method", expand=True)
		colsplitcol.itemL(text="Textures:")
		colsplitcol.itemR(system, "gl_texture_limit", text="Limit")
		colsplitcol.itemR(system, "texture_time_out", text="Time Out")
		colsplitcol.itemR(system, "texture_collection_rate", text="Collection Rate")		
		
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
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		colsplitcol.itemR(save, "save_version")
		colsplitcol.itemR(save, "recent_files")
		colsplitcol.itemR(save, "save_preview_images")
		
		col = split.column()
		colsplit = col.split(percentage=0.8)
		colsplitcol = colsplit.column()
		colsplitcol.itemR(save, "auto_save_temporary_files")
		colsub = colsplitcol.column()
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
		split.itemO("wm.save_homefile", text="Save As Default")

bpy.types.register(INFO_HT_header)
bpy.types.register(INFO_MT_file)
bpy.types.register(INFO_MT_file_import)
bpy.types.register(INFO_MT_file_export)
bpy.types.register(INFO_MT_file_external_data)
bpy.types.register(INFO_MT_add)
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

# Help operators

import bpy_ops # XXX - should not need to do this
del bpy_ops

class HelpOperator(bpy.types.Operator):
	def execute(self, context):
		try: import webbrowser
		except: webbrowser = None

		if webbrowser:
			webbrowser.open(self.__URL__)
		else:
			raise Exception("Operator requires a full Python installation")

		return ('FINISHED',)

class HELP_OT_manual(HelpOperator):
	__idname__ = "help.manual"
	__label__ = "Manual"
	__URL__ = 'http://wiki.blender.org/index.php/Manual'

class HELP_OT_release_logs(HelpOperator):
	__idname__ = "help.release_logs"
	__label__ = "Release Logs"
	__URL__ = 'http://www.blender.org/development/release-logs/'

class HELP_OT_blender_website(HelpOperator):
	__idname__ = "help.blender_website"
	__label__ = "Blender Website"
	__URL__ = 'http://www.blender.org/'

class HELP_OT_blender_eshop(HelpOperator):
	__idname__ = "help.blender_eshop"
	__label__ = "Blender e-Shop"
	__URL__ = 'http://www.blender3d.org/e-shop'

class HELP_OT_developer_community(HelpOperator):
	__idname__ = "help.developer_community"
	__label__ = "Developer Community"
	__URL__ = 'http://www.blender.org/community/get-involved/'

class HELP_OT_user_community(HelpOperator):
	__idname__ = "help.user_community"
	__label__ = "User Community"
	__URL__ = 'http://www.blender.org/community/user-community/'

bpy.ops.add(HELP_OT_manual)
bpy.ops.add(HELP_OT_release_logs)
bpy.ops.add(HELP_OT_blender_website)
bpy.ops.add(HELP_OT_blender_eshop)
bpy.ops.add(HELP_OT_developer_community)
bpy.ops.add(HELP_OT_user_community)

