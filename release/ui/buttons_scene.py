
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
		sub.itemR(rd, "render_radiosity", text="Radio")
		
		sub = split.column()
		subsub = sub.box()
		subsub.itemR(rd, "render_raytracing", text="Ray Tracing")
		if (rd.render_raytracing):
			subsub.itemR(rd, "octree_resolution", text="Octree")
		sub.itemR(rd, "alpha_mode")
		
class RENDER_PT_image(RenderButtonsPanel):
	__label__ = "Image"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		rd = scene.render_data

		flow = layout.column_flow()
		flow.itemR(rd, "resolution_x", text="SizeX")
		flow.itemR(rd, "resolution_y", text="SizeY")
		flow.itemR(rd, "pixel_aspect_x", text="AspX")
		flow.itemR(rd, "pixel_aspect_y", text="AspY")

		box = layout.box()
		box.itemR(rd, "output_path")
		box.itemR(rd, "image_type")
		row = box.row()
		row.itemR(rd, "file_extensions")
		row.itemR(rd, "color_mode")
		if rd.image_type in ("AVIJPEG", "JPEG"):
			row = box.row()
			row.itemR(rd, "quality")
	
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
			row = layout.row()
			row.itemR(rd, "pixel_filter")
			row.itemR(rd, "filter_size")

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

		row = layout.row()
		row.itemR(scene, "start_frame", text="Start")
		row.itemR(rd, "fps")
		row.itemR(scene, "current_frame", text="Frame")
		
		row = layout.row()
		row.itemR(scene, "end_frame", text="End")
		row.itemR(rd, "fps_base",text="/")
		row.itemR(scene, "frame_step", text="Step")

		row = layout.row()
		row.itemR(rd, "do_composite")
		row.itemR(rd, "do_sequence")

		row = layout.row()
		row.itemL(text="General:")

		flow = layout.column_flow()
		flow.itemR(rd, "resolution_percentage", text="Size ")
		flow.itemR(rd, "dither_intensity")
		flow.itemR(rd, "parts_x")
		flow.itemR(rd, "parts_y")
		
		split = layout.split()
		
		sub = split.column()
		subsub = sub.box()
		subsub.itemL(text="Threads Mode:")
		subsub.itemR(rd, "threads_mode", expand=True)
		if rd.threads_mode == 'THREADS_FIXED':
			subsub.itemR(rd, "threads")
		
		subsub = sub.box()
		subsub.itemL(text="Distributed Rendering:")
		subsub.itemR(rd, "placeholders")
		subsub.itemR(rd, "no_overwrite")
		subsub = sub.box()
		subsub.itemR(rd, "fields", text="Fields")
		if rd.fields:
			subsub.itemR(rd, "fields_still", text="Still")
			subsub.itemR(rd, "field_order", text="Order")
		
		sub = split.column()
		subsub = sub.box()
		subsub.itemL(text="Extra:")
		subsub.itemR(rd, "panorama")
		subsub.itemR(rd, "backbuf")
		subsub.itemR(rd, "free_image_textures")
		
		subsub = sub.box()
		subsub.itemL(text="Border:")
		subsub.itemR(rd, "border", text="Border Render")
		if rd.border:
			subsub.itemR(rd, "crop_to_border")

bpy.types.register(RENDER_PT_render)
bpy.types.register(RENDER_PT_antialiasing)
bpy.types.register(RENDER_PT_shading)
bpy.types.register(RENDER_PT_image)

