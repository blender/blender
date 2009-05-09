
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
		
		layout.split(number=2)
		
		sub = layout.sub(0)
		sub.column()
		sub.itemR(rd, "render_shadows", text="Shadows")
		sub.itemR(rd, "render_sss", text="SSS")
		sub.itemR(rd, "render_envmaps", text="EnvMap")
		sub.itemR(rd, "render_radiosity", text="Radio")
		
		sub = layout.sub(1)
		subsub = sub.box()
		subsub.column()
		subsub.itemR(rd, "render_raytracing", text="Ray Tracing")
		if (rd.render_raytracing):
			subsub.itemR(rd, "octree_resolution", text="Octree")
		sub.row()
		sub.itemR(rd, "alpha_mode")
		
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

		sub = layout.box()
		sub.row()
		sub.itemR(rd, "image_type")
		sub.row()
		sub.itemR(rd, "file_extensions")
		sub.itemR(rd, "color_mode")
		if rd.image_type in ("AVIJPEG", "JPEG"):
			sub.row()
			sub.itemR(rd, "quality")
	
class RENDER_PT_antialiasing(RenderButtonsPanel):
	__label__ = "Anti-Aliasing"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data

		layout.row()
		layout.itemR(rd, "antialiasing", text="Enable")
		if (rd.antialiasing):
			layout.row()
			layout.itemL(text="Samples:")
			layout.itemR(rd, "antialiasing_samples", expand=True)
			layout.row()
			layout.itemR(rd, "pixel_filter")
			layout.itemR(rd, "filter_size")

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
		layout.itemR(rd, "fps")
		layout.itemR(scene, "current_frame", text="Frame")
		
		layout.row()
		layout.itemR(scene, "end_frame", text="End")
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
		
		layout.split(number=2)
		
		sub = layout.sub(0)
		subsub = sub.box()
		subsub.column()
		subsub.itemL(text="Threads Mode:")
		subsub.itemR(rd, "threads_mode", expand=True)
		if rd.threads_mode == 'THREADS_FIXED':
			subsub.itemR(rd, "threads")
		
		subsub = sub.box()
		subsub.column()
		subsub.itemL(text="Distributed Rendering:")
		subsub.itemR(rd, "placeholders")
		subsub.itemR(rd, "no_overwrite")
		subsub = sub.box()
		subsub.column()
		subsub.itemR(rd, "fields", text="Fields")
		if (rd.fields):
			subsub.itemR(rd, "fields_still", text="Still")
			subsub.itemR(rd, "field_order", text="Order")
		
		sub = layout.sub(1)
		subsub = sub.box()
		subsub.column()
		subsub.itemL(text="Extra:")
		subsub.itemR(rd, "panorama")
		subsub.itemR(rd, "backbuf")
		subsub.itemR(rd, "free_image_textures")
		
		subsub = sub.box()
		subsub.column()
		subsub.itemL(text="Border:")
		subsub.itemR(rd, "border", text="Border Render")
		if (rd.border):
			subsub.itemR(rd, "crop_to_border")

bpy.types.register(RENDER_PT_render)
bpy.types.register(RENDER_PT_antialiasing)
bpy.types.register(RENDER_PT_shading)
bpy.types.register(RENDER_PT_image)
