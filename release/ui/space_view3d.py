
import bpy

class VIEW3D_MT_view_navigation(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Navigation"

	def draw(self, context):
		layout = self.layout

		# layout.itemO("VIEW3D_OT_view_fly_mode")
		# layout.itemS()
		
		layout.items_enumO("VIEW3D_OT_view_orbit", "type")
		
		layout.itemS()
		
		layout.items_enumO("VIEW3D_OT_view_pan", "type")
		
		layout.itemS()
		
		layout.item_floatO("VIEW3D_OT_zoom", "delta", 1.0, text="Zoom In")
		layout.item_floatO("VIEW3D_OT_zoom", "delta", -1.0, text="Zoom Out")

class VIEW3D_MT_view(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout

		layout.itemO("VIEW3D_OT_properties", icon="ICON_MENU_PANEL")
		layout.itemO("VIEW3D_OT_toolbar", icon="ICON_MENU_PANEL")
		
		layout.itemS()
		
		layout.item_enumO("VIEW3D_OT_viewnumpad", "type", "CAMERA")
		layout.item_enumO("VIEW3D_OT_viewnumpad", "type", "TOP")
		layout.item_enumO("VIEW3D_OT_viewnumpad", "type", "FRONT")
		layout.item_enumO("VIEW3D_OT_viewnumpad", "type", "RIGHT")
		
		# layout.itemM("VIEW3D_MT_view_cameras", text="Cameras")
		
		layout.itemS()

		layout.itemO("VIEW3D_OT_view_persportho")
		
		layout.itemS()
		
		# layout.itemO("VIEW3D_OT_view_show_all_layers")
		
		# layout.itemS()
		
		# layout.itemO("VIEW3D_OT_view_local_view")
		# layout.itemO("VIEW3D_OT_view_global_view")
		
		# layout.itemS()
		
		layout.itemM("VIEW3D_MT_view_navigation")
		# layout.itemM("VIEW3D_MT_view_align", text="Align View")
		
		layout.itemS()

		layout.operator_context = "INVOKE_REGION_WIN"

		layout.itemO("VIEW3D_OT_clip_border")
		layout.itemO("VIEW3D_OT_zoom_border")
		
		layout.itemS()
		
		layout.itemO("VIEW3D_OT_view_center")
		layout.itemO("VIEW3D_OT_view_all")
		
		layout.itemS()
		
		layout.itemO("SCREEN_OT_screen_full_area")

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

class VIEW3D_PT_random_panel(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "UI"
	__label__ = "Random Panel"

	def draw(self, context):
		layout = self.layout
		layout.itemL(text="panel contents")

bpy.types.register(VIEW3D_MT_view_navigation)
bpy.types.register(VIEW3D_MT_view)
bpy.types.register(VIEW3D_HT_header)
bpy.types.register(VIEW3D_PT_random_panel)

