
import bpy

class IMAGE_MT_view(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout
		sima = context.space_data
		uv = sima.uv_editor

		show_uvedit = sima.show_uvedit

		layout.itemO("IMAGE_OT_properties") # icon

		layout.itemS()

		layout.itemR(sima, "update_automatically")
		# XXX if show_uvedit:
		# XXX	layout.itemR(uv, "local_view") # "UV Local View", Numpad /

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
		layout.itemO("UV_OT_select_invert")
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

				# only for dirty && specific image types : XXX poll?
				#if(ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
				if False:
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
		scene = context.scene

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

		layout.itemM(context, "IMAGE_MT_uvs_transform")
		layout.itemM(context, "IMAGE_MT_uvs_mirror")
		layout.itemM(context, "IMAGE_MT_uvs_weldalign")

		layout.itemS()

		# XXX layout.itemR(scene, "proportional_editing")
		layout.item_menu_enumR(scene, "proportional_editing_falloff")

		layout.itemS()

		layout.itemM(context, "IMAGE_MT_uvs_showhide")

class IMAGE_HT_header(bpy.types.Header):
	__space_type__ = "IMAGE_EDITOR"

	def draw(self, context):
		sima = context.space_data
		ima = sima.image
		layout = self.layout

		show_render = sima.show_render
		show_paint = sima.show_paint
		show_uvedit = sima.show_uvedit

		layout.template_header(context)

		# menus
		if context.area.show_menus:
			row = layout.row()
			row.itemM(context, "IMAGE_MT_view")

			if show_uvedit:
				row.itemM(context, "IMAGE_MT_select")

			# XXX menuname= (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))? "Image*": "Image";
			row.itemM(context, "IMAGE_MT_image")

			if show_uvedit:
				row.itemM(context, "IMAGE_MT_uvs")

		layout.template_ID(context, sima, "image", new="IMAGE_OT_new", open="IMAGE_OT_open")

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
			pass
		
		"""
		/* uv editing */
		if(show_uvedit) {
			/* pivot */
			uiDefIconTextButS(block, ICONTEXTROW, B_NOP, ICON_ROTATE,
					"Pivot: %t|Bounding Box Center %x0|Median Point %x3|2D Cursor %x1",
					xco,yco,XIC+10,YIC, &ar->v2d.around, 0, 3.0, 0, 0,
					"Rotation/Scaling Pivot (Hotkeys: Comma, Shift Comma, Period)");
			xco+= XIC + 18;
			
			/* selection modes */
			uiDefIconButBitS(block, TOG, UV_SYNC_SELECTION, B_REDR, ICON_EDIT, xco,yco,XIC,YIC, &scene->toolsettings->uv_flag, 0, 0, 0, 0, "Sync UV and Mesh Selection");
			xco+= XIC+8;

			if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
				uiBlockBeginAlign(block);
				
				uiDefIconButBitS(block, TOG, SCE_SELECT_VERTEX, B_REDR, ICON_VERTEXSEL,
					xco,yco,XIC,YIC, &scene->selectmode, 1.0, 0.0, 0, 0, "Vertex select mode");
				uiDefIconButBitS(block, TOG, SCE_SELECT_EDGE, B_REDR, ICON_EDGESEL,
					xco+=XIC,yco,XIC,YIC, &scene->selectmode, 1.0, 0.0, 0, 0, "Edge select mode");
				uiDefIconButBitS(block, TOG, SCE_SELECT_FACE, B_REDR, ICON_FACESEL,
					xco+=XIC,yco,XIC,YIC, &scene->selectmode, 1.0, 0.0, 0, 0, "Face select mode");

				uiBlockEndAlign(block);
			}
			else {
				uiBlockBeginAlign(block);

				uiDefIconButS(block, ROW, B_REDR, ICON_VERTEXSEL,
					xco,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_VERTEX, 0, 0, "Vertex select mode");
				uiDefIconButS(block, ROW, B_REDR, ICON_EDGESEL,
					xco+=XIC,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_EDGE, 0, 0, "Edge select mode");
				uiDefIconButS(block, ROW, B_REDR, ICON_FACESEL,
					xco+=XIC,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_FACE, 0, 0, "Face select mode");
				uiDefIconButS(block, ROW, B_REDR, ICON_LINKEDSEL,
					xco+=XIC,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_ISLAND, 0, 0, "Island select mode");

				uiBlockEndAlign(block);

				/* would use these if const's could go in strings 
				 * SI_STICKY_LOC SI_STICKY_DISABLE SI_STICKY_VERTEX */
				but = uiDefIconTextButC(block, ICONTEXTROW, B_REDR, ICON_STICKY_UVS_LOC,
						"Sticky UV Selection: %t|Disable%x1|Shared Location%x0|Shared Vertex%x2",
						xco+=XIC+10,yco,XIC+10,YIC, &(sima->sticky), 0, 3.0, 0, 0,
						"Sticky UV Selection (Hotkeys: Shift C, Alt C, Ctrl C)");
			}

			xco+= XIC + 16;
			
			/* snap options, identical to options in 3d view header */
			uiBlockBeginAlign(block);

			if (scene->snap_flag & SCE_SNAP) {
				uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEO,xco,yco,XIC,YIC, &scene->snap_flag, 0, 0, 0, 0, "Use Snap or Grid (Shift Tab).");
				xco+= XIC;
				uiDefButS(block, MENU, B_NOP, "Mode%t|Closest%x0|Center%x1|Median%x2",xco,yco,70,YIC, &scene->snap_target, 0, 0, 0, 0, "Snap Target Mode.");
				xco+= 70;
			}
			else {
				uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEAR,xco,yco,XIC,YIC, &scene->snap_flag, 0, 0, 0, 0, "Snap while Ctrl is held during transform (Shift Tab).");	
				xco+= XIC;
			}

			uiBlockEndAlign(block);
			xco+= 8;

			/* uv layers */
			{
				Object *obedit= CTX_data_edit_object(C);
				char menustr[34*MAX_MTFACE];
				static int act;
				
				image_menu_uvlayers(obedit, menustr, &act);

				but = uiDefButI(block, MENU, B_NOP, menustr ,xco,yco,85,YIC, &act, 0, 0, 0, 0, "Active UV Layer for editing.");
				// uiButSetFunc(but, do_image_buttons_set_uvlayer_callback, &act, NULL);
				
				xco+= 85;
			}

			xco+= 8;
		}
		"""

		if ima:
			"""
			RenderResult *rr;
		
			/* render layers and passes */
			rr= BKE_image_get_renderresult(scene, ima);
			if(rr) {
				uiBlockBeginAlign(block);
#if 0
				uiblock_layer_pass_buttons(block, rr, &sima->iuser, B_REDR, xco, 0, 160);
#endif
				uiBlockEndAlign(block);
				xco+= 166;
			}
			"""

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

	def draw(self, context):
		sima = context.space_data
		layout = self.layout

		ima = sima.image

		if ima:
			split = layout.split()

			col = split.column(align=True)
			col.itemR(ima, "animated")

			subcol = col.column()
			subcol.itemR(ima, "animation_start", text="Start")
			subcol.itemR(ima, "animation_end", text="End")
			subcol.itemR(ima, "animation_speed", text="Speed")
			subcol.active = ima.animated

			col = split.column()
			col.itemR(ima, "tiles")

			subrow = col.row(align=True)
			subrow.itemR(ima, "tiles_x", text="X")
			subrow.itemR(ima, "tiles_y", text="Y")
			subrow.active = ima.tiles

			col.itemS()
			col.itemR(ima, "clamp_x")
			col.itemR(ima, "clamp_y")

			col.itemR(ima, "mapping", expand=True)

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


import bpy

class IMAGE_MT_view(bpy.types.Menu):
	__space_type__ = "IMAGE_EDITOR"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout
		sima = context.space_data
		uv = sima.uv_editor

		show_uvedit = sima.show_uvedit

		layout.itemO("IMAGE_OT_properties") # icon

		layout.itemS()

		layout.itemR(sima, "update_automatically")
		# XXX if show_uvedit:
		# XXX	layout.itemR(uv, "local_view") # "UV Local View", Numpad /

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
		layout.itemO("UV_OT_select_invert")
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

				# only for dirty && specific image types : XXX poll?
				#if(ibuf && (ibuf->userflags & IB_BITMAPDIRTY))
				if False:
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
		scene = context.scene

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

		layout.itemM(context, "IMAGE_MT_uvs_transform")
		layout.itemM(context, "IMAGE_MT_uvs_mirror")
		layout.itemM(context, "IMAGE_MT_uvs_weldalign")

		layout.itemS()

		# XXX layout.itemR(scene, "proportional_editing")
		layout.item_menu_enumR(scene, "proportional_editing_falloff")

		layout.itemS()

		layout.itemM(context, "IMAGE_MT_uvs_showhide")

class IMAGE_HT_header(bpy.types.Header):
	__space_type__ = "IMAGE_EDITOR"

	def draw(self, context):
		sima = context.space_data
		ima = sima.image
		layout = self.layout

		show_render = sima.show_render
		show_paint = sima.show_paint
		show_uvedit = sima.show_uvedit

		layout.template_header(context)

		# menus
		if context.area.show_menus:
			row = layout.row()
			row.itemM(context, "IMAGE_MT_view")

			if show_uvedit:
				row.itemM(context, "IMAGE_MT_select")

			# XXX menuname= (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))? "Image*": "Image";
			row.itemM(context, "IMAGE_MT_image")

			if show_uvedit:
				row.itemM(context, "IMAGE_MT_uvs")

		layout.template_ID(context, sima, "image", new="IMAGE_OT_new", open="IMAGE_OT_open")

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
			pass
		
		"""
		/* uv editing */
		if(show_uvedit) {
			/* pivot */
			uiDefIconTextButS(block, ICONTEXTROW, B_NOP, ICON_ROTATE,
					"Pivot: %t|Bounding Box Center %x0|Median Point %x3|2D Cursor %x1",
					xco,yco,XIC+10,YIC, &ar->v2d.around, 0, 3.0, 0, 0,
					"Rotation/Scaling Pivot (Hotkeys: Comma, Shift Comma, Period)");
			xco+= XIC + 18;
			
			/* selection modes */
			uiDefIconButBitS(block, TOG, UV_SYNC_SELECTION, B_REDR, ICON_EDIT, xco,yco,XIC,YIC, &scene->toolsettings->uv_flag, 0, 0, 0, 0, "Sync UV and Mesh Selection");
			xco+= XIC+8;

			if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
				uiBlockBeginAlign(block);
				
				uiDefIconButBitS(block, TOG, SCE_SELECT_VERTEX, B_REDR, ICON_VERTEXSEL,
					xco,yco,XIC,YIC, &scene->selectmode, 1.0, 0.0, 0, 0, "Vertex select mode");
				uiDefIconButBitS(block, TOG, SCE_SELECT_EDGE, B_REDR, ICON_EDGESEL,
					xco+=XIC,yco,XIC,YIC, &scene->selectmode, 1.0, 0.0, 0, 0, "Edge select mode");
				uiDefIconButBitS(block, TOG, SCE_SELECT_FACE, B_REDR, ICON_FACESEL,
					xco+=XIC,yco,XIC,YIC, &scene->selectmode, 1.0, 0.0, 0, 0, "Face select mode");

				uiBlockEndAlign(block);
			}
			else {
				uiBlockBeginAlign(block);

				uiDefIconButS(block, ROW, B_REDR, ICON_VERTEXSEL,
					xco,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_VERTEX, 0, 0, "Vertex select mode");
				uiDefIconButS(block, ROW, B_REDR, ICON_EDGESEL,
					xco+=XIC,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_EDGE, 0, 0, "Edge select mode");
				uiDefIconButS(block, ROW, B_REDR, ICON_FACESEL,
					xco+=XIC,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_FACE, 0, 0, "Face select mode");
				uiDefIconButS(block, ROW, B_REDR, ICON_LINKEDSEL,
					xco+=XIC,yco,XIC,YIC, &scene->toolsettings->uv_selectmode, 1.0, UV_SELECT_ISLAND, 0, 0, "Island select mode");

				uiBlockEndAlign(block);

				/* would use these if const's could go in strings 
				 * SI_STICKY_LOC SI_STICKY_DISABLE SI_STICKY_VERTEX */
				but = uiDefIconTextButC(block, ICONTEXTROW, B_REDR, ICON_STICKY_UVS_LOC,
						"Sticky UV Selection: %t|Disable%x1|Shared Location%x0|Shared Vertex%x2",
						xco+=XIC+10,yco,XIC+10,YIC, &(sima->sticky), 0, 3.0, 0, 0,
						"Sticky UV Selection (Hotkeys: Shift C, Alt C, Ctrl C)");
			}

			xco+= XIC + 16;
			
			/* snap options, identical to options in 3d view header */
			uiBlockBeginAlign(block);

			if (scene->snap_flag & SCE_SNAP) {
				uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEO,xco,yco,XIC,YIC, &scene->snap_flag, 0, 0, 0, 0, "Use Snap or Grid (Shift Tab).");
				xco+= XIC;
				uiDefButS(block, MENU, B_NOP, "Mode%t|Closest%x0|Center%x1|Median%x2",xco,yco,70,YIC, &scene->snap_target, 0, 0, 0, 0, "Snap Target Mode.");
				xco+= 70;
			}
			else {
				uiDefIconButBitS(block, TOG, SCE_SNAP, B_REDR, ICON_SNAP_GEAR,xco,yco,XIC,YIC, &scene->snap_flag, 0, 0, 0, 0, "Snap while Ctrl is held during transform (Shift Tab).");	
				xco+= XIC;
			}

			uiBlockEndAlign(block);
			xco+= 8;

			/* uv layers */
			{
				Object *obedit= CTX_data_edit_object(C);
				char menustr[34*MAX_MTFACE];
				static int act;
				
				image_menu_uvlayers(obedit, menustr, &act);

				but = uiDefButI(block, MENU, B_NOP, menustr ,xco,yco,85,YIC, &act, 0, 0, 0, 0, "Active UV Layer for editing.");
				// uiButSetFunc(but, do_image_buttons_set_uvlayer_callback, &act, NULL);
				
				xco+= 85;
			}

			xco+= 8;
		}
		"""

		if ima:
			"""
			RenderResult *rr;
		
			/* render layers and passes */
			rr= BKE_image_get_renderresult(scene, ima);
			if(rr) {
				uiBlockBeginAlign(block);
#if 0
				uiblock_layer_pass_buttons(block, rr, &sima->iuser, B_REDR, xco, 0, 160);
#endif
				uiBlockEndAlign(block);
				xco+= 166;
			}
			"""

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

	def draw(self, context):
		sima = context.space_data
		layout = self.layout

		ima = sima.image

		if ima:
			split = layout.split()

			col = split.column(align=True)
			col.itemR(ima, "animated")

			subcol = col.column()
			subcol.itemR(ima, "animation_start", text="Start")
			subcol.itemR(ima, "animation_end", text="End")
			subcol.itemR(ima, "animation_speed", text="Speed")
			subcol.active = ima.animated

			col = split.column()
			col.itemR(ima, "tiles")

			subrow = col.row(align=True)
			subrow.itemR(ima, "tiles_x", text="X")
			subrow.itemR(ima, "tiles_y", text="Y")
			subrow.active = ima.tiles

			col.itemS()
			col.itemR(ima, "clamp_x")
			col.itemR(ima, "clamp_y")

			col.itemR(ima, "mapping", expand=True)

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

