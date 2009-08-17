
import bpy

class INFO_HT_header(bpy.types.Header):
	__space_type__ = "USER_PREFERENCES"

	def draw(self, context):
		layout = self.layout
		
		st = context.space_data
		rd = context.scene.render_data
		
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

		layout.template_ID(context.window, "screen", new="screen.new", unlink="screen.delete")
		layout.template_ID(context.screen, "scene", new="scene.new", unlink="scene.delete")

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
		layout.itemO("screen.userpref_show", text="User Preferences...")

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

		layout.item_menu_enumO( "OBJECT_OT_mesh_add", "type", text="Mesh", icon='ICON_OUTLINER_OB_MESH')
		layout.item_menu_enumO( "OBJECT_OT_curve_add", "type", text="Curve", icon='ICON_OUTLINER_OB_CURVE')
		layout.item_menu_enumO( "OBJECT_OT_surface_add", "type", text="Surface", icon='ICON_OUTLINER_OB_SURFACE')
		layout.item_menu_enumO( "OBJECT_OT_metaball_add", "type", 'META', icon='ICON_OUTLINER_OB_META')
		layout.itemO("OBJECT_OT_text_add", text="Text", icon='ICON_OUTLINER_OB_FONT')

		layout.itemS()

		layout.itemO("OBJECT_OT_armature_add", text="Armature", icon='ICON_OUTLINER_OB_ARMATURE')
		layout.item_enumO("OBJECT_OT_object_add", "type", 'LATTICE', icon='ICON_OUTLINER_OB_LATTICE')
		layout.item_enumO("OBJECT_OT_object_add", "type", 'EMPTY', icon='ICON_OUTLINER_OB_EMPTY')

		layout.itemS()

		layout.item_enumO("OBJECT_OT_object_add", "type", 'CAMERA', icon='ICON_OUTLINER_OB_CAMERA')
		layout.item_enumO("OBJECT_OT_object_add", "type", 'LAMP', icon='ICON_OUTLINER_OB_LAMP')

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
	__show_header__ = False

	def draw(self, context):
		layout = self.layout
		
		userpref = context.user_preferences

		layout.itemR(userpref, "active_section", expand=True)

class INFO_PT_view(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "View"
	__show_header__ = False

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'VIEW_CONTROLS')

	def draw(self, context):
		layout = self.layout
		
		userpref = context.user_preferences
		view = userpref.view

		split = layout.split()
		
		col = split.column()
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		sub1.itemL(text="Display:")
		sub1.itemR(view, "tooltips")
		sub1.itemR(view, "display_object_info", text="Object Info")
		sub1.itemR(view, "use_large_cursors")
		sub1.itemR(view, "show_view_name", text="View Name")
		sub1.itemR(view, "show_playback_fps", text="Playback FPS")
		sub1.itemR(view, "global_scene")
		sub1.itemR(view, "pin_floating_panels")
		sub1.itemR(view, "object_center_size")
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		sub1.itemR(view, "show_mini_axis")
		sub2 = sub1.column()
		sub2.enabled = view.show_mini_axis
		sub2.itemR(view, "mini_axis_size")
		sub2.itemR(view, "mini_axis_brightness")
		
		col = split.column()
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		sub1.itemL(text="View Manipulation:")
		sub1.itemR(view, "auto_depth")
		sub1.itemR(view, "global_pivot")
		sub1.itemR(view, "zoom_to_mouse")
		sub1.itemR(view, "rotate_around_selection")
		sub1.itemS()
		sub1.itemL(text="Zoom Style:")
		sub1.row().itemR(view, "viewport_zoom_style", expand=True)
		sub1.itemL(text="Orbit Style:")
		sub1.row().itemR(view, "view_rotation", expand=True)
		sub1.itemR(view, "perspective_orthographic_switch")
		sub1.itemR(view, "smooth_view")
		sub1.itemR(view, "rotation_angle")
		sub1.itemS()
		sub1.itemL(text="NDOF Device:")
		sub1.itemR(view, "ndof_pan_speed", text="Pan Speed")
		sub1.itemR(view, "ndof_rotate_speed", text="Orbit Speed")
		
		col = split.column()
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		sub1.itemL(text="Mouse Buttons:")
		sub1.itemR(view, "left_mouse_button_select")
		sub1.itemR(view, "right_mouse_button_select")
		sub1.itemR(view, "emulate_3_button_mouse")
		sub1.itemR(view, "use_middle_mouse_paste")
		sub1.itemR(view, "middle_mouse_rotate")
		sub1.itemR(view, "middle_mouse_pan")
		sub1.itemR(view, "wheel_invert_zoom")
		sub1.itemR(view, "wheel_scroll_lines")
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		sub1.itemL(text="Menus:")
		sub1.itemR(view, "open_mouse_over")
		sub1.itemL(text="Menu Open Delay:")
		sub1.itemR(view, "open_toplevel_delay", text="Top Level")
		sub1.itemR(view, "open_sublevel_delay", text="Sub Level")

		col = split.column()
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		#manipulator
		sub1.itemR(view, "use_manipulator")
		sub2 = sub1.column()
		sub2.enabled = view.use_manipulator
		sub2.itemR(view, "manipulator_size", text="Size")
		sub2.itemR(view, "manipulator_handle_size", text="Handle Size")
		sub2.itemR(view, "manipulator_hotspot", text="Hotspot")	
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()			
		sub1.itemL(text="Toolbox:")
		sub1.itemR(view, "use_column_layout")
		sub1.itemL(text="Open Toolbox Delay:")
		sub1.itemR(view, "open_left_mouse_delay", text="Hold LMB")
		sub1.itemR(view, "open_right_mouse_delay", text="Hold RMB")

class INFO_PT_edit(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Edit"
	__show_header__ = False

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
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		sub1.itemL(text="Materials:")
		sub1.itemR(edit, "material_linked_object", text="Linked to Object")
		sub1.itemR(edit, "material_linked_obdata", text="Linked to ObData")
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		sub1.itemL(text="New Objects:")
		sub1.itemR(edit, "enter_edit_mode")
		sub1.itemR(edit, "align_to_view")
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		sub1.itemL(text="Transform:")
		sub1.itemR(edit, "drag_immediately")

		col = split.column()
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		sub1.itemL(text="Snap:")
		sub1.itemR(edit, "snap_translate", text="Translate")
		sub1.itemR(edit, "snap_rotate", text="Rotate")
		sub1.itemR(edit, "snap_scale", text="Scale")
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		sub1.itemL(text="Grease Pencil:")
		sub1.itemR(edit, "grease_pencil_manhattan_distance", text="Manhattan Distance")
		sub1.itemR(edit, "grease_pencil_euclidean_distance", text="Euclidean Distance")
		sub1.itemR(edit, "grease_pencil_smooth_stroke", text="Smooth Stroke")
		# sub1.itemR(edit, "grease_pencil_simplify_stroke", text="Simplify Stroke")
		sub1.itemR(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
		
		col = split.column()
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		sub1.itemL(text="Keyframing:")
		sub1.itemR(edit, "use_visual_keying")
		sub1.itemR(edit, "new_interpolation_type")
		sub1.itemS()
		sub1.itemR(edit, "auto_keying_enable", text="Auto Keyframing")
		sub2 = sub1.column()
		sub2.enabled = edit.auto_keying_enable
		sub2.row().itemR(edit, "auto_keying_mode", expand=True)
		sub2.itemR(edit, "auto_keyframe_insert_available", text="Only Insert Available")
		sub2.itemR(edit, "auto_keyframe_insert_needed", text="Only Insert Needed")
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		sub1.itemL(text="Undo:")
		sub1.itemR(edit, "global_undo")
		sub1.itemR(edit, "undo_steps", text="Steps")
		sub1.itemR(edit, "undo_memory_limit", text="Memory Limit")
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()

		col = split.column()
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		sub1.itemL(text="Duplicate:")
		sub1.itemR(edit, "duplicate_mesh", text="Mesh")
		sub1.itemR(edit, "duplicate_surface", text="Surface")
		sub1.itemR(edit, "duplicate_curve", text="Curve")
		sub1.itemR(edit, "duplicate_text", text="Text")
		sub1.itemR(edit, "duplicate_metaball", text="Metaball")
		sub1.itemR(edit, "duplicate_armature", text="Armature")
		sub1.itemR(edit, "duplicate_lamp", text="Lamp")
		sub1.itemR(edit, "duplicate_material", text="Material")
		sub1.itemR(edit, "duplicate_texture", text="Texture")
		sub1.itemR(edit, "duplicate_ipo", text="F-Curve")
		sub1.itemR(edit, "duplicate_action", text="Action")
		
class INFO_PT_system(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "System"
	__show_header__ = False

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
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		sub1.itemR(system, "emulate_numpad")	
		sub1.itemS()
		sub1.itemS()
		
		#Weight Colors
		sub1.itemL(text="Weight Colors:")
		sub1.itemR(system, "use_weight_color_range", text="Use Custom Range")
		
		sub2 = sub1.column()
		sub2.active = system.use_weight_color_range
		sub2.template_color_ramp(system.weight_color_range, expand=True)
		sub1.itemS()
		sub1.itemS()
		
		#sequencer
		sub1.itemL(text="Sequencer:")
		sub1.itemR(system, "prefetch_frames")
		sub1.itemR(system, "memory_cache_limit")
		
		col = split.column()	
		sub = col.split(percentage=0.85)
		
		sub1 = sub .column()
		#System
		sub1.itemL(text="System:")
		sub1.itemR(lan, "dpi")
		sub1.itemR(system, "auto_run_python_scripts")
		sub1.itemR(system, "frame_server_port")
		sub1.itemR(system, "filter_file_extensions")
		sub1.itemR(system, "hide_dot_files_datablocks")
		sub1.itemR(lan, "scrollback", text="Console Scrollback")
		sub1.itemS()
		sub1.itemS()
		sub1.itemL(text="Sound:")
		sub1.itemR(system, "audio_device")
		sub2 = sub1.column()
		sub2.active = system.audio_device != 'AUDIO_DEVICE_NULL'
		sub2.itemR(system, "enable_all_codecs")
		sub2.itemR(system, "game_sound")
		sub2.itemR(system, "audio_channels")
		sub2.itemR(system, "audio_mixing_buffer")
		sub2.itemR(system, "audio_sample_rate")
		sub2.itemR(system, "audio_sample_format")
		
		col = split.column()
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		#OpenGL
		sub1.itemL(text="OpenGL:")
		sub1.itemR(system, "clip_alpha", slider=True)
		sub1.itemR(system, "use_mipmaps")
		sub1.itemL(text="Window Draw Method:")
		sub1.row().itemR(system, "window_draw_method", expand=True)
		sub1.itemL(text="Textures:")
		sub1.itemR(system, "gl_texture_limit", text="Limit Size")
		sub1.itemR(system, "texture_time_out", text="Time Out")
		sub1.itemR(system, "texture_collection_rate", text="Collection Rate")		
		
class INFO_PT_filepaths(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "File Paths"
	__show_header__ = False

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'FILE_PATHS')

	def draw(self, context):
		layout = self.layout
		
		userpref = context.user_preferences
		paths = userpref.filepaths
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="File Paths:")
		sub = col.split(percentage=0.3)
		
		sub.itemL(text="Fonts:")
		sub.itemR(paths, "fonts_directory", text="")
		sub = col.split(percentage=0.3)
		sub.itemL(text="Textures:")
		sub.itemR(paths, "textures_directory", text="")
		sub = col.split(percentage=0.3)
		sub.itemL(text="Texture Plugins:")
		sub.itemR(paths, "texture_plugin_directory", text="")
		sub = col.split(percentage=0.3)
		sub.itemL(text="Sequence Plugins:")
		sub.itemR(paths, "sequence_plugin_directory", text="")
		sub = col.split(percentage=0.3)
		sub.itemL(text="Render Output:")
		sub.itemR(paths, "render_output_directory", text="")
		sub = col.split(percentage=0.3)
		sub.itemL(text="Scripts:")
		sub.itemR(paths, "python_scripts_directory", text="")
		sub = col.split(percentage=0.3)
		sub.itemL(text="Sounds:")
		sub.itemR(paths, "sounds_directory", text="")
		sub = col.split(percentage=0.3)
		sub.itemL(text="Temp:")
		sub.itemR(paths, "temporary_directory", text="")
		
		col = split.column()
		sub = col.split(percentage=0.2)
		sub1 = sub.column()
		sub2 = sub.column()
		sub2.itemL(text="Save & Load:")
		sub2.itemR(paths, "use_relative_paths")
		sub2.itemR(paths, "compress_file")
		sub2.itemL(text="Auto Save:")
		sub2.itemR(paths, "save_version")
		sub2.itemR(paths, "recent_files")
		sub2.itemR(paths, "save_preview_images")
		sub2.itemR(paths, "auto_save_temporary_files")
		sub3 = sub2.column()
		sub3.enabled = paths.auto_save_temporary_files
		sub3.itemR(paths, "auto_save_time")

class INFO_PT_language(bpy.types.Panel):
	__space_type__ = "USER_PREFERENCES"
	__label__ = "Language"
	__show_header__ = False

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
	__show_header__ = False

	def draw(self, context):
		layout = self.layout
		userpref = context.user_preferences
	
		split = layout.split(percentage=0.8)
		split.itemL(text="")
		layout.operator_context = "EXEC_AREA"
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
