
import bpy

class RenderButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "scene"

class RENDER_PT_shading(RenderButtonsPanel):
	__label__ = "Shading"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(rd, "render_shadows", text="Shadows")
		sub.itemR(rd, "render_sss", text="SSS")
		sub.itemR(rd, "render_envmaps", text="EnvMap")
		#	sub.itemR(rd, "render_radiosity", text="Radio")
		
		sub = split.column()
		sub.itemR(rd, "render_raytracing", text="Ray Tracing")
		if (rd.render_raytracing):
			sub.itemR(rd, "octree_resolution", text="Octree")
		
class RENDER_PT_output(RenderButtonsPanel):
	__label__ = "Output"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data
		
		col = layout.column()
		col.itemR(rd, "output_path")
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(rd, "image_type")
		if rd.image_type in ("AVIJPEG", "JPEG"):
			sub.itemR(rd, "quality")
		
		sub = split.column()
		sub.itemR(rd, "color_mode")
		sub.itemR(rd, "alpha_mode")
		
		split = layout.split()
		
		sub = split.column()
		sub.itemL(text="Distributed Rendering:")
		sub.itemR(rd, "placeholders")
		sub.itemR(rd, "no_overwrite")
		
		sub = split.column()
		sub.itemL(text="Settings:")
		sub.itemR(rd, "file_extensions")
		sub.itemR(rd, "fields", text="Fields")
		if rd.fields:
			sub.itemR(rd, "fields_still", text="Still")
			sub.row().itemR(rd, "field_order", expand=True)
		
	
class RENDER_PT_antialiasing(RenderButtonsPanel):
	__label__ = "Anti-Aliasing"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data

		row = layout.row()
		row.itemR(rd, "antialiasing", text="Enable")

		if rd.antialiasing:
			row = layout.row()
			row.itemL(text="Samples:")
			row.itemR(rd, "antialiasing_samples", expand=True)
			row = layout.row(align=True)
			row.itemR(rd, "pixel_filter")
			row.itemR(rd, "filter_size", text="Size")

			row = layout.row()
			row.itemR(rd, "save_buffers")
			if rd.save_buffers:
				row.itemR(rd, "full_sample")

class RENDER_PT_render(RenderButtonsPanel):
	__label__ = "Render"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data

		row = layout.row()
		row.itemO("SCREEN_OT_render", text="RENDER", icon=0) # ICON_SCENE
		row.item_booleanO("SCREEN_OT_render", "anim", True, text="ANIM", icon=0)
		
		split = layout.split()
		
		sub = split.column(align=True)
		sub.itemR(scene, "start_frame", text="Start")
		sub.itemR(scene, "end_frame", text="End")
		
		sub = split.column(align=True)
		sub.itemR(rd, "fps")
		sub.itemR(rd, "fps_base",text="/")
		
		sub = split.column(align=True)
		sub.itemR(scene, "current_frame", text="Frame")
		sub.itemR(scene, "frame_step", text="Step")

		row = layout.row()
		row.itemR(rd, "do_composite")
		row.itemR(rd, "do_sequence")
		if rd.do_composite:
			row = layout.row()
			row.itemR(rd, "free_image_textures")
		
		row = layout.row()
		row.itemL(text="Threads:")
		row.itemL(text="Tiles:")
		
		split = layout.split()
		
		sub = split.column(align=True)
		sub.itemR(rd, "threads_mode", expand=True)
		if rd.threads_mode == 'THREADS_FIXED':
			sub.itemR(rd, "threads")
		
		sub = split.column(align=True)
		sub.itemR(rd, "parts_x", text="X")
		sub.itemR(rd, "parts_y", text="Y")
		
		row = layout.row()
		row.itemR(rd, "panorama")
		row.itemR(rd, "dither_intensity")
		#	row.itemR(rd, "backbuf")
			
class RENDER_PT_dimensions(RenderButtonsPanel):
	__label__ = "Dimensions"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data
		
		row = layout.row()
		row.itemL(text="Resolution:")
		row.itemL(text="Aspect Ratio:")
		
		split = layout.split()

		col = split.column(align=True)
		col.itemR(rd, "resolution_x", text="X")
		col.itemR(rd, "resolution_y", text="Y")
		col.itemR(rd, "resolution_percentage", text="")
		
		col = split.column()
		sub = col.column(align=True)
		sub.itemR(rd, "pixel_aspect_x", text="X")
		sub.itemR(rd, "pixel_aspect_y", text="Y")
		
		sub = col.column()
		sub.itemR(rd, "border", text="Border")
		if rd.border:
			sub.itemR(rd, "crop_to_border")

bpy.types.register(RENDER_PT_render)
bpy.types.register(RENDER_PT_dimensions)
bpy.types.register(RENDER_PT_antialiasing)
bpy.types.register(RENDER_PT_shading)
bpy.types.register(RENDER_PT_output)