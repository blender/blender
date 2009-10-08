 
import bpy

class USERPREF_HT_header(bpy.types.Header):
	__space_type__ = 'USER_PREFERENCES'

	def draw(self, context):
		layout = self.layout
		layout.template_header(menus=False)
		
		userpref = context.user_preferences
	
		layout.operator_context = "EXEC_AREA"
		layout.itemO("wm.save_homefile", text="Save As Default")
			
class USERPREF_MT_view(bpy.types.Menu):
	__space_type__ = 'USER_PREFERENCES'
	__label__ = "View"

	def draw(self, context):
		layout = self.layout

class USERPREF_PT_tabs(bpy.types.Panel):
	__space_type__ = 'USER_PREFERENCES'
	__show_header__ = False

	def draw(self, context):
		layout = self.layout
		
		userpref = context.user_preferences

		layout.itemR(userpref, "active_section", expand=True)

class USERPREF_PT_view(bpy.types.Panel):
	__space_type__ = 'USER_PREFERENCES'
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
		sub1.itemR(view, "show_mini_axis", text="Display Mini Axis")
		sub2 = sub1.column()
		sub2.enabled = view.show_mini_axis
		sub2.itemR(view, "mini_axis_size", text="Size")
		sub2.itemR(view, "mini_axis_brightness", text="Brightness")
		
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
		
		sub2 = sub1.column()
		sub2.enabled = (view.select_mouse == 'RIGHT')
		sub2.itemR(view, "emulate_3_button_mouse")
		sub1.itemL(text="Select With:")
		sub1.row().itemR(view, "select_mouse", expand=True)
		sub1.itemL(text="Middle Mouse:")
		sub1.row().itemR(view, "middle_mouse", expand=True)
		sub1.itemR(view, "use_middle_mouse_paste")
		sub1.itemL(text="Mouse Wheel:")
		sub1.itemR(view, "wheel_invert_zoom", text="Invert Zoom")
		sub1.itemR(view, "wheel_scroll_lines", text="Scroll Lines")
		sub1.itemL(text="Mouse Motion:")
		sub1.itemR(view, "continuous_mouse", text="Continuous Grab")
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

class USERPREF_PT_edit(bpy.types.Panel):
	__space_type__ = 'USER_PREFERENCES'
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
		sub1.itemL(text="Link Materials To:")
		sub1.row().itemR(edit, "material_link", expand=True)
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		sub1.itemL(text="New Objects:")
		sub1.itemR(edit, "enter_edit_mode")
		sub1.itemL(text="Align To:")
		sub1.row().itemR(edit, "object_align", expand=True)
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
		# sub1.itemR(edit, "grease_pencil_simplify_stroke", text="Simplify Stroke")
		sub1.itemR(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
		sub1.itemR(edit, "grease_pencil_smooth_stroke", text="Smooth Stroke")
		
		col = split.column()
		sub = col.split(percentage=0.85)
		
		sub1 = sub.column()
		sub1.itemL(text="Keyframing:")
		sub1.itemR(edit, "use_visual_keying")
		sub1.itemR(edit, "keyframe_insert_needed", text="Only Insert Needed")
		sub1.itemS()
		sub1.itemL(text="New F-Curve Defaults:")
		sub1.itemR(edit, "new_interpolation_type", text="Interpolation")
		sub1.itemS()
		sub1.itemR(edit, "auto_keying_enable", text="Auto Keyframing:")
		sub2 = sub1.column()
		sub2.active = edit.auto_keying_enable
		sub2.itemR(edit, "auto_keyframe_insert_keyingset", text="Only Insert for Keying Set")
		sub2.itemR(edit, "auto_keyframe_insert_available", text="Only Insert Available")
		
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
		sub1.itemL(text="Duplicate Data:")
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
		sub1.itemR(edit, "duplicate_particle", text="Particle")
		
class USERPREF_PT_system(bpy.types.Panel):
	__space_type__ = 'USER_PREFERENCES'
	__label__ = "System"
	__show_header__ = False

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'SYSTEM_OPENGL')

	def draw(self, context):
		layout = self.layout
		
		userpref = context.user_preferences
		system = userpref.system
		
		split = layout.split()
		
		col = split.column()
		sub = col.split(percentage=0.9)
		
		sub1 = sub.column()
		sub1.itemL(text="General:")
		sub1.itemR(system, "dpi")
		sub1.itemR(system, "frame_server_port")
		sub1.itemR(system, "scrollback", text="Console Scrollback")
		sub1.itemR(system, "emulate_numpad")
		sub1.itemR(system, "auto_run_python_scripts")
		
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		
		sub1.itemL(text="Sound:")
		sub1.row().itemR(system, "audio_device", expand=True)
		sub2 = sub1.column()
		sub2.active = system.audio_device != 'AUDIO_DEVICE_NULL'
		sub2.itemR(system, "enable_all_codecs")
		sub2.itemR(system, "game_sound")
		sub2.itemR(system, "audio_channels", text="Channels")
		sub2.itemR(system, "audio_mixing_buffer", text="Mixing Buffer")
		sub2.itemR(system, "audio_sample_rate", text="Sample Rate")
		sub2.itemR(system, "audio_sample_format", text="Sample Format")
		
		col = split.column()	
		sub = col.split(percentage=0.9)
		
		sub1 = sub .column()
		sub1.itemL(text="Weight Colors:")
		sub1.itemR(system, "use_weight_color_range", text="Use Custom Range")
		sub2 = sub1.column()
		sub2.active = system.use_weight_color_range
		sub2.template_color_ramp(system, "weight_color_range", expand=True)
		
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		
		sub1.itemR(system, "language")
		sub1.itemL(text="Translate:")
		sub1.itemR(system, "translate_tooltips", text="Tooltips")
		sub1.itemR(system, "translate_buttons", text="Labels")
		sub1.itemR(system, "translate_toolbox", text="Toolbox")
		
		sub1.itemS()
		
		sub1.itemR(system, "use_textured_fonts")
		
		col = split.column()
		sub = col.split(percentage=0.9)
		
		sub1 = sub.column()

		sub1.itemL(text="OpenGL:")
		sub1.itemR(system, "clip_alpha", slider=True)
		sub1.itemR(system, "use_mipmaps")
		sub1.itemR(system, "use_vbos")
		sub1.itemL(text="Window Draw Method:")
		sub1.row().itemR(system, "window_draw_method", expand=True)
		sub1.itemL(text="Textures:")
		sub1.itemR(system, "gl_texture_limit", text="Limit Size")
		sub1.itemR(system, "texture_time_out", text="Time Out")
		sub1.itemR(system, "texture_collection_rate", text="Collection Rate")
		
		sub1.itemS()
		sub1.itemS()
		sub1.itemS()
		
		sub1.itemL(text="Sequencer:")
		sub1.itemR(system, "prefetch_frames")
		sub1.itemR(system, "memory_cache_limit")
		
class USERPREF_PT_filepaths(bpy.types.Panel):
	__space_type__ = 'USER_PREFERENCES'
	__label__ = "File Paths"
	__show_header__ = False

	def poll(self, context):
		userpref = context.user_preferences
		return (userpref.active_section == 'FILE_PATHS')

	def draw(self, context):
		layout = self.layout
		
		userpref = context.user_preferences
		paths = userpref.filepaths
		
		split = layout.split(percentage=0.6)
		
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
		sub2.itemR(paths, "load_ui")
		sub2.itemR(paths, "filter_file_extensions")
		sub2.itemR(paths, "hide_dot_files_datablocks")
		sub2.itemS()
		sub2.itemS()
		sub2.itemL(text="Auto Save:")
		sub2.itemR(paths, "save_version")
		sub2.itemR(paths, "recent_files")
		sub2.itemR(paths, "save_preview_images")
		sub2.itemR(paths, "auto_save_temporary_files")
		sub3 = sub2.column()
		sub3.enabled = paths.auto_save_temporary_files
		sub3.itemR(paths, "auto_save_time", text="Timer (mins)")

bpy.types.register(USERPREF_HT_header)
bpy.types.register(USERPREF_MT_view)
bpy.types.register(USERPREF_PT_tabs)
bpy.types.register(USERPREF_PT_view)
bpy.types.register(USERPREF_PT_edit)
bpy.types.register(USERPREF_PT_system)
bpy.types.register(USERPREF_PT_filepaths)

