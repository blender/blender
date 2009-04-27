
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

		layout.column_flow()
		layout.itemR(rd, "render_shadows", text="Shadows")
		layout.itemR(rd, "render_sss", text="SSS")
		layout.itemR(rd, "render_envmaps", text="EnvMap")
		layout.itemR(rd, "render_radiosity", text="Radio")
		
		layout.row()
		layout.itemR(rd, "render_raytracing", text="Ray Tracing")
		layout.itemR(rd, "octree_resolution")

		layout.row()
		layout.itemR(rd, "alpha_mode")
		layout.row()
		layout.itemR(rd, "free_image_textures")

class RENDER_PT_image(RenderButtonsPanel):
	__label__ = "Image"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data

		layout.column_flow()
		layout.itemR(rd, "resolution_x", text="SizeX")
		layout.itemR(rd, "resolution_y", text="SizeY")
		layout.itemR(rd, "pixel_aspect_x", text="AspX")
		layout.itemR(rd, "pixel_aspect_y", text="AspY")
		
		layout.row()
		layout.itemR(rd, "quality")
		layout.itemR(rd, "color_mode")
		
		layout.row()
		layout.itemR(rd, "placeholders")
		layout.itemR(rd, "no_overwrite")
		layout.itemR(rd, "file_extensions")
		
		layout.row()
		layout.itemR(rd, "crop_to_border")

class RENDER_PT_antialiasing(RenderButtonsPanel):
	__label__ = "Anti-Aliasing"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data

		layout.row()
		layout.itemR(rd, "antialiasing", text="Enable")
		layout.itemR(rd, "filter_size")
		layout.row()
		layout.itemR(rd, "pixel_filter")
		layout.itemR(rd, "antialiasing_samples", text="Samples")
		
		
		layout.row()
		layout.itemR(rd, "save_buffers")
		if (rd.save_buffers):
			layout.itemR(rd, "full_sample")

class RENDER_PT_render(RenderButtonsPanel):
	__label__ = "Render"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data

		layout.row()
		layout.itemO("SCREEN_OT_render", text="RENDER", icon=0) # ICON_SCENE
		#layout.itemO("SCREEN_OT_render", text="ANIM", icon=0) # "anim", 1

		layout.row()
		layout.itemR(scene, "start_frame", text="Start")
		layout.itemR(scene, "end_frame", text="End")
		layout.itemR(scene, "current_frame", text="Frame")
		
		layout.row()
		layout.itemR(rd, "fps")
		layout.itemR(rd, "fps_base",text="/")
		layout.itemR(scene, "frame_step", text="Step")

		layout.row()
		layout.itemR(rd, "do_composite")
		layout.itemR(rd, "do_sequence")

		layout.row()
		layout.itemL(text="General:")

		layout.column_flow()
		layout.itemR(rd, "resolution_percentage", text="Size ")
		layout.itemR(rd, "dither_intensity")
		layout.itemR(rd, "parts_x")
		layout.itemR(rd, "parts_y")

		layout.row()
		layout.itemR(rd, "threads_mode")
		if (rd.threads_mode == 'THREADS_FIXED'):
			layout.itemR(rd, "threads")
		
		layout.row()
		layout.itemR(rd, "fields", text="Fields")
		layout.itemR(rd, "field_order", text="Order")
		layout.itemR(rd, "fields_still", text="Still")

		layout.row()
		layout.itemL(text="Extra:")
		layout.row()
		layout.itemR(rd, "border", text="Border Render")
		layout.itemR(rd, "panorama")
		layout.itemR(rd, "backbuf")

bpy.types.register(RENDER_PT_render)
bpy.types.register(RENDER_PT_antialiasing)
bpy.types.register(RENDER_PT_shading)
bpy.types.register(RENDER_PT_image)
