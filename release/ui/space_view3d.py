
import bpy

# ********** Header **********

class VIEW3D_HT_header(bpy.types.Header):
	__space_type__ = "VIEW_3D"

	def draw(self, context):
		layout = self.layout
		
		view = context.space_data
		mode_string = context.mode_string

		layout.template_header()
		
		# Menus
		if context.area.show_menus:
			row = layout.row()

			row.itemM("VIEW3D_MT_view")
			
			# Select Menu
			selectmenu = "VIEW3D_MT_select_%s" % mode_string
			if selectmenu in dir(bpy.types):
				layout.itemM(selectmenu)

		layout.template_header_3D()

# ********** Menu **********

# ********** View menus **********

class VIEW3D_MT_view(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.properties", icon="ICON_MENU_PANEL")
		layout.itemO("view3d.toolbar", icon="ICON_MENU_PANEL")
		
		layout.itemS()
		
		layout.item_enumO("view3d.viewnumpad", "type", 'CAMERA')
		layout.item_enumO("view3d.viewnumpad", "type", 'TOP')
		layout.item_enumO("view3d.viewnumpad", "type", 'FRONT')
		layout.item_enumO("view3d.viewnumpad", "type", 'RIGHT')
		
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
		
		layout.itemO("screen.region_foursplit", text="Toggle Quad View")
		layout.itemO("screen.screen_full_area", text="Toggle Full Screen")
		
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

# ********** Select menus **********

class VIEW3D_MT_select_objectmode(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")

		layout.itemS()

		layout.itemO("object.select_all_toggle", text="Select/Deselect All")
		layout.itemO("object.select_inverse", text="Inverse")
		layout.itemO("object.select_random", text="Random")
		layout.itemO("object.select_by_layer", text="Select All by Layer")
		layout.item_enumO("object.select_by_type", "type", "", text="Select All by Type")
		layout.item_enumO("object.select_grouped", "type", "", text="Select Grouped")

class VIEW3D_MT_select_posemode(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")

		layout.itemS()
		
		layout.itemO("pose.select_all_toggle", text="Select/Deselect All")
		layout.itemO("pose.select_inverse", text="Inverse")
		layout.itemO("pose.select_constraint_target", text="Constraint Target")
		
		layout.itemS()
		
		layout.item_enumO("pose.select_hierarchy", "direction", "PARENT")
		layout.item_enumO("pose.select_hierarchy", "direction", "CHILD")
		
		layout.itemS()
		
		layout.view3d_select_posemenu()

class VIEW3D_MT_select_particlemode(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")

		layout.itemS()
		
		layout.itemO("particle.select_all_toggle", text="Select/Deselect All")
		layout.itemO("particle.select_linked")
		
		layout.itemS()
		
		#layout.itemO("particle.select_last")
		#layout.itemO("particle.select_first")
		
		layout.itemO("particle.select_more")
		layout.itemO("particle.select_less")

class VIEW3D_MT_select_mesh_edit(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")

		layout.itemS()

		layout.itemO("mesh.select_all_toggle", text="Select/Deselect All")
		layout.itemO("mesh.select_inverse", text="Inverse")

		layout.itemS()

		layout.itemO("mesh.select_random", text="Random...")
		layout.itemO("mesh.edges_select_sharp", text="Sharp Edges")
		layout.itemO("mesh.faces_select_linked_flat", text="Linked Flat Faces")

		layout.itemS()

		layout.item_enumO("mesh.select_by_number_vertices", "type", "TRIANGLES", text="Triangles")
		layout.item_enumO("mesh.select_by_number_vertices", "type", "QUADS", text="Quads")
		layout.item_enumO("mesh.select_by_number_vertices", "type", "OTHER", text="Loose Verts/Edges")
		layout.itemO("mesh.select_similar", text="Similar...")

		layout.itemS()

		layout.itemO("mesh.select_less", text="Less")
		layout.itemO("mesh.select_more", text="More")

		layout.itemS()

		layout.itemO("mesh.select_linked", text="Linked")
		layout.itemO("mesh.select_vertex_path", text="Vertex Path")
		layout.itemO("mesh.loop_multi_select", text="Edge Loop")
		layout.item_booleanO("mesh.loop_multi_select", "ring", True, text="Edge Ring")

		layout.itemS()

		layout.itemO("mesh.loop_to_region")
		layout.itemO("mesh.region_to_loop")

class VIEW3D_MT_select_curve_edit(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")
		layout.itemO("view3d.select_circle")

		layout.itemS()
		
		layout.itemO("curve.select_all_toggle", text="Select/Deselect All")
		layout.itemO("curve.select_inverse")
		layout.itemO("curve.select_random")
		layout.itemO("curve.select_every_nth")

		layout.itemS()
		
		layout.itemO("curve.de_select_first")
		layout.itemO("curve.de_select_last")
		layout.itemO("curve.select_next")
		layout.itemO("curve.select_previous")

		layout.itemS()
		
		layout.itemO("curve.select_more")
		layout.itemO("curve.select_less")

class VIEW3D_MT_select_surface_edit(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")
		layout.itemO("view3d.select_circle")

		layout.itemS()
		
		layout.itemO("curve.select_all_toggle", text="Select/Deselect All")
		layout.itemO("curve.select_inverse")
		layout.itemO("curve.select_random")
		layout.itemO("curve.select_every_nth")

		layout.itemS()
		
		layout.itemO("curve.select_row")

		layout.itemS()
		
		layout.itemO("curve.select_more")
		layout.itemO("curve.select_less")

class VIEW3D_MT_select_mball_edit(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")
		
		layout.itemS()
		
		layout.itemL(text="Select/Deselect All")
		layout.itemL(text="Inverse")
		
		layout.itemS()
		
		layout.itemL(text="Random")

class VIEW3D_MT_select_lattice_edit(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")

		layout.itemS()
		
		layout.itemO("lattice.select_all_toggle", text="Select/Deselect All")

class VIEW3D_MT_select_armature_edit(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")

		layout.itemS()
		
		layout.itemO("armature.select_all_toggle", text="Select/Deselect All")
		layout.itemO("armature.select_inverse", text="Inverse")

		layout.itemS()
		
		layout.item_enumO("armature.select_hierarchy", "direction", "PARENT")
		layout.item_enumO("armature.select_hierarchy", "direction", "CHILD")
		
		layout.itemS()
		
		layout.view3d_select_armaturemenu()

class VIEW3D_MT_select_facesel(bpy.types.Menu):
	__space_type__ = "VIEW_3D"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.view3d_select_faceselmenu()


# ********** Panel **********

class VIEW3D_PT_3dview_properties(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "UI"
	__label__ = "View"

	def poll(self, context):
		view = context.space_data
		return (view)

	def draw(self, context):
		layout = self.layout
		
		view = context.space_data
		scene = context.scene
		
		col = layout.column()
		col.itemR(view, "camera")
		col.itemR(view, "lens")
		
		layout.itemL(text="Clip:")
		col = layout.column(align=True)
		col.itemR(view, "clip_start", text="Start")
		col.itemR(view, "clip_end", text="End")
		
		layout.itemL(text="Grid:")
		col = layout.column(align=True)
		col.itemR(view, "grid_lines", text="Lines")
		col.itemR(view, "grid_spacing", text="Spacing")
		col.itemR(view, "grid_subdivisions", text="Subdivisions")
		
		layout.column().itemR(scene, "cursor_location", text="3D Cursor:")
		
class VIEW3D_PT_3dview_display(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "UI"
	__label__ = "Display"

	def poll(self, context):
		view = context.space_data
		return (view)

	def draw(self, context):
		layout = self.layout
		view = context.space_data
		
		col = layout.column()
		col.itemR(view, "display_floor", text="Grid Floor")
		col.itemR(view, "display_x_axis", text="X Axis")
		col.itemR(view, "display_y_axis", text="Y Axis")
		col.itemR(view, "display_z_axis", text="Z Axis")
		col.itemR(view, "outline_selected")
		col.itemR(view, "all_object_centers")
		col.itemR(view, "relationship_lines")
		col.itemR(view, "textured_solid")
		
		layout.itemS()
		
		layout.itemO("screen.region_foursplit")
		
		col = layout.column()
		col.itemR(view, "lock_rotation")
		col.itemR(view, "box_preview")
		col.itemR(view, "box_clip")
	
class VIEW3D_PT_background_image(bpy.types.Panel):
	__space_type__ = "VIEW_3D"
	__region_type__ = "UI"
	__label__ = "Background Image"
	__default_closed__ = True

	def poll(self, context):
		view = context.space_data
		bg = context.space_data.background_image
		return (view)

	def draw_header(self, context):
		layout = self.layout
		view = context.space_data

		layout.itemR(view, "display_background_image", text="")

	def draw(self, context):
		layout = self.layout
		
		view = context.space_data
		bg = view.background_image

		if bg:
			layout.active = view.display_background_image

			col = layout.column()
			col.itemR(bg, "image", text="")
			#col.itemR(bg, "image_user")
			col.itemR(bg, "size")
			col.itemR(bg, "transparency", slider=True)
			col.itemL(text="Offset:")
			
			col = layout.column(align=True)
			col.itemR(bg, "x_offset", text="X")
			col.itemR(bg, "y_offset", text="Y")

bpy.types.register(VIEW3D_HT_header) # Header

bpy.types.register(VIEW3D_MT_view) #View Menus
bpy.types.register(VIEW3D_MT_view_navigation)

bpy.types.register(VIEW3D_MT_select_objectmode) # Select Menus
bpy.types.register(VIEW3D_MT_select_posemode)
bpy.types.register(VIEW3D_MT_select_particlemode)
bpy.types.register(VIEW3D_MT_select_mesh_edit)
bpy.types.register(VIEW3D_MT_select_curve_edit)
bpy.types.register(VIEW3D_MT_select_surface_edit)
bpy.types.register(VIEW3D_MT_select_mball_edit)
bpy.types.register(VIEW3D_MT_select_lattice_edit)
bpy.types.register(VIEW3D_MT_select_armature_edit)
bpy.types.register(VIEW3D_MT_select_facesel)

bpy.types.register(VIEW3D_PT_3dview_properties) # Panels
bpy.types.register(VIEW3D_PT_3dview_display)
bpy.types.register(VIEW3D_PT_background_image)
