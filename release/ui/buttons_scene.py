
import bpy

class RenderButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "scene"

class RENDER_PT_render(RenderButtonsPanel):
	__label__ = "Render"

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data

		row = layout.row()
		row.itemO("SCREEN_OT_render", text="Image", icon='ICON_IMAGE_COL')
		row.item_booleanO("SCREEN_OT_render", "anim", True, text="Animation", icon='ICON_SEQUENCE')

		layout.itemR(rd, "display_mode", text="Display")

class RENDER_PT_layers(RenderButtonsPanel):
	__label__ = "Layers"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout
		scene = context.scene
		rd = scene.render_data

		split = layout.split()
		split.itemL(text="Scene:")
		split.column().itemR(scene, "visible_layers", text="")

		row = layout.row()
		row.template_list(rd, "layers", rd, "active_layer_index", rows=2)

		col = row.column(align=True)
		col.itemO("SCENE_OT_render_layer_add", icon="ICON_ZOOMIN", text="")
		col.itemO("SCENE_OT_render_layer_remove", icon="ICON_ZOOMOUT", text="")

		rl = rd.layers[rd.active_layer_index]

		split = layout.split()
		split.itemL(text="Layers:")
		split.column().itemR(rl, "visible_layers", text="")

		layout.itemR(rl, "light_override", text="Light")
		layout.itemR(rl, "material_override", text="Material")

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
		row.itemR(rl, "pass_specular_exclude", text="", icon="ICON_DOT")
		row = col.row()
		row.itemR(rl, "pass_shadow")
		row.itemR(rl, "pass_shadow_exclude", text="", icon="ICON_DOT")
		row = col.row()
		row.itemR(rl, "pass_ao")
		row.itemR(rl, "pass_ao_exclude", text="", icon="ICON_DOT")
		row = col.row()
		row.itemR(rl, "pass_reflection")
		row.itemR(rl, "pass_reflection_exclude", text="", icon="ICON_DOT")
		row = col.row()
		row.itemR(rl, "pass_refraction")
		row.itemR(rl, "pass_refraction_exclude", text="", icon="ICON_DOT")

class RENDER_PT_shading(RenderButtonsPanel):
	__label__ = "Shading"

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data
		
		split = layout.split()
		
		col = split.column()
		col.itemR(rd, "render_shadows", text="Shadows")
		col.itemR(rd, "render_sss", text="Subsurface Scattering")
		col.itemR(rd, "render_envmaps", text="Environment Map")
		
		col = split.column()
		col.itemR(rd, "render_raytracing", text="Ray Tracing")
		row = col.row()
		row.active = rd.render_raytracing
		row.itemR(rd, "octree_resolution", text="Octree")
		col.itemR(rd, "alpha_mode", text="Alpha")

class RENDER_PT_performance(RenderButtonsPanel):
	__label__ = "Performance"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data

		split = layout.split()
		
		col = split.column(align=True)
		col.itemL(text="Threads:")
		col.row().itemR(rd, "threads_mode", expand=True)
		colsub = col.column()
		colsub.enabled = rd.threads_mode == 'THREADS_FIXED'
		colsub.itemR(rd, "threads")

		col = split.column()
		
		sub = col.column(align=True)
		sub.itemL(text="Tiles:")
		sub.itemR(rd, "parts_x", text="X")
		sub.itemR(rd, "parts_y", text="Y")

		split = layout.split()

		col = split.column()
		col.itemL(text="Memory:")
		row = col.row()
		row.itemR(rd, "save_buffers")
		row.enabled = not rd.full_sample

		col = split.column()
		col.itemL()
		col.itemR(rd, "free_image_textures")
		col.active = rd.use_compositing

class RENDER_PT_post_processing(RenderButtonsPanel):
	__label__ = "Post Processing"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data

		split = layout.split()

		col = split.column()
		col.itemR(rd, "use_compositing")
		col.itemR(rd, "use_sequencer")

		col = split.column()
		row = col.row()
		row.itemR(rd, "fields", text="Fields")
		rowsub = row.row()
		rowsub.active = rd.fields
		rowsub.itemR(rd, "fields_still", text="Still")
		rowsub = col.row()
		rowsub.active = rd.fields
		rowsub.itemR(rd, "field_order", expand=True)

		split = layout.split()
		split.itemL()
		split.itemR(rd, "dither_intensity", text="Dither", slider=True)
		
class RENDER_PT_output(RenderButtonsPanel):
	__label__ = "Output"

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data
		
		layout.itemR(rd, "output_path", text="")

		split = layout.split()
		col = split.column()
		col.itemR(rd, "placeholders")
		col.itemR(rd, "no_overwrite")

		col = split.column()
		col.itemR(rd, "file_format", text="")
		col.itemR(rd, "file_extensions")		

		if rd.file_format in ('AVIJPEG', 'JPEG'):
			split = layout.split()
			split.itemR(rd, "color_mode", text="Color")
			split.itemR(rd, "quality", slider=True)
			
		elif rd.file_format == 'OPENEXR':
			split = layout.split()
			col = split.column()
			col.itemR(rd, "color_mode", text="Color")
			col.itemR(rd, "exr_codec")

			subsplit = split.split()
			col = subsplit.column()
			col.itemR(rd, "exr_half")
			col.itemR(rd, "exr_zbuf")
			col = subsplit.column()
			col.itemR(rd, "exr_preview")
		
		elif rd.file_format == 'JPEG2000':
			split = layout.split()
			col = split.column()
			col.itemR(rd, "color_mode", text="Color")
			col.itemL(text="Depth:")
			col.row().itemR(rd, "jpeg_depth", expand=True)

			col = split.column()
			col.itemR(rd, "jpeg_preset", text="")
			col.itemR(rd, "jpeg_ycc")
			col.itemR(rd, "exr_preview")
			
		elif rd.file_format in ('CINEON', 'DPX'):
			split = layout.split()
			col = split.column()
			col.itemR(rd, "color_mode", text="Color")
			col.itemR(rd, "cineon_log", text="Convert to Log")

			col = split.column(align=True)
			col.active = rd.cineon_log
			col.itemR(rd, "cineon_black", text="Black")
			col.itemR(rd, "cineon_white", text="White")
			col.itemR(rd, "cineon_gamma", text="Gamma")
			
		elif rd.file_format == 'TIFF':
			split = layout.split()
			split.itemR(rd, "color_mode", text="Color")
			split.itemR(rd, "tiff_bit")
		
		else:
			split = layout.split()
			split.itemR(rd, "color_mode", text="Color")
			split.itemL()

class RENDER_PT_encoding(RenderButtonsPanel):
	__label__ = "Encoding"
	__default_closed__ = True
	
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
		col = split.column()
		col.itemR(rd, "ffmpeg_multiplex_audio")

class RENDER_PT_antialiasing(RenderButtonsPanel):
	__label__ = "Anti-Aliasing"

	def draw_header(self, context):
		layout = self.layout
		rd = context.scene.render_data

		layout.itemR(rd, "antialiasing", text="")

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data

		layout.active = rd.antialiasing

		split = layout.split()
		
		col = split.column()
		col.row().itemR(rd, "antialiasing_samples", expand=True)
		col.itemR(rd, "full_sample")

		col = split.column()
		col.itemR(rd, "pixel_filter", text="Filter")
		col.itemR(rd, "filter_size", text="Size", slider=True)
	
class RENDER_PT_dimensions(RenderButtonsPanel):
	__label__ = "Dimensions"

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
		row.itemR(rd, "border", text="Border")
		rowsub = row.row()
		rowsub.active = rd.border
		rowsub.itemR(rd, "crop_to_border", text="Crop")
		
		col = split.column(align=True)
		col.itemL(text="Frame Range:")
		col.itemR(scene, "start_frame", text="Start")
		col.itemR(scene, "end_frame", text="End")
		col.itemR(scene, "frame_step", text="Step")
		
		col.itemL(text="Frame Rate:")
		col.itemR(rd, "fps")
		col.itemR(rd, "fps_base",text="/")

class RENDER_PT_stamp(RenderButtonsPanel):
	__label__ = "Stamp"
	__default_closed__ = True

	def draw_header(self, context):
		rd = context.scene.render_data

		layout = self.layout
		layout.itemR(rd, "render_stamp", text="")

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

		sub = split.column()
		sub.active = rd.render_stamp
		sub.itemR(rd, "stamp_foreground", slider=True)
		sub.itemR(rd, "stamp_background", slider=True)
		sub.itemR(rd, "stamp_font_size", text="Font Size")

		row = layout.split(percentage=0.2)
		row.itemR(rd, "stamp_note", text="Note")
		rowsub = row.row()
		rowsub.active = rd.stamp_note
		rowsub.itemR(rd, "stamp_note_text", text="")

bpy.types.register(RENDER_PT_render)
bpy.types.register(RENDER_PT_dimensions)
bpy.types.register(RENDER_PT_antialiasing)
bpy.types.register(RENDER_PT_layers)
bpy.types.register(RENDER_PT_shading)
bpy.types.register(RENDER_PT_post_processing)
bpy.types.register(RENDER_PT_performance)
bpy.types.register(RENDER_PT_output)
bpy.types.register(RENDER_PT_encoding)
bpy.types.register(RENDER_PT_stamp)

