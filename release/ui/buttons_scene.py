
import bpy

class RenderButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "scene"

class RENDER_PT_shading(RenderButtonsPanel):
	__label__ = "Shading"

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(rd, "render_shadows", text="Shadows")
		sub.itemR(rd, "render_sss", text="Subsurface Scattering")
		sub.itemR(rd, "render_envmaps", text="Environment Map")
		#	sub.itemR(rd, "render_radiosity", text="Radio")
		
		col = split.column()
		col.itemR(rd, "render_raytracing", text="Ray Tracing")
		colsub = col.column()
		colsub.active = rd.render_raytracing
		colsub.itemR(rd, "raytrace_structure", text="Structure")
		colsub.itemR(rd, "raytrace_tree_type", text="Tree Type")
		colsub.itemR(rd, "octree_resolution", text="Octree")
		col.itemR(rd, "dither_intensity", text="Dither", slider=True)
		
class RENDER_PT_output(RenderButtonsPanel):
	__label__ = "Output"

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data
		
		layout.itemR(rd, "display_mode", text="Display")
		
		layout.itemR(rd, "output_path")
		
		split = layout.split()
		
		col = split.column()
		col.itemR(rd, "file_extensions")		
		col.itemR(rd, "fields", text="Fields")
		colsub = col.column()
		colsub.active = rd.fields
		colsub.itemR(rd, "fields_still", text="Still")
		colsub.row().itemR(rd, "field_order", expand=True)
		
		col = split.column()
		col.itemR(rd, "color_mode")
		col.itemR(rd, "alpha_mode")
		col.itemL(text="Distributed Rendering:")
		col.itemR(rd, "placeholders")
		col.itemR(rd, "no_overwrite")
		
		layout.itemR(rd, "file_format", text="Format")
		
		split = layout.split()
		
		col = split.column()
		
		if rd.file_format in ('AVIJPEG', 'JPEG'):
			col.itemR(rd, "quality", slider=True)
			
		elif rd.file_format == 'OPENEXR':
			col.itemR(rd, "exr_codec")
			col.itemR(rd, "exr_half")
			col = split.column()
			col.itemR(rd, "exr_zbuf")
			col.itemR(rd, "exr_preview")
		
		elif rd.file_format == 'JPEG2000':
			row = layout.row()
			row.itemR(rd, "jpeg_preset")
			split = layout.split()
			col = split.column()
			col.itemL(text="Depth:")
			col.row().itemR(rd, "jpeg_depth", expand=True)
			col = split.column()
			col.itemR(rd, "jpeg_ycc")
			col.itemR(rd, "exr_preview")
			
		elif rd.file_format in ('CINEON', 'DPX'):
			col.itemR(rd, "cineon_log", text="Convert to Log")
			colsub = col.column()
			colsub.active = rd.cineon_log
			colsub.itemR(rd, "cineon_black", text="Black")
			colsub.itemR(rd, "cineon_white", text="White")
			colsub.itemR(rd, "cineon_gamma", text="Gamma")
			
		elif rd.file_format == 'TIFF':
			col.itemR(rd, "tiff_bit")
		
		elif rd.file_format == 'FFMPEG':
			#row = layout.row()
			#row.itemR(rd, "ffmpeg_format")
			#row.itemR(rd, "ffmpeg_codec")
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
			#row.itemR(rd, "ffmpeg_audio_codec")
			
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
		
		sub = split.column()
		sub.itemL(text="Samples:")
		sub.row().itemR(rd, "antialiasing_samples", expand=True)
		sub.itemR(rd, "pixel_filter")

		col = split.column()
		col.itemR(rd, "filter_size", text="Size", slider=True)
		col.itemR(rd, "save_buffers")
		colsub = col.column()
		colsub.active = rd.save_buffers
		colsub.itemR(rd, "full_sample")

class RENDER_PT_render(RenderButtonsPanel):
	__label__ = "Render"

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data

		row = layout.row()
		row.itemO("SCREEN_OT_render", text="Render Still", icon='ICON_IMAGE_COL')
		row.item_booleanO("SCREEN_OT_render", "anim", True, text="Render Animation", icon='ICON_SEQUENCE')
		
		row = layout.row()
		row.itemR(rd, "do_composite")
		row.itemR(rd, "do_sequence")
		rowsub = layout.row()
		rowsub.active = rd.do_composite
		rowsub.itemR(rd, "free_image_textures")

		split = layout.split()
		
		col = split.column(align=True)
		col.itemL(text="Threads:")
		col.row().itemR(rd, "threads_mode", expand=True)
		colsub = col.column()
		colsub.active = rd.threads_mode == 'THREADS_FIXED'
		colsub.itemR(rd, "threads")
		
		sub = split.column(align=True)
		sub.itemL(text="Tiles:")
		sub.itemR(rd, "parts_x", text="X")
		sub.itemR(rd, "parts_y", text="Y")
		
		split = layout.split()
		sub = split.column()
		sub = split.column()
		sub.itemR(rd, "panorama")
		
		#	row.itemR(rd, "backbuf")
			
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
		
		col = col.column(align=False)
		col.itemR(rd, "border", text="Border")
		colsub = col.column()
		colsub.active = rd.border
		colsub.itemR(rd, "crop_to_border")

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

	def draw_header(self, context):
		rd = context.scene.render_data

		layout = self.layout
		layout.itemR(rd, "stamp", text="")

	def draw(self, context):
		layout = self.layout
		rd = context.scene.render_data

		layout.active = rd.stamp

		split = layout.split()
		
		col = split.column()
		col.itemR(rd, "stamp_time", text="Time")
		col.itemR(rd, "stamp_date", text="Date")
		col.itemR(rd, "stamp_frame", text="Frame")
		col.itemR(rd, "stamp_camera", text="Scene")
		col.itemR(rd, "stamp_marker", text="Marker")
		col.itemR(rd, "stamp_filename", text="Filename")
		col.itemR(rd, "stamp_sequence_strip", text="Seq. Strip")
		col.itemR(rd, "stamp_note", text="Note")
		colsub = col.column()
		colsub.active = rd.stamp_note
		colsub.itemR(rd, "stamp_note_text", text="")
		
		sub = split.column()
		sub.itemR(rd, "render_stamp")
		colsub = sub.column()
		colsub.active = rd.render_stamp
		colsub.itemR(rd, "stamp_foreground", slider=True)
		colsub.itemR(rd, "stamp_background", slider=True)
		colsub.itemR(rd, "stamp_font_size", text="Font Size")

bpy.types.register(RENDER_PT_render)
bpy.types.register(RENDER_PT_dimensions)
bpy.types.register(RENDER_PT_antialiasing)
bpy.types.register(RENDER_PT_shading)
bpy.types.register(RENDER_PT_output)
bpy.types.register(RENDER_PT_stamp)
