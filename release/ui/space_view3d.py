
import bpy

class VIEW3D_MT_view_navigation(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Navigation"

	def draw(self, context):
		layout = self.layout

		# layout.itemO("view3d.view_fly_mode")
		# layout.itemS()
		
		layout.items_enumO("view3d.view_orbit", "type")
		
		layout.itemS()
		
		layout.items_enumO("view3d.view_pan", "type")
		
		layout.itemS()
		
		layout.item_floatO("view3d.zoom", "delta", 1.0, text="Zoom In")
		layout.item_floatO("view3d.zoom", "delta", -1.0, text="Zoom Out")

class VIEW3D_MT_view(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.properties", icon="ICON_MENU_PANEL")
		layout.itemO("view3d.toolbar", icon="ICON_MENU_PANEL")
		
		layout.itemS()
		
		layout.item_enumO("view3d.viewnumpad", "type", "CAMERA")
		layout.item_enumO("view3d.viewnumpad", "type", "TOP")
		layout.item_enumO("view3d.viewnumpad", "type", "FRONT")
		layout.item_enumO("view3d.viewnumpad", "type", "RIGHT")
		
		# layout.itemM("VIEW3D_MT_view_cameras", text="Cameras")
		
		layout.itemS()

		layout.itemO("view3d.view_persportho")
		
		layout.itemS()
		
		# layout.itemO("view3d.view_show_all_layers")
		
		# layout.itemS()
		
		# layout.itemO("view3d.view_local_view")
		# layout.itemO("view3d.view_global_view")
		
		# layout.itemS()
		
		layout.itemM("VIEW3D_MT_view_navigation")
		# layout.itemM("VIEW3D_MT_view_align", text="Align View")
		
		layout.itemS()

		layout.operator_context = "INVOKE_REGION_WIN"

		layout.itemO("view3d.clip_border")
		layout.itemO("view3d.zoom_border")
		
		layout.itemS()
		
		layout.itemO("view3d.view_center")
		layout.itemO("view3d.view_all")
		
		layout.itemS()
		
		layout.itemO("screen.screen_full_area")

class VIEW3D_HT_header(bpy.types.Header):
	__space_type__ = "VIEW_3D"

	def draw(self, context):
		layout = self.layout

		layout.template_header()

		# menus
		if context.area.show_menus:
			row = layout.row()
			row.itemM("VIEW3D_MT_view")

		layout.template_header_3D()

class VIEW3D_PT_3dview_properties(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "UI"
	__label__ = "View"

	def poll(self, context):
		view = context.space_data
		return (view)

	def draw(self, context):
		view = context.space_data
		layout = self.layout
		
		split = layout.split()
		col = split.column()
		col.itemR(view, "camera")
		col.itemR(view, "lens")
		col.itemL(text="Clip:")
		col.itemR(view, "clip_start", text="Start")
		col.itemR(view, "clip_end", text="End")
		col.itemL(text="Grid:")
		col.itemR(view, "grid_spacing", text="Spacing")
		col.itemR(view, "grid_subdivisions", text="Subdivisions")
		
class VIEW3D_PT_3dview_display(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "UI"
	__label__ = "Display"

	def poll(self, context):
		view = context.space_data
		return (view)

	def draw(self, context):
		view = context.space_data
		layout = self.layout
		
		split = layout.split()
		col = split.column()
		col.itemR(view, "display_floor", text="Grid Floor")
		col.itemR(view, "display_x_axis", text="X Axis")
		col.itemR(view, "display_y_axis", text="Y Axis")
		col.itemR(view, "display_z_axis", text="Z Axis")
		col.itemR(view, "outline_selected")
		col.itemR(view, "all_object_centers")
		col.itemR(view, "relationship_lines")
		col.itemR(view, "textured_solid")
			
class VIEW3D_PT_background_image(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "UI"
	__label__ = "Background Image"

	def poll(self, context):
		view = context.space_data
		bg = context.space_data.background_image
		return (view)

	def draw_header(self, context):
		layout = self.layout
		view = context.space_data

		layout.itemR(view, "display_background_image", text="")

	def draw(self, context):
		view = context.space_data
		bg = context.space_data.background_image
		layout = self.layout
		
		layout.active = view.display_background_image
		split = layout.split()
		col = split.column()
		col.itemR(bg, "image")
#		col.itemR(bg, "image_user")
		col.itemR(bg, "size")
		col.itemR(bg, "transparency", slider=True)
		col.itemL(text="Offset:")
		col.itemR(bg, "x_offset", text="X")
		col.itemR(bg, "y_offset", text="Y")



bpy.types.register(VIEW3D_MT_view_navigation)
bpy.types.register(VIEW3D_MT_view)
bpy.types.register(VIEW3D_HT_header)
bpy.types.register(VIEW3D_PT_3dview_properties)
bpy.types.register(VIEW3D_PT_3dview_display)
bpy.types.register(VIEW3D_PT_background_image)

