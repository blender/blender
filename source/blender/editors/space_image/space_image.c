/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_image/space_image.c
 *  \ingroup spimage
 */

#include "DNA_gpencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_mask_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_image_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "IMB_imbuf_types.h"

#include "ED_image.h"
#include "ED_mask.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_render.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_uvedit.h"

#include "BIF_gl.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_interface.h"
#include "UI_view2d.h"

#include "image_intern.h"

/**************************** common state *****************************/

static void image_scopes_tag_refresh(ScrArea *sa)
{
	SpaceImage *sima = (SpaceImage *)sa->spacedata.first;
	ARegion *ar;

	/* only while histogram is visible */
	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_TOOLS && ar->flag & RGN_FLAG_HIDDEN)
			return;
	}

	sima->scopes.ok = 0;
}

static void image_user_refresh_scene(const bContext *C, SpaceImage *sima)
{
	if (sima->image && sima->image->type == IMA_TYPE_R_RESULT) {
		/* for render result, try to use the currently rendering scene */
		Scene *render_scene = ED_render_job_get_current_scene(C);
		if (render_scene) {
			sima->iuser.scene = render_scene;
			return;
		}
	}
	sima->iuser.scene = CTX_data_scene(C);
}

/* ******************** manage regions ********************* */

ARegion *image_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar = BKE_area_find_region_type(sa, RGN_TYPE_UI);
	if (ar) return ar;
	
	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

	/* is error! */
	if (ar == NULL) return NULL;
	
	arnew = MEM_callocN(sizeof(ARegion), "buttons for image");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_UI;
	arnew->alignment = RGN_ALIGN_RIGHT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

ARegion *image_has_tools_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar = BKE_area_find_region_type(sa, RGN_TYPE_TOOLS);
	if (ar) return ar;

	/* add subdiv level; after buttons */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_UI);

	/* is error! */
	if (ar == NULL) return NULL;
	
	arnew = MEM_callocN(sizeof(ARegion), "scopes for image");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_TOOLS;
	arnew->alignment = RGN_ALIGN_LEFT;
	
	arnew->flag = RGN_FLAG_HIDDEN;

	image_scopes_tag_refresh(sa);
	
	return arnew;
}

/* ******************** default callbacks for image space ***************** */

static SpaceLink *image_new(const bContext *UNUSED(C))
{
	ARegion *ar;
	SpaceImage *simage;
	
	simage = MEM_callocN(sizeof(SpaceImage), "initimage");
	simage->spacetype = SPACE_IMAGE;
	simage->zoom = 1.0f;
	simage->lock = true;
	simage->flag = SI_SHOW_GPENCIL | SI_USE_ALPHA;

	simage->iuser.ok = true;
	simage->iuser.fie_ima = 2;
	simage->iuser.frames = 100;
	simage->iuser.flag = IMA_SHOW_STEREO;

	scopes_new(&simage->scopes);
	simage->sample_line_hist.height = 100;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;
	
	/* buttons/list view */
	ar = MEM_callocN(sizeof(ARegion), "buttons for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype = RGN_TYPE_UI;
	ar->alignment = RGN_ALIGN_RIGHT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* scopes/uv sculpt/paint */
	ar = MEM_callocN(sizeof(ARegion), "buttons for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype = RGN_TYPE_TOOLS;
	ar->alignment = RGN_ALIGN_LEFT;
	ar->flag = RGN_FLAG_HIDDEN;

	/* main area */
	ar = MEM_callocN(sizeof(ARegion), "main area for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	
	return (SpaceLink *)simage;
}

/* not spacelink itself */
static void image_free(SpaceLink *sl)
{	
	SpaceImage *simage = (SpaceImage *) sl;

	scopes_free(&simage->scopes);
}


/* spacetype; init callback, add handlers */
static void image_init(struct wmWindowManager *UNUSED(wm), ScrArea *sa)
{
	ListBase *lb = WM_dropboxmap_find("Image", SPACE_IMAGE, 0);

	/* add drop boxes */
	WM_event_add_dropbox_handler(&sa->handlers, lb);
	
}

static SpaceLink *image_duplicate(SpaceLink *sl)
{
	SpaceImage *simagen = MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */

	scopes_new(&simagen->scopes);

	return (SpaceLink *)simagen;
}

static void image_operatortypes(void)
{
	WM_operatortype_append(IMAGE_OT_view_all);
	WM_operatortype_append(IMAGE_OT_view_pan);
	WM_operatortype_append(IMAGE_OT_view_selected);
	WM_operatortype_append(IMAGE_OT_view_zoom);
	WM_operatortype_append(IMAGE_OT_view_zoom_in);
	WM_operatortype_append(IMAGE_OT_view_zoom_out);
	WM_operatortype_append(IMAGE_OT_view_zoom_ratio);
	WM_operatortype_append(IMAGE_OT_view_zoom_border);
#ifdef WITH_INPUT_NDOF
	WM_operatortype_append(IMAGE_OT_view_ndof);
#endif

	WM_operatortype_append(IMAGE_OT_new);
	WM_operatortype_append(IMAGE_OT_open);
	WM_operatortype_append(IMAGE_OT_match_movie_length);
	WM_operatortype_append(IMAGE_OT_replace);
	WM_operatortype_append(IMAGE_OT_reload);
	WM_operatortype_append(IMAGE_OT_save);
	WM_operatortype_append(IMAGE_OT_save_as);
	WM_operatortype_append(IMAGE_OT_save_sequence);
	WM_operatortype_append(IMAGE_OT_pack);
	WM_operatortype_append(IMAGE_OT_unpack);
	
	WM_operatortype_append(IMAGE_OT_invert);

	WM_operatortype_append(IMAGE_OT_cycle_render_slot);

	WM_operatortype_append(IMAGE_OT_sample);
	WM_operatortype_append(IMAGE_OT_sample_line);
	WM_operatortype_append(IMAGE_OT_curves_point_set);

	WM_operatortype_append(IMAGE_OT_properties);
	WM_operatortype_append(IMAGE_OT_toolshelf);

	WM_operatortype_append(IMAGE_OT_change_frame);

	WM_operatortype_append(IMAGE_OT_read_renderlayers);
	WM_operatortype_append(IMAGE_OT_render_border);
	WM_operatortype_append(IMAGE_OT_clear_render_border);
}

static void image_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Image Generic", SPACE_IMAGE, 0);
	wmKeyMapItem *kmi;
	int i;
	
	WM_keymap_add_item(keymap, "IMAGE_OT_new", NKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_open", OKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_reload", RKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_read_renderlayers", RKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_save", SKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_save_as", F3KEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_properties", NKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_toolshelf", TKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "IMAGE_OT_cycle_render_slot", JKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "IMAGE_OT_cycle_render_slot", JKEY, KM_PRESS, KM_ALT, 0)->ptr, "reverse", true);
	
	keymap = WM_keymap_find(keyconf, "Image", SPACE_IMAGE, 0);
	
	WM_keymap_add_item(keymap, "IMAGE_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "IMAGE_OT_view_all", HOMEKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "fit_view", true);

	WM_keymap_add_item(keymap, "IMAGE_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_pan", MIDDLEMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_pan", MIDDLEMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_pan", MOUSEPAN, 0, 0, 0);

#ifdef WITH_INPUT_NDOF
	WM_keymap_add_item(keymap, "IMAGE_OT_view_all", NDOF_BUTTON_FIT, KM_PRESS, 0, 0); // or view selected?
	WM_keymap_add_item(keymap, "IMAGE_OT_view_ndof", NDOF_MOTION, 0, 0, 0);
#endif

	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_in", WHEELINMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_out", WHEELOUTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_in", PADPLUSKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_out", PADMINUS, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom", MIDDLEMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom", MOUSEZOOM, 0, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom", MOUSEPAN, 0, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_border", BKEY, KM_PRESS, KM_SHIFT, 0);

	/* ctrl now works as well, shift + numpad works as arrow keys on Windows */
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD8, KM_PRESS, KM_CTRL, 0)->ptr, "ratio", 8.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD4, KM_PRESS, KM_CTRL, 0)->ptr, "ratio", 4.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD2, KM_PRESS, KM_CTRL, 0)->ptr, "ratio", 2.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD8, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 8.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD4, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 4.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD2, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 2.0f);

	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD1, KM_PRESS, 0, 0)->ptr, "ratio", 1.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD2, KM_PRESS, 0, 0)->ptr, "ratio", 0.5f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD4, KM_PRESS, 0, 0)->ptr, "ratio", 0.25f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD8, KM_PRESS, 0, 0)->ptr, "ratio", 0.125f);

	WM_keymap_add_item(keymap, "IMAGE_OT_change_frame", LEFTMOUSE, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "IMAGE_OT_sample", ACTIONMOUSE, KM_PRESS, 0, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "IMAGE_OT_curves_point_set", ACTIONMOUSE, KM_PRESS, KM_CTRL, 0)->ptr, "point", 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "IMAGE_OT_curves_point_set", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "point", 1);

	/* toggle editmode is handy to have while UV unwrapping */
	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_mode_set", TABKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", OB_MODE_EDIT);
	RNA_boolean_set(kmi->ptr, "toggle", true);

	/* fast switch to render slots */
	for (i = 0; i < MIN2(IMA_MAX_RENDER_SLOT, 9); i++) {
		kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_int", ONEKEY + i, KM_PRESS, 0, 0);
		RNA_string_set(kmi->ptr, "data_path", "space_data.image.render_slots.active_index");
		RNA_int_set(kmi->ptr, "value", i);
	}

	/* pivot */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", COMMAKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.pivot_point");
	RNA_string_set(kmi->ptr, "value", "CENTER");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", COMMAKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.pivot_point");
	RNA_string_set(kmi->ptr, "value", "MEDIAN");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", PERIODKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.pivot_point");
	RNA_string_set(kmi->ptr, "value", "CURSOR");

	/* render border */
	WM_keymap_add_item(keymap, "IMAGE_OT_render_border", BKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_clear_render_border", BKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
}

/* dropboxes */
static int image_drop_poll(bContext *UNUSED(C), wmDrag *drag, const wmEvent *UNUSED(event))
{
	if (drag->type == WM_DRAG_PATH)
		if (ELEM(drag->icon, 0, ICON_FILE_IMAGE, ICON_FILE_MOVIE, ICON_FILE_BLANK)) /* rule might not work? */
			return 1;
	return 0;
}

static void image_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	/* copy drag path to properties */
	RNA_string_set(drop->ptr, "filepath", drag->path);
}

/* area+region dropbox definition */
static void image_dropboxes(void)
{
	ListBase *lb = WM_dropboxmap_find("Image", SPACE_IMAGE, 0);
	
	WM_dropbox_add(lb, "IMAGE_OT_open", image_drop_poll, image_drop_copy);
}

/**
 * \note take care not to get into feedback loop here,
 *       calling composite job causes viewer to refresh.
 */
static void image_refresh(const bContext *C, ScrArea *sa)
{
	Scene *scene = CTX_data_scene(C);
	SpaceImage *sima = sa->spacedata.first;
	Object *obedit = CTX_data_edit_object(C);
	Image *ima;

	ima = ED_space_image(sima);

	BKE_image_user_check_frame_calc(&sima->iuser, scene->r.cfra, 0);
	
	/* check if we have to set the image from the editmesh */
	if (ima && (ima->source == IMA_SRC_VIEWER && sima->mode == SI_MODE_MASK)) {
		if (scene->nodetree) {
			Mask *mask = ED_space_image_get_mask(sima);
			if (mask) {
				ED_node_composite_job(C, scene->nodetree, scene);
			}
		}
	}
	else if (ima && (ima->source == IMA_SRC_VIEWER || sima->pin)) {
		/* pass */
	}
	else if (obedit && obedit->type == OB_MESH) {
		Mesh *me = (Mesh *)obedit->data;
		struct BMEditMesh *em = me->edit_btmesh;
		bool sloppy = true; /* partially selected face is ok */
		bool selected = !(scene->toolsettings->uv_flag & UV_SYNC_SELECTION); /* only selected active face? */

		if (BKE_scene_use_new_shading_nodes(scene)) {
			/* new shading system does not alter image */
		}
		else {
			/* old shading system, we set texface */
			MTexPoly *tf;
			
			if (em && EDBM_mtexpoly_check(em)) {
				tf = EDBM_mtexpoly_active_get(em, NULL, sloppy, selected);

				if (tf) {
					/* don't need to check for pin here, see above */
					sima->image = tf->tpage;
					
					if ((sima->flag & SI_EDITTILE) == 0) {
						sima->curtile = tf->tile;
					}
				}
			}
		}
	}
}

static void image_listener(bScreen *sc, ScrArea *sa, wmNotifier *wmn)
{
	Scene *scene = sc->scene;
	SpaceImage *sima = (SpaceImage *)sa->spacedata.first;
	
	/* context changes */
	switch (wmn->category) {
		case NC_WINDOW:
			/* notifier comes from editing color space */
			image_scopes_tag_refresh(sa);
			ED_area_tag_redraw(sa);
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_FRAME:
					image_scopes_tag_refresh(sa);
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
				case ND_MODE:
					if (wmn->subtype == NS_EDITMODE_MESH)
						ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
				case ND_RENDER_RESULT:
				case ND_RENDER_OPTIONS:
				case ND_COMPO_RESULT:
					if (ED_space_image_show_render(sima))
						image_scopes_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_IMAGE:
			if (wmn->reference == sima->image || !wmn->reference) {
				if (wmn->action != NA_PAINTING) {
					image_scopes_tag_refresh(sa);
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);
				}
			}
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_IMAGE) {
				image_scopes_tag_refresh(sa);
				ED_area_tag_redraw(sa);
			}
			break;
		case NC_MASK:
		{
			// Scene *scene = wmn->window->screen->scene;
			/* ideally would check for: ED_space_image_check_show_maskedit(scene, sima) but we cant get the scene */
			if (sima->mode == SI_MODE_MASK) {
				switch (wmn->data) {
					case ND_SELECT:
						ED_area_tag_redraw(sa);
						break;
					case ND_DATA:
					case ND_DRAW:
						/* causes node-recalc */
						ED_area_tag_redraw(sa);
						ED_area_tag_refresh(sa);
						break;
				}
				switch (wmn->action) {
					case NA_SELECTED:
						ED_area_tag_redraw(sa);
						break;
					case NA_EDITED:
						/* causes node-recalc */
						ED_area_tag_redraw(sa);
						ED_area_tag_refresh(sa);
						break;
				}
			}
			break;
		}
		case NC_GEOM:
		{
			switch (wmn->data) {
				case ND_DATA:
				case ND_SELECT:
					image_scopes_tag_refresh(sa);
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		}
		case NC_OBJECT:
		{
			switch (wmn->data) {
				case ND_TRANSFORM:
				case ND_MODIFIER:
				{
					Object *ob = OBACT;
					if (ob && (ob == wmn->reference) && (ob->mode & OB_MODE_EDIT)) {
						if (sima->lock && (sima->flag & SI_DRAWSHADOW)) {
							ED_area_tag_refresh(sa);
							ED_area_tag_redraw(sa);
						}
					}
					break;
				}
			}

			break;
		}
		case NC_ID:
		{
			if (wmn->action == NA_RENAME) {
				ED_area_tag_redraw(sa);
			}
			break;
		}
		case NC_WM:
			if (wmn->data == ND_UNDO) {
				ED_area_tag_redraw(sa);
				ED_area_tag_refresh(sa);
			}
			break;
	}
}

const char *image_context_dir[] = {"edit_image", "edit_mask", NULL};

static int image_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceImage *sima = CTX_wm_space_image(C);

	if (CTX_data_dir(member)) {
		CTX_data_dir_set(result, image_context_dir);
	}
	else if (CTX_data_equals(member, "edit_image")) {
		CTX_data_id_pointer_set(result, (ID *)ED_space_image(sima));
		return 1;
	}
	else if (CTX_data_equals(member, "edit_mask")) {
		Mask *mask = ED_space_image_get_mask(sima);
		if (mask) {
			CTX_data_id_pointer_set(result, &mask->id);
		}
		return true;
	}
	return 0;
}

/************************** main region ***************************/

/* sets up the fields of the View2D from zoom and offset */
static void image_main_region_set_view2d(SpaceImage *sima, ARegion *ar)
{
	Image *ima = ED_space_image(sima);
	float x1, y1, w, h;
	int width, height, winx, winy;
	
#if 0
	if (image_preview_active(curarea, &width, &height)) {}
	else
#endif
	ED_space_image_get_size(sima, &width, &height);

	w = width;
	h = height;
	
	if (ima)
		h *= ima->aspy / ima->aspx;

	winx = BLI_rcti_size_x(&ar->winrct) + 1;
	winy = BLI_rcti_size_y(&ar->winrct) + 1;
		
	ar->v2d.tot.xmin = 0;
	ar->v2d.tot.ymin = 0;
	ar->v2d.tot.xmax = w;
	ar->v2d.tot.ymax = h;
	
	ar->v2d.mask.xmin = ar->v2d.mask.ymin = 0;
	ar->v2d.mask.xmax = winx;
	ar->v2d.mask.ymax = winy;

	/* which part of the image space do we see? */
	x1 = ar->winrct.xmin + (winx - sima->zoom * w) / 2.0f;
	y1 = ar->winrct.ymin + (winy - sima->zoom * h) / 2.0f;

	x1 -= sima->zoom * sima->xof;
	y1 -= sima->zoom * sima->yof;
	
	/* relative display right */
	ar->v2d.cur.xmin = ((ar->winrct.xmin - (float)x1) / sima->zoom);
	ar->v2d.cur.xmax = ar->v2d.cur.xmin + ((float)winx / sima->zoom);
	
	/* relative display left */
	ar->v2d.cur.ymin = ((ar->winrct.ymin - (float)y1) / sima->zoom);
	ar->v2d.cur.ymax = ar->v2d.cur.ymin + ((float)winy / sima->zoom);
	
	/* normalize 0.0..1.0 */
	ar->v2d.cur.xmin /= w;
	ar->v2d.cur.xmax /= w;
	ar->v2d.cur.ymin /= h;
	ar->v2d.cur.ymax /= h;
}

/* add handlers, stuff you only do once or on area/region changes */
static void image_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	// image space manages own v2d
	// UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);

	/* mask polls mode */
	keymap = WM_keymap_find(wm->defaultconf, "Mask Editing", 0, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	/* image paint polls for mode */
	keymap = WM_keymap_find(wm->defaultconf, "Curve", 0, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	keymap = WM_keymap_find(wm->defaultconf, "Paint Curve", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Image Paint", 0, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	keymap = WM_keymap_find(wm->defaultconf, "UV Editor", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	keymap = WM_keymap_find(wm->defaultconf, "UV Sculpt", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	/* own keymaps */
	keymap = WM_keymap_find(wm->defaultconf, "Image Generic", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	keymap = WM_keymap_find(wm->defaultconf, "Image", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

}

static void image_main_region_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceImage *sima = CTX_wm_space_image(C);
	Object *obact = CTX_data_active_object(C);
	Object *obedit = CTX_data_edit_object(C);
	Mask *mask = NULL;
	bool curve = false;
	Scene *scene = CTX_data_scene(C);
	View2D *v2d = &ar->v2d;
	//View2DScrollers *scrollers;
	float col[3];

	/* XXX not supported yet, disabling for now */
	scene->r.scemode &= ~R_COMP_CROP;
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	image_user_refresh_scene(C, sima);

	/* we set view2d from own zoom and offset each time */
	image_main_region_set_view2d(sima, ar);

	/* we draw image in pixelspace */
	draw_image_main(C, ar);

	/* and uvs in 0.0-1.0 space */
	UI_view2d_view_ortho(v2d);

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

	ED_uvedit_draw_main(sima, ar, scene, obedit, obact);

	/* check for mask (delay draw) */
	if (ED_space_image_show_uvedit(sima, obedit)) {
		/* pass */
	}
	else if (sima->mode == SI_MODE_MASK) {
		mask = ED_space_image_get_mask(sima);
	}
	else if (ED_space_image_paint_curve(C)) {
		curve = true;
	}

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

	if (sima->flag & SI_SHOW_GPENCIL) {
		/* Grease Pencil too (in addition to UV's) */
		draw_image_grease_pencil((bContext *)C, true);
	}

	/* sample line */
	draw_image_sample_line(sima);

	UI_view2d_view_restore(C);

	if (sima->flag & SI_SHOW_GPENCIL) {
		/* draw Grease Pencil - screen space only */
		draw_image_grease_pencil((bContext *)C, false);
	}

	if (mask) {
		Image *image = ED_space_image(sima);
		int width, height, show_viewer;
		float aspx, aspy;

		show_viewer = (image && image->source == IMA_SRC_VIEWER);

		if (show_viewer) {
			/* ED_space_image_get* will acquire image buffer which requires
			 * lock here by the same reason why lock is needed in draw_image_main
			 */
			BLI_lock_thread(LOCK_DRAW_IMAGE);
		}

		ED_space_image_get_size(sima, &width, &height);
		ED_space_image_get_aspect(sima, &aspx, &aspy);

		if (show_viewer)
			BLI_unlock_thread(LOCK_DRAW_IMAGE);

		ED_mask_draw_region(mask, ar,
		                    sima->mask_info.draw_flag,
		                    sima->mask_info.draw_type,
		                    sima->mask_info.overlay_mode,
		                    width, height,
		                    aspx, aspy,
		                    true, false,
		                    NULL, C);

		UI_view2d_view_ortho(v2d);
		ED_image_draw_cursor(ar, sima->cursor);
		UI_view2d_view_restore(C);
	}
	else if (curve) {
		UI_view2d_view_ortho(v2d);
		ED_image_draw_cursor(ar, sima->cursor);
		UI_view2d_view_restore(C);
	}

	draw_image_cache(C, ar);

	/* scrollers? */
#if 0
	scrollers = UI_view2d_scrollers_calc(C, v2d, V2D_UNIT_VALUES, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
#endif
}

static void image_main_region_listener(bScreen *UNUSED(sc), ScrArea *sa, ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_GPENCIL:
			if (ELEM(wmn->action, NA_EDITED, NA_SELECTED))
				ED_region_tag_redraw(ar);
			else if (wmn->data & ND_GPENCIL_EDITMODE)
				ED_region_tag_redraw(ar);
			break;
		case NC_IMAGE:
			if (wmn->action == NA_PAINTING)
				ED_region_tag_redraw(ar);
			break;
		case NC_MATERIAL:
			if (wmn->data == ND_SHADING_LINKS) {
				SpaceImage *sima = sa->spacedata.first;

				if (sima->iuser.scene && (sima->iuser.scene->toolsettings->uv_flag & UV_SHOW_SAME_IMAGE))
					ED_region_tag_redraw(ar);
			}
			break;
	}
}

/* *********************** buttons region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void image_buttons_region_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ar->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
	ED_region_panels_init(wm, ar);
	
	keymap = WM_keymap_find(wm->defaultconf, "Image Generic", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void image_buttons_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, NULL, -1, true);
}

static void image_buttons_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_TEXTURE:
		case NC_MATERIAL:
			/* sending by texture render job and needed to properly update displaying
			 * brush texture icon */
			ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_MODE:
				case ND_RENDER_RESULT:
				case ND_COMPO_RESULT:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_IMAGE:
			if (wmn->action != NA_PAINTING)
				ED_region_tag_redraw(ar);
			break;
		case NC_NODE:
			ED_region_tag_redraw(ar);
			break;
		case NC_GPENCIL:
			if (ELEM(wmn->action, NA_EDITED, NA_SELECTED))
				ED_region_tag_redraw(ar);
			break;
	}
}

/* *********************** scopes region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void image_tools_region_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	ar->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
	ED_region_panels_init(wm, ar);
	
	keymap = WM_keymap_find(wm->defaultconf, "Image Generic", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void image_tools_region_draw(const bContext *C, ARegion *ar)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	void *lock;
	ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock);
	/* XXX performance regression if name of scopes category changes! */
	PanelCategoryStack *category = UI_panel_category_active_find(ar, "Scopes");

	/* only update scopes if scope category is active */
	if (category) {
		if (ibuf) {
			if (!sima->scopes.ok) {
				BKE_histogram_update_sample_line(&sima->sample_line_hist, ibuf, &scene->view_settings, &scene->display_settings);
			}
			if (sima->image->flag & IMA_VIEW_AS_RENDER)
				ED_space_image_scopes_update(C, sima, ibuf, true);
			else
				ED_space_image_scopes_update(C, sima, ibuf, false);
		}
	}
	ED_space_image_release_buffer(sima, ibuf, lock);
	
	ED_region_panels(C, ar, NULL, -1, true);
}

static void image_tools_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_GPENCIL:
			if (wmn->data == ND_DATA || ELEM(wmn->action, NA_EDITED, NA_SELECTED))
				ED_region_tag_redraw(ar);
			break;
		case NC_BRUSH:
			/* NA_SELECTED is used on brush changes */
			if (ELEM(wmn->action, NA_EDITED, NA_SELECTED))
				ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_MODE:
				case ND_RENDER_RESULT:
				case ND_COMPO_RESULT:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_IMAGE:
			if (wmn->action != NA_PAINTING)
				ED_region_tag_redraw(ar);
			break;
		case NC_NODE:
			ED_region_tag_redraw(ar);
			break;
			
	}
}

/************************* header region **************************/

/* add handlers, stuff you only do once or on area/region changes */
static void image_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void image_header_region_draw(const bContext *C, ARegion *ar)
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceImage *sima = sa->spacedata.first;

	image_user_refresh_scene(C, sima);

	ED_region_header(C, ar);
}

static void image_header_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_MODE:
				case ND_TOOLSETTINGS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_GEOM:
			switch (wmn->data) {
				case ND_DATA:
				case ND_SELECT:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
	}
}

static void image_id_remap(ScrArea *UNUSED(sa), SpaceLink *slink, ID *old_id, ID *new_id)
{
	SpaceImage *simg = (SpaceImage *)slink;

	if (!ELEM(GS(old_id->name), ID_IM, ID_GD, ID_MSK)) {
		return;
	}

	if ((ID *)simg->image == old_id) {
		simg->image = (Image *)new_id;
		id_us_ensure_real(new_id);
	}

	if ((ID *)simg->gpd == old_id) {
		simg->gpd = (bGPdata *)new_id;
		id_us_min(old_id);
		id_us_plus(new_id);
	}

	if ((ID *)simg->mask_info.mask == old_id) {
		simg->mask_info.mask = (Mask *)new_id;
		id_us_ensure_real(new_id);
	}
}

/**************************** spacetype *****************************/

/* only called once, from space/spacetypes.c */
void ED_spacetype_image(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype image");
	ARegionType *art;
	
	st->spaceid = SPACE_IMAGE;
	strncpy(st->name, "Image", BKE_ST_MAXNAME);
	
	st->new = image_new;
	st->free = image_free;
	st->init = image_init;
	st->duplicate = image_duplicate;
	st->operatortypes = image_operatortypes;
	st->keymap = image_keymap;
	st->dropboxes = image_dropboxes;
	st->refresh = image_refresh;
	st->listener = image_listener;
	st->context = image_context;
	st->id_remap = image_id_remap;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_GPENCIL;
	art->init = image_main_region_init;
	art->draw = image_main_region_draw;
	art->listener = image_main_region_listener;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: listview/buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex = 220; // XXX
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = image_buttons_region_listener;
	art->init = image_buttons_region_init;
	art->draw = image_buttons_region_draw;
	BLI_addhead(&st->regiontypes, art);

	ED_uvedit_buttons_register(art);
	image_buttons_register(art);

	/* regions: statistics/scope buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_TOOLS;
	art->prefsizex = 220; // XXX
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = image_tools_region_listener;
	art->init = image_tools_region_init;
	art->draw = image_tools_region_draw;
	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	art->listener = image_header_region_listener;
	art->init = image_header_region_init;
	art->draw = image_header_region_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	BKE_spacetype_register(st);
}

