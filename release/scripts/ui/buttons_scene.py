
import bpy

class SceneButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "scene"
	
	def poll(self, context):
		return (context.scene != None)

class RenderButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "scene"
	# COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here
	
	def poll(self, context):
		rd = context.scene.render_data
		return (rd.use_game_engine==False) and (rd.engine in self.COMPAT_ENGINES)

class SCENE_PT_render(RenderButtonsPanel):
	__label__ = "Render"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])
	
	def draw(self, context):
		layout = self.layout
		
		rd = context.scene.render_data

		row = layout.row()
		row.itemO("screen.render", text="Image", icon='ICON_RENDER_STILL')
		row.item_booleanO("screen.render", "animation", True, text="Animation", icon='ICON_RENDER_ANIMATION')

		layout.itemR(rd, "display_mode", text="Display")

class SCENE_PT_layers(RenderButtonsPanel):
	__label__ = "Layers"
	__default_closed__ = True
	COMPAT_ENGINES = set(['BLENDER_RENDER'])
	
	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		rd = scene.render_data

		row = layout.row()
		row.template_list(rd, "layers", rd, "active_layer_index", rows=2)

		col = row.column(align=True)
		col.itemO("scene.render_layer_add", icon='ICON_ZOOMIN', text="")
		col.itemO("scene.render_layer_remove", icon='ICON_ZOOMOUT', text="")

		rl = rd.layers[rd.active_layer_index]

		split = layout.split()
		
		col = split.column()
		col.itemR(scene, "visible_layers", text="Scene")
		col = split.column()
		col.itemR(rl, "visible_layers", text="Layer")

		layout.itemR(rl, "light_override", text="Light")
		layout.itemR(rl, "material_override", text="Material")
		
		layout.itemS()
		layout.itemL(text="Include:")
		
		split = layout.split()

		col = split.column()
		col.itemR(rl, "zmask")
		row = col.row()
		row.itemR(rl, "zmask_negate", text="Negate")
		row.active = rl.zmask
		col.itemR(rl, "all_z")

		col = split.column()
		col.itemR(rl, "solid")
		col.itemR(rl, "halo")
		col.itemR(rl, "ztransp")

		col = split.column()
		col.itemR(rl, "sky")
		col.itemR(rl, "edge")
		col.itemR(rl, "strand")

		if rl.zmask:
			split = layout.split()
			split.itemL(text="Zmask Layers:")
			split.column().itemR(rl, "zmask_layers", text="")
		
		layout.itemS()
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Passes:")
		col.itemR(rl, "pass_combined")
		col.itemR(rl, "pass_z")
		col.itemR(rl, "pass_vector")
		col.itemR(rl, "pass_normal")
		col.itemR(rl, "pass_uv")
		col.itemR(rl, "pass_mist")
		col.itemR(rl, "pass_object_index")

		col = split.column()
		col.itemL()
		col.itemR(rl, "pass_color")
		col.itemR(rl, "pass_diffuse")
		row = col.row()
		row.itemR(rl, "pass_specular")
		row.itemR(rl, "pass_specular_exclude", text="", icon='ICON_X')
		row = col.row()
		row.itemR(rl, "pass_shadow")
		row.itemR(rl, "pass_shadow_exclude", text="", icon='ICON_X')
		row = col.row()
		row.itemR(rl, "pass_ao")
		row.itemR(rl, "pass_ao_exclude", text="", icon='ICON_X')
		row = col.row()
		row.itemR(rl, "pass_reflection")
		row.itemR(rl, "pass_reflection_exclude", text="", icon='ICON_X')
		row = col.row()
		row.itemR(rl, "pass_refraction")
		row.itemR(rl, "pass_refraction_exclude", text="", icon='ICON_X')

class SCENE_PT_shading(RenderButtonsPanel):
	__label__ = "Shading"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw(self, context):
		layout = self.layout
		
		rd = context.scene.render_data
		
		split = layout.split()
		
		col = split.column()
		col.itemR(rd, "render_textures", text="Textures")
		col.itemR(rd, "render_shadows", text="Shadows")
		col.itemR(rd, "render_sss", text="Subsurface Scattering")
		col.itemR(rd, "render_envmaps", text="Environment Map")
		
		col = split.column()
		col.itemR(rd, "render_raytracing", text="Ray Tracing")
		col.itemR(rd, "color_management")
		col.itemR(rd, "alpha_mode", text="Alpha")

class SCENE_PT_performance(RenderButtonsPanel):
	__label__ = "Performance"
	__default_closed__ = True
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw(self, context):
		layout = self.layout
		
		rd = context.scene.render_data

		split = layout.split()
		
		col = split.column(align=True)
		col.itemL(text="Threads:")
		col.row().itemR(rd, "threads_mode", expand=True)
		sub = col.column()
		sub.enabled = rd.threads_mode == 'THREADS_FIXED'
		sub.itemR(rd, "threads")
		col.itemL(text="Tiles:")
		col.itemR(rd, "parts_x", text="X")
		col.itemR(rd, "parts_y", text="Y")

		col = split.column()
		col.itemL(text="Memory:")
		sub = col.column()
		sub.itemR(rd, "save_buffers")
		sub.enabled = not rd.full_sample
		sub = col.column()
		sub.active = rd.use_compositing
		sub.itemR(rd, "free_image_textures")
		sub = col.column()
		sub.active = rd.render_raytracing
		sub.itemL(text="Acceleration structure:")
		sub.itemR(rd, "raytrace_structure", text="")
		if rd.raytrace_structure == "OCTREE":
			sub.itemR(rd, "octree_resolution", text="Resolution")
		else:
			sub.itemR(rd, "use_instances", text="Instances")
		sub.itemR(rd, "use_local_coords", text="Local Coordinates")

class SCENE_PT_post_processing(RenderButtonsPanel):
	__label__ = "Post Processing"
	__default_closed__ = True
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw(self, context):
		layout = self.layout
		
		rd = context.scene.render_data

		split = layout.split()

		col = split.column()
		col.itemR(rd, "use_compositing")
		col.itemR(rd, "use_sequencer")
		
		col = split.column()
		col.itemR(rd, "dither_intensity", text="Dither", slider=True)
		
		layout.itemS()
				
		split = layout.split()
		
		col = split.column()
		col.itemR(rd, "fields", text="Fields")
		sub = col.column()
		sub.active = rd.fields
		sub.row().itemR(rd, "field_order", expand=True)
		sub.itemR(rd, "fields_still", text="Still")
		
		col = split.column()
		col.itemR(rd, "edge")
		sub = col.column()
		sub.active = rd.edge
		sub.itemR(rd, "edge_threshold", text="Threshold", slider=True)
		sub.itemR(rd, "edge_color", text="")
		
class SCENE_PT_output(RenderButtonsPanel):
	__label__ = "Output"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw(self, context):
		layout = self.layout
		
		rd = context.scene.render_data
		
		layout.itemR(rd, "output_path", text="")

		split = layout.split()
		col = split.column()
		col.itemR(rd, "file_format", text="")
		col.row().itemR(rd, "color_mode", text="Color", expand=True)

		col = split.column()
		col.itemR(rd, "file_extensions")
		col.itemR(rd, "use_overwrite")
		col.itemR(rd, "use_placeholder")

		if rd.file_format in ('AVIJPEG', 'JPEG'):
			split = layout.split()
			split.itemR(rd, "quality", slider=True)
			
		elif rd.file_format == 'OPENEXR':
			split = layout.split()
			
			col = split.column()
			col.itemL(text="Codec:")
			col.itemR(rd, "exr_codec", text="")

			subsplit = split.split()
			col = subsplit.column()
			col.itemR(rd, "exr_half")
			col.itemR(rd, "exr_zbuf")
			col = subsplit.column()
			col.itemR(rd, "exr_preview")
		
		elif rd.file_format == 'JPEG2000':
			split = layout.split()
			col = split.column()
			col.itemL(text="Depth:")
			col.row().itemR(rd, "jpeg2k_depth", expand=True)

			col = split.column()
			col.itemR(rd, "jpeg2k_preset", text="")
			col.itemR(rd, "jpeg2k_ycc")
			
		elif rd.file_format in ('CINEON', 'DPX'):
			split = layout.split()
			col = split.column()
			col.itemR(rd, "cineon_log", text="Convert to Log")

			col = split.column(align=True)
			col.active = rd.cineon_log
			col.itemR(rd, "cineon_black", text="Black")
			col.itemR(rd, "cineon_white", text="White")
			col.itemR(rd, "cineon_gamma", text="Gamma")
			
		elif rd.file_format == 'TIFF':
			split = layout.split()
			split.itemR(rd, "tiff_bit")

class SCENE_PT_encoding(RenderButtonsPanel):
	__label__ = "Encoding"
	__default_closed__ = True
	COMPAT_ENGINES = set(['BLENDER_RENDER'])
	
	def poll(self, context):
		rd = context.scene.render_data
		return rd.file_format in ('FFMPEG', 'XVID', 'H264', 'THEORA')

	def draw(self, context):
		layout = self.layout
		
		rd = context.scene.render_data

		split = layout.split()
		
		split.itemR(rd, "ffmpeg_format")
		if rd.ffmpeg_format in ('AVI', 'QUICKTIME', 'MKV', 'OGG'):
			split.itemR(rd, "ffmpeg_codec")
		else:
			split.itemL()

		split = layout.split()
	
		col = split.column()
		col.itemR(rd, "ffmpeg_video_bitrate")
		col.itemL(text="Rate:")
		col.itemR(rd, "ffmpeg_minrate", text="Minimum")
		col.itemR(rd, "ffmpeg_maxrate", text="Maximum")
		col.itemR(rd, "ffmpeg_buffersize", text="Buffer")
		
		col = split.column()
		col.itemR(rd, "ffmpeg_gopsize")
		col.itemR(rd, "ffmpeg_autosplit")
		col.itemL(text="Mux:")
		col.itemR(rd, "ffmpeg_muxrate", text="Rate")
		col.itemR(rd, "ffmpeg_packetsize", text="Packet Size")
		
		row = layout.row()
		row.itemL(text="Audio:")
		row = layout.row()
		row.itemR(rd, "ffmpeg_audio_codec")
		
		split = layout.split()

		col = split.column()
		col.itemR(rd, "ffmpeg_audio_bitrate")
		col.itemR(rd, "ffmpeg_audio_mixrate")
		col = split.column()
		col.itemR(rd, "ffmpeg_multiplex_audio")
		col.itemR(rd, "ffmpeg_audio_volume")

class SCENE_PT_antialiasing(RenderButtonsPanel):
	__label__ = "Anti-Aliasing"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw_header(self, context):
		rd = context.scene.render_data

		self.layout.itemR(rd, "antialiasing", text="")

	def draw(self, context):
		layout = self.layout
		
		rd = context.scene.render_data

		layout.active = rd.antialiasing

		split = layout.split()
		
		col = split.column()
		col.row().itemR(rd, "antialiasing_samples", expand=True)
		col.itemR(rd, "full_sample")

		col = split.column()
		col.itemR(rd, "pixel_filter", text="")
		col.itemR(rd, "filter_size", text="Size", slider=True)
	
class SCENE_PT_dimensions(RenderButtonsPanel):
	__label__ = "Dimensions"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		rd = scene.render_data
		
		split = layout.split()
		
		col = split.column()
		sub = col.column(align=True)
		sub.itemL(text="Resolution:")
		sub.itemR(rd, "resolution_x", text="X")
		sub.itemR(rd, "resolution_y", text="Y")
		sub.itemR(rd, "resolution_percentage", text="")
		
		sub.itemL(text="Aspect Ratio:")
		sub.itemR(rd, "pixel_aspect_x", text="X")
		sub.itemR(rd, "pixel_aspect_y", text="Y")

		row = col.row()
		row.itemR(rd, "use_border", text="Border")
		rowsub = row.row()
		rowsub.active = rd.use_border
		rowsub.itemR(rd, "crop_to_border", text="Crop")
		
		col = split.column(align=True)
		col.itemL(text="Frame Range:")
		col.itemR(scene, "start_frame", text="Start")
		col.itemR(scene, "end_frame", text="End")
		col.itemR(scene, "frame_step", text="Step")
		
		col.itemL(text="Frame Rate:")
		col.itemR(rd, "fps")
		col.itemR(rd, "fps_base",text="/")

class SCENE_PT_stamp(RenderButtonsPanel):
	__label__ = "Stamp"
	__default_closed__ = True
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw_header(self, context):
		rd = context.scene.render_data

		self.layout.itemR(rd, "render_stamp", text="")

	def draw(self, context):
		layout = self.layout
		
		rd = context.scene.render_data

		layout.active = rd.render_stamp

		split = layout.split()
		
		col = split.column()
		col.itemR(rd, "stamp_time", text="Time")
		col.itemR(rd, "stamp_date", text="Date")
		col.itemR(rd, "stamp_frame", text="Frame")
		col.itemR(rd, "stamp_scene", text="Scene")
		col.itemR(rd, "stamp_camera", text="Camera")
		col.itemR(rd, "stamp_filename", text="Filename")
		col.itemR(rd, "stamp_marker", text="Marker")
		col.itemR(rd, "stamp_sequence_strip", text="Seq. Strip")

		col = split.column()
		col.active = rd.render_stamp
		col.itemR(rd, "stamp_foreground", slider=True)
		col.itemR(rd, "stamp_background", slider=True)
		col.itemR(rd, "stamp_font_size", text="Font Size")

		row = layout.split(percentage=0.2)
		row.itemR(rd, "stamp_note", text="Note")
		sub = row.row()
		sub.active = rd.stamp_note
		sub.itemR(rd, "stamp_note_text", text="")

class SCENE_PT_unit(RenderButtonsPanel):
	__label__ = "Units"
	__default_closed__ = True
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw(self, context):
		layout = self.layout
		
		unit = context.scene.unit_settings
		
		col = layout.column()
		col.row().itemR(unit, "system", expand=True)
		
		row = layout.row()
		row.active = (unit.system != 'NONE')
		row.itemR(unit, "scale_length", text="Scale")
		row.itemR(unit, "use_separate")
		
class SCENE_PT_keying_sets(SceneButtonsPanel):
	__label__ = "Keying Sets"
	__default_closed__ = True
	
	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		
		row = layout.row()
		row.itemL(text="Keying Sets:")
		
		row = layout.row()
		
		col = row.column()
		col.template_list(scene, "keying_sets", scene, "active_keying_set_index", rows=2)
		
		col = row.column(align=True)
		col.itemO("anim.keying_set_add", icon='ICON_ZOOMIN', text="")
		col.itemO("anim.keying_set_remove", icon='ICON_ZOOMOUT', text="")
		
		ks = scene.active_keying_set
		if ks:
			row = layout.row()
			
			col = row.column()
			col.itemR(ks, "name")
			col.itemR(ks, "absolute")
			
			col = row.column()
			col.itemL(text="Keyframing Settings:")
			col.itemR(ks, "insertkey_needed", text="Needed")
			col.itemR(ks, "insertkey_visual", text="Visual")
			
class SCENE_PT_keying_set_paths(SceneButtonsPanel):
	__label__ = "Active Keying Set"
	__default_closed__ = True
	
	def poll(self, context):
		return (context.scene != None) and (context.scene.active_keying_set != None)
	
	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		ks = scene.active_keying_set
		
		row = layout.row()
		row.itemL(text="Paths:")
		
		row = layout.row()
		
		col = row.column()
		col.template_list(ks, "paths", ks, "active_path_index", rows=2)
		
		col = row.column(align=True)
		col.itemO("anim.keying_set_path_add", icon='ICON_ZOOMIN', text="")
		col.itemO("anim.keying_set_path_remove", icon='ICON_ZOOMOUT', text="")
		
		ksp = ks.active_path
		if ksp:
			col = layout.column()
			col.itemL(text="Target:")
			col.template_any_ID(ksp, "id", "id_type")
			col.itemR(ksp, "rna_path")
			
			
			row = layout.row()
			
			col = row.column()
			col.itemL(text="Array Target:")
			col.itemR(ksp, "entire_array")
			if ksp.entire_array == False:
				col.itemR(ksp, "array_index")
				
			col = row.column()
			col.itemL(text="F-Curve Grouping:")
			col.itemR(ksp, "grouping")
			if ksp.grouping == 'NAMED':
				col.itemR(ksp, "group")
				
			
			

class SCENE_PT_physics(RenderButtonsPanel):
	__label__ = "Gravity"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw_header(self, context):
		self.layout.itemR(context.scene, "use_gravity", text="")

	def draw(self, context):
		layout = self.layout
		
		scene = context.scene

		layout.active = scene.use_gravity

		layout.itemR(scene, "gravity", text="")

		
bpy.types.register(SCENE_PT_render)
bpy.types.register(SCENE_PT_layers)
bpy.types.register(SCENE_PT_dimensions)
bpy.types.register(SCENE_PT_antialiasing)
bpy.types.register(SCENE_PT_shading)
bpy.types.register(SCENE_PT_output)
bpy.types.register(SCENE_PT_encoding)
bpy.types.register(SCENE_PT_performance)
bpy.types.register(SCENE_PT_post_processing)
bpy.types.register(SCENE_PT_stamp)
bpy.types.register(SCENE_PT_unit)
bpy.types.register(SCENE_PT_keying_sets)
bpy.types.register(SCENE_PT_keying_set_paths)
bpy.types.register(SCENE_PT_physics)
