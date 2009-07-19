
import bpy

class IMAGE_MT_view(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout
		sima = context.space_data
		uv = sima.uv_editor
		settings = context.scene.tool_settings

		show_uvedit = sima.show_uvedit

		layout.itemO("IMAGE_OT_properties", icon="ICON_MENU_PANEL")

		layout.itemS()

		layout.itemR(sima, "update_automatically")
		if show_uvedit:
			layout.itemR(settings, "uv_local_view") # Numpad /

		layout.itemS()

		layout.itemO("IMAGE_OT_view_zoom_in")
		layout.itemO("IMAGE_OT_view_zoom_out")

		layout.itemS()

		ratios = [[1, 8], [1, 4], [1, 2], [1, 1], [2, 1], [4, 1], [8, 1]];

		for a, b in ratios:
			text = "Zoom %d:%d" % (a, b)
			layout.item_floatO("IMAGE_OT_view_zoom_ratio", "ratio", a/b, text=text)

		layout.itemS()

		if show_uvedit:
			layout.itemO("IMAGE_OT_view_selected")

		layout.itemO("IMAGE_OT_view_all")
		layout.itemO("SCREEN_OT_screen_full_area")

class IMAGE_MT_select(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("UV_OT_select_border")
		layout.item_booleanO("UV_OT_select_border", "pinned", True)

		layout.itemS()
		
		layout.itemO("UV_OT_select_all_toggle")
		layout.itemO("UV_OT_select_inverse")
		layout.itemO("UV_OT_unlink_selection")
		
		layout.itemS()

		layout.itemO("UV_OT_select_pinned")
		layout.itemO("UV_OT_select_linked")

class IMAGE_MT_image(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "Image"

	def draw(self, context):
		layout = self.layout
		sima = context.space_data
		ima = sima.image

		layout.itemO("IMAGE_OT_new")
		layout.itemO("IMAGE_OT_open")

		show_render = sima.show_render

		if ima:
			if show_render:
				layout.itemO("IMAGE_OT_replace")
				layout.itemO("IMAGE_OT_reload")

			layout.itemO("IMAGE_OT_save")
			layout.itemO("IMAGE_OT_save_as")

			if ima.source == "SEQUENCE":
				layout.itemO("IMAGE_OT_save_sequence")

			if not show_render:
				layout.itemS()

				if ima.packed_file:
					layout.itemO("IMAGE_OT_unpack")
				else:
					layout.itemO("IMAGE_OT_pack")

				# only for dirty && specific image types, perhaps
				# this could be done in operator poll too
				if ima.dirty:
					if ima.source in ("FILE", "GENERATED") and ima.type != "MULTILAYER":
						layout.item_booleanO("IMAGE_OT_pack", "as_png", True, text="Pack As PNG")

			layout.itemS()

			layout.itemR(sima, "image_painting")

class IMAGE_MT_uvs_showhide(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "Show/Hide Faces"

	def draw(self, context):
		layout = self.layout

		layout.itemO("UV_OT_reveal")
		layout.itemO("UV_OT_hide")
		layout.item_booleanO("UV_OT_hide", "unselected", True)

class IMAGE_MT_uvs_transform(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "Transform"

	def draw(self, context):
		layout = self.layout

		layout.item_enumO("TFM_OT_transform", "mode", "TRANSLATION")
		layout.item_enumO("TFM_OT_transform", "mode", "ROTATION")
		layout.item_enumO("TFM_OT_transform", "mode", "RESIZE")

class IMAGE_MT_uvs_mirror(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "Mirror"

	def draw(self, context):
		layout = self.layout

		layout.item_enumO("UV_OT_mirror", "axis", "MIRROR_X") # "X Axis", M, 
		layout.item_enumO("UV_OT_mirror", "axis", "MIRROR_Y") # "Y Axis", M, 

class IMAGE_MT_uvs_weldalign(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "Weld/Align"

	def draw(self, context):
		layout = self.layout

		layout.itemO("UV_OT_weld") # W, 1
		layout.items_enumO("UV_OT_align", "axis") # W, 2/3/4


class IMAGE_MT_uvs(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "UVs"

	def draw(self, context):
		layout = self.layout
		sima = context.space_data
		uv = sima.uv_editor
		settings = context.scene.tool_settings

		layout.itemR(uv, "snap_to_pixels")
		layout.itemR(uv, "constrain_to_image_bounds")

		layout.itemS()

		layout.itemR(uv, "live_unwrap")
		layout.itemO("UV_OT_unwrap")
		layout.item_booleanO("UV_OT_pin", "clear", True, text="Unpin")
		layout.itemO("UV_OT_pin")

		layout.itemS()

		layout.itemO("UV_OT_pack_islands")
		layout.itemO("UV_OT_average_islands_scale")
		layout.itemO("UV_OT_minimize_stretch")
		layout.itemO("UV_OT_stitch")

		layout.itemS()

		layout.itemM("IMAGE_MT_uvs_transform")
		layout.itemM("IMAGE_MT_uvs_mirror")
		layout.itemM("IMAGE_MT_uvs_weldalign")

		layout.itemS()

		layout.itemR(settings, "proportional_editing")
		layout.item_menu_enumR(settings, "proportional_editing_falloff")

		layout.itemS()

		layout.itemM("IMAGE_MT_uvs_showhide")

class IMAGE_HT_header(bpy.types.Header):
	__space_type__ = "IMAGE_EDITOR"

	def draw(self, context):
		sima = context.space_data
		ima = sima.image
		iuser = sima.image_user
		layout = self.layout
		settings = context.scene.tool_settings

		show_render = sima.show_render
		show_paint = sima.show_paint
		show_uvedit = sima.show_uvedit

		layout.template_header()

		# menus
		if context.area.show_menus:
			row = layout.row()
			row.itemM("IMAGE_MT_view")

			if show_uvedit:
				row.itemM("IMAGE_MT_select")

			if ima and ima.dirty:
				row.itemM("IMAGE_MT_image", text="Image*")
			else:
				row.itemM("IMAGE_MT_image", text="Image")

			if show_uvedit:
				row.itemM("IMAGE_MT_uvs")

		layout.template_ID(sima, "image", new="IMAGE_OT_new", open="IMAGE_OT_open")

		"""
		/* image select */

		pinflag= (show_render)? 0: UI_ID_PIN;
		xco= uiDefIDPoinButs(block, CTX_data_main(C), NULL, (ID*)sima->image, ID_IM, &sima->pin, xco, yco,
			sima_idpoin_handle, UI_ID_BROWSE|UI_ID_BROWSE_RENDER|UI_ID_RENAME|UI_ID_ADD_NEW|UI_ID_OPEN|UI_ID_DELETE|pinflag);
		xco += 8;
		"""

		"""
		if(ima && !ELEM3(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE, IMA_SRC_VIEWER) && ima->ok) {
			/* XXX this should not be a static var */
			static int headerbuttons_packdummy;
			
			headerbuttons_packdummy = 0;

			if (ima->packedfile) {
				headerbuttons_packdummy = 1;
			}
			if (ima->packedfile && ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
				uiDefIconButBitI(block, TOG, 1, 0 /* XXX B_SIMA_REPACK */, ICON_UGLYPACKAGE,	xco,yco,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Re-Pack this image as PNG");
			else
				uiDefIconButBitI(block, TOG, 1, 0 /* XXX B_SIMAPACKIMA */, ICON_PACKAGE,	xco,yco,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Pack/Unpack this image");
				
			xco+= XIC+8;
		}
		"""

		# uv editing
		if show_uvedit:
			uvedit = sima.uv_editor

			layout.itemR(uvedit, "pivot", text="")
			layout.itemR(settings, "uv_sync_selection", text="")

			if settings.uv_sync_selection:
				layout.itemR(settings, "mesh_selection_mode", text="", expand=True)
			else:
				layout.itemR(settings, "uv_selection_mode", text="", expand=True)
				layout.itemR(uvedit, "sticky_selection_mode", text="")
			pass

			row = layout.row(align=True)
			row.itemR(settings, "snap", text="")
			if settings.snap:
				row.itemR(settings, "snap_mode", text="")

			"""
			mesh = context.edit_object.data
			row.item_pointerR(mesh, "active_uv_layer", mesh, "uv_layers")
			"""

		if ima:
			# layers
			layout.template_image_layers(ima, iuser)

			# painting
			layout.itemR(sima, "image_painting", text="")

			# draw options
			row = layout.row(align=True)
			row.itemR(sima, "draw_channels", text="", expand=True)

			row = layout.row(align=True)
			if ima.type == "COMPOSITE":
				row.itemO("IMAGE_OT_record_composite", icon="ICON_REC")
			if ima.type == "COMPOSITE" and ima.source in ("MOVIE", "SEQUENCE"):
				row.itemO("IMAGE_OT_play_composite", icon="ICON_PLAY")
		
		layout.itemR(sima, "update_automatically", text="")

class IMAGE_PT_game_properties(bpy.types.Panel):
	__space_type__ = "IMAGE_EDITOR"
	__region_type__ = "UI"
	__label__ = "Game Properties"

	def poll(self, context):
		sima = context.space_data
		return (sima and sima.image)

	def draw(self, context):
		sima = context.space_data
		layout = self.layout

		ima = sima.image

		if ima:
			split = layout.split()

			col = split.column()

			subcol = col.column(align=True)
			subcol.itemR(ima, "clamp_x")
			subcol.itemR(ima, "clamp_y")

			col.itemR(ima, "mapping", expand=True)
			col.itemR(ima, "tiles")

			col = split.column()

			subcol = col.column(align=True)
			subcol.itemR(ima, "animated")

			subcol = subcol.column()
			subcol.itemR(ima, "animation_start", text="Start")
			subcol.itemR(ima, "animation_end", text="End")
			subcol.itemR(ima, "animation_speed", text="Speed")
			subcol.active = ima.animated

			subrow = col.row(align=True)
			subrow.itemR(ima, "tiles_x", text="X")
			subrow.itemR(ima, "tiles_y", text="Y")
			subrow.active = ima.tiles or ima.animated

class IMAGE_PT_view_properties(bpy.types.Panel):
	__space_type__ = "IMAGE_EDITOR"
	__region_type__ = "UI"
	__label__ = "View Properties"

	def poll(self, context):
		sima = context.space_data
		return (sima and (sima.image or sima.show_uvedit))

	def draw(self, context):
		sima = context.space_data
		layout = self.layout

		ima = sima.image
		show_uvedit = sima.show_uvedit
		uvedit = sima.uv_editor

		split = layout.split()

		col = split.column()
		if ima:
			col.itemR(ima, "display_aspect")

			col = split.column()
			col.itemR(sima, "draw_repeated", text="Repeat")
			if show_uvedit:
				col.itemR(uvedit, "normalized_coordinates", text="Normalized")
		elif show_uvedit:
			col.itemR(uvedit, "normalized_coordinates", text="Normalized")

		if show_uvedit:
			col = layout.column()
			row = col.row()
			row.itemR(uvedit, "edge_draw_type", expand=True)
			row = col.row()
			row.itemR(uvedit, "draw_smooth_edges", text="Smooth")
			row.itemR(uvedit, "draw_modified_edges", text="Modified")

			row = col.row()
			row.itemR(uvedit, "draw_stretch", text="Stretch")
			row.itemR(uvedit, "draw_stretch_type", text="")
			#col.itemR(uvedit, "draw_edges")
			#col.itemR(uvedit, "draw_faces")

bpy.types.register(IMAGE_MT_view)
bpy.types.register(IMAGE_MT_select)
bpy.types.register(IMAGE_MT_image)
bpy.types.register(IMAGE_MT_uvs_showhide)
bpy.types.register(IMAGE_MT_uvs_transform)
bpy.types.register(IMAGE_MT_uvs_mirror)
bpy.types.register(IMAGE_MT_uvs_weldalign)
bpy.types.register(IMAGE_MT_uvs)
bpy.types.register(IMAGE_HT_header)
bpy.types.register(IMAGE_PT_game_properties)
bpy.types.register(IMAGE_PT_view_properties)

