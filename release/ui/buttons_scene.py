
import bpy

class RENDER_PT_shading(bpy.types.Panel):
	__label__ = "Shading"
	__context__ = "render"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		if not scene:
			return

		rd = scene.render_data

		layout.column_flow()
		layout.itemR(rd, "render_shadows", text="Shadows")
		layout.itemR(rd, "render_sss", text="SSS")
		layout.itemR(rd, "render_envmaps", text="EnvMap")
		layout.itemR(rd, "render_radiosity", text="Radio")
		layout.itemR(rd, "render_raytracing", text="Ray Tracing")
		layout.itemR(rd, "octree_resolution")

		layout.row()
		layout.itemR(rd, "alpha_mode")

class RENDER_PT_image(bpy.types.Panel):
	__label__ = "Image"
	__context__ = "render"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		if not scene:
			return

		rd = scene.render_data

		layout.column_flow()
		layout.itemR(rd, "resolution_x", text="SizeX")
		layout.itemR(rd, "resolution_y", text="SizeY")
		layout.itemR(rd, "pixel_aspect_x", text="AspX")
		layout.itemR(rd, "pixel_aspect_y", text="AspY")

		layout.row()
		layout.itemR(rd, "crop_to_border")

class RENDER_PT_antialiasing(bpy.types.Panel):
	__label__ = "Anti-Aliasing"
	__context__ = "render"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		if not scene:
			return

		rd = scene.render_data

		layout.column_flow()
		layout.itemR(rd, "antialiasing", text="Enable")
		layout.itemR(rd, "antialiasing_samples", text="Samples")
		layout.itemR(rd, "pixel_filter")
		layout.itemR(rd, "filter_size")

class RENDER_PT_render(bpy.types.Panel):
	__label__ = "Render"
	__context__ = "render"

	def draw(self, context):
		scene = context.scene
		layout = self.layout

		if not scene:
			return

		rd = scene.render_data

		layout.row()
		layout.itemO("SCREEN_OT_render", text="RENDER", icon=0) # ICON_SCENE
		#layout.itemO("SCREEN_OT_render", text="ANIM", icon=0) # "anim", 1

		layout.row()
		layout.itemR(scene, "start_frame", text="Start")
		layout.itemR(scene, "end_frame", text="End")
		layout.itemR(scene, "current_frame", text="Frame")

		layout.row()
		layout.itemR(rd, "do_composite")
		layout.itemR(rd, "do_sequence")

		layout.row()
		layout.itemL(text="General")

		layout.row()
		layout.itemR(rd, "resolution_percentage", text="Size ")
		layout.itemR(rd, "dither_intensity")

		layout.row()
		layout.itemR(rd, "parts_x")
		layout.itemR(rd, "parts_y")

		layout.row()
		layout.itemR(rd, "threads")
		layout.itemR(rd, "threads_mode")

		layout.row()
		layout.itemR(rd, "fields", text="Fields")
		layout.itemR(rd, "field_order", text="Order")
		layout.itemR(rd, "fields_still", text="Still")

		layout.row()
		layout.itemL(text="Extra:")
		layout.row()
		layout.itemR(rd, "border", text="Border Render")
		layout.itemR(rd, "panorama")

bpy.ui.addPanel(RENDER_PT_shading, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(RENDER_PT_image, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(RENDER_PT_antialiasing, "BUTTONS_WINDOW", "WINDOW")
bpy.ui.addPanel(RENDER_PT_render, "BUTTONS_WINDOW", "WINDOW")

