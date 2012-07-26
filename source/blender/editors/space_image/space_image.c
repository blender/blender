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

#include "DNA_mesh_types.h"
#include "DNA_mask_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_tessmesh.h"
#include "BKE_sequencer.h"

#include "IMB_imbuf_types.h"

#include "ED_image.h"
#include "ED_mask.h"
#include "ED_mesh.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_uvedit.h"

#include "BIF_gl.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "image_intern.h"

/**************************** common state *****************************/

static void image_scopes_tag_refresh(ScrArea *sa)
{
	SpaceImage *sima = (SpaceImage *)sa->spacedata.first;
	ARegion *ar;

	/* only while histogram is visible */
	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_PREVIEW && ar->flag & RGN_FLAG_HIDDEN)
			return;
	}

	sima->scopes.ok = 0;
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
	arnew->alignment = RGN_ALIGN_LEFT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

ARegion *image_has_scope_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar = BKE_area_find_region_type(sa, RGN_TYPE_PREVIEW);
	if (ar) return ar;

	/* add subdiv level; after buttons */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_UI);

	/* is error! */
	if (ar == NULL) return NULL;
	
	arnew = MEM_callocN(sizeof(ARegion), "scopes for image");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_PREVIEW;
	arnew->alignment = RGN_ALIGN_RIGHT;
	
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
	simage->lock = TRUE;

	simage->iuser.ok = TRUE;
	simage->iuser.fie_ima = 2;
	simage->iuser.frames = 100;
	
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
	ar->alignment = RGN_ALIGN_LEFT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* scopes */
	ar = MEM_callocN(sizeof(ARegion), "buttons for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype = RGN_TYPE_PREVIEW;
	ar->alignment = RGN_ALIGN_RIGHT;
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
	
	if (simage->cumap)
		curvemapping_free(simage->cumap);
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
	if (simagen->cumap)
		simagen->cumap = curvemapping_copy(simagen->cumap);

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
	WM_operatortype_append(IMAGE_OT_view_ndof);

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

	WM_operatortype_append(IMAGE_OT_record_composite);

	WM_operatortype_append(IMAGE_OT_properties);
	WM_operatortype_append(IMAGE_OT_scopes);
}

static void image_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Image Generic", SPACE_IMAGE, 0);
	wmKeyMapItem *kmi;
	int i;
	
	WM_keymap_add_item(keymap, "IMAGE_OT_new", NKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_open", OKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_reload", RKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_save", SKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_save_as", F3KEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_properties", NKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_scopes", TKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "IMAGE_OT_cycle_render_slot", JKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "IMAGE_OT_cycle_render_slot", JKEY, KM_PRESS, KM_ALT, 0)->ptr, "reverse", TRUE);
	
	keymap = WM_keymap_find(keyconf, "Image", SPACE_IMAGE, 0);
	
	WM_keymap_add_item(keymap, "IMAGE_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_pan", MIDDLEMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_pan", MIDDLEMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_pan", MOUSEPAN, 0, 0, 0);

	WM_keymap_add_item(keymap, "IMAGE_OT_view_all", NDOF_BUTTON_FIT, KM_PRESS, 0, 0); // or view selected?
	WM_keymap_add_item(keymap, "IMAGE_OT_view_ndof", NDOF_MOTION, 0, 0, 0);

	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_in", WHEELINMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_out", WHEELOUTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_in", PADPLUSKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_out", PADMINUS, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom", MIDDLEMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom", MOUSEZOOM, 0, 0, 0);

	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD8, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 8.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD4, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 4.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD2, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 2.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD1, KM_PRESS, 0, 0)->ptr, "ratio", 1.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD2, KM_PRESS, 0, 0)->ptr, "ratio", 0.5f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD4, KM_PRESS, 0, 0)->ptr, "ratio", 0.25f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD8, KM_PRESS, 0, 0)->ptr, "ratio", 0.125f);

	WM_keymap_add_item(keymap, "IMAGE_OT_sample", ACTIONMOUSE, KM_PRESS, 0, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "IMAGE_OT_curves_point_set", ACTIONMOUSE, KM_PRESS, KM_CTRL, 0)->ptr, "point", 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "IMAGE_OT_curves_point_set", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "point", 1);

	/* toggle editmode is handy to have while UV unwrapping */
	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_mode_set", TABKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", OB_MODE_EDIT);
	RNA_boolean_set(kmi->ptr, "toggle", TRUE);

	/* fast switch to render slots */
	for (i = 0; i < MAX2(IMA_MAX_RENDER_SLOT, 9); i++) {
		kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_int", ONEKEY + i, KM_PRESS, 0, 0);
		RNA_string_set(kmi->ptr, "data_path", "space_data.image.render_slot");
		RNA_int_set(kmi->ptr, "value", i);
	}
}

/* dropboxes */
static int image_drop_poll(bContext *UNUSED(C), wmDrag *drag, wmEvent *UNUSED(event))
{
	if (drag->type == WM_DRAG_PATH)
		if (ELEM3(drag->icon, 0, ICON_FILE_IMAGE, ICON_FILE_BLANK)) /* rule might not work? */
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


static void image_refresh(const bContext *C, ScrArea *UNUSED(sa))
{
	Scene *scene = CTX_data_scene(C);
	SpaceImage *sima = CTX_wm_space_image(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima;

	ima = ED_space_image(sima);

	BKE_image_user_check_frame_calc(&sima->iuser, scene->r.cfra, 0);
	
	/* check if we have to set the image from the editmesh */
	if (ima && (ima->source == IMA_SRC_VIEWER || sima->pin)) ;
	else if (obedit && obedit->type == OB_MESH) {
		Mesh *me = (Mesh *)obedit->data;
		struct BMEditMesh *em = me->edit_btmesh;
		int sloppy = 1; /* partially selected face is ok */

		if (BKE_scene_use_new_shading_nodes(scene)) {
			/* new shading system, get image from material */
			BMFace *efa = BM_active_face_get(em->bm, sloppy);

			if (efa) {
				Image *node_ima;
				ED_object_get_active_image(obedit, efa->mat_nr + 1, &node_ima, NULL, NULL);

				if (node_ima)
					sima->image = node_ima;
			}
		}
		else {
			/* old shading system, we set texface */
			MTexPoly *tf;
			
			if (em && EDBM_mtexpoly_check(em)) {
				sima->image = NULL;
				
				tf = EDBM_mtexpoly_active_get(em, NULL, TRUE); /* partially selected face is ok */
				
				if (tf) {
					/* don't need to check for pin here, see above */
					sima->image = tf->tpage;
					
					if (sima->flag & SI_EDITTILE) ;
					else sima->curtile = tf->tile;
				}
			}
		}
	}
}

static void image_listener(ScrArea *sa, wmNotifier *wmn)
{
	SpaceImage *sima = (SpaceImage *)sa->spacedata.first;
	
	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_FRAME:
					image_scopes_tag_refresh(sa);
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);					
					break;
				case ND_MODE:
				case ND_RENDER_RESULT:
				case ND_COMPO_RESULT:
					if (ED_space_image_show_render(sima))
						image_scopes_tag_refresh(sa);
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);					
					break;
			}
			break;
		case NC_IMAGE:
			if (wmn->reference == sima->image || !wmn->reference) {
				image_scopes_tag_refresh(sa);
				ED_area_tag_refresh(sa);
				ED_area_tag_redraw(sa);
			}
			break;
		case NC_SPACE:	
			if (wmn->data == ND_SPACE_IMAGE) {
				image_scopes_tag_refresh(sa);
				ED_area_tag_redraw(sa);
			}
			break;
		case NC_MASK:
			switch (wmn->data) {
				case ND_SELECT:
				case ND_DATA:
				case ND_DRAW:
					ED_area_tag_redraw(sa);
					break;
			}
			switch (wmn->action) {
				case NA_SELECTED:
					ED_area_tag_redraw(sa);
					break;
				case NA_EDITED:
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_GEOM:
			switch (wmn->data) {
				case ND_DATA:
				case ND_SELECT:
					image_scopes_tag_refresh(sa);
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
		case NC_OBJECT:
		{
			Object *ob = (Object *)wmn->reference;
			switch (wmn->data) {
				case ND_TRANSFORM:
				case ND_MODIFIER:
					if (ob && (ob->mode & OB_MODE_EDIT) && sima->lock && (sima->flag & SI_DRAWSHADOW)) {
						ED_area_tag_refresh(sa);
						ED_area_tag_redraw(sa);
					}
					break;
			}
		}
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
		return TRUE;
	}
	return 0;
}

/************************** main region ***************************/

/* sets up the fields of the View2D from zoom and offset */
static void image_main_area_set_view2d(SpaceImage *sima, ARegion *ar)
{
	Image *ima = ED_space_image(sima);
	float x1, y1, w, h;
	int width, height, winx, winy;
	
#if 0
	if (image_preview_active(curarea, &width, &height)) ;
	else
#endif
	ED_space_image_get_size(sima, &width, &height);

	w = width;
	h = height;
	
	if (ima)
		h *= ima->aspy / ima->aspx;

	winx = ar->winrct.xmax - ar->winrct.xmin + 1;
	winy = ar->winrct.ymax - ar->winrct.ymin + 1;
		
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
static void image_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	// image space manages own v2d
	// UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);

	/* mask polls mode */
	keymap = WM_keymap_find(wm->defaultconf, "Mask Editing", 0, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	/* image paint polls for mode */
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

static void image_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceImage *sima = CTX_wm_space_image(C);
	Object *obact = CTX_data_active_object(C);
	Object *obedit = CTX_data_edit_object(C);
	Mask *mask = NULL;
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

	/* put scene context variable in iuser */
	sima->iuser.scene = scene;

	/* we set view2d from own zoom and offset each time */
	image_main_area_set_view2d(sima, ar);

	/* we draw image in pixelspace */
	draw_image_main(sima, ar, scene);

	/* and uvs in 0.0-1.0 space */
	UI_view2d_view_ortho(v2d);
	draw_uvedit_main(sima, ar, scene, obedit, obact);

	/* check for mask (delay draw) */
	if (obedit) {
		/* pass */
	}
	else if (sima->mode == SI_MODE_MASK) {
		mask = ED_space_image_get_mask(sima);
		draw_image_cursor(sima, ar);
	}

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

	/* Grease Pencil too (in addition to UV's) */
	draw_image_grease_pencil((bContext *)C, 1); 

	/* sample line */
	draw_image_sample_line(sima);

	UI_view2d_view_restore(C);

	/* draw Grease Pencil - screen space only */
	draw_image_grease_pencil((bContext *)C, 0);

	if (mask) {
		int width, height;
		ED_mask_size(C, &width, &height);
		ED_mask_draw_region(mask, ar,
		                    sima->mask_info.draw_flag, sima->mask_info.draw_type,
		                    width, height,
		                    TRUE, FALSE,
		                    NULL, C);

		draw_image_cursor(sima, ar);
	}

	/* scrollers? */
#if 0
	scrollers = UI_view2d_scrollers_calc(C, v2d, V2D_UNIT_VALUES, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
#endif
}

static void image_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCREEN:
			if (wmn->data == ND_GPENCIL)
				ED_region_tag_redraw(ar);
			break;
	}
}

/* *********************** buttons region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void image_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);
	
	keymap = WM_keymap_find(wm->defaultconf, "Image Generic", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void image_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

static void image_buttons_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCREEN:
			if (wmn->data == ND_GPENCIL)
				ED_region_tag_redraw(ar);
			break;
		case NC_BRUSH:
			if (wmn->action == NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
		case NC_TEXTURE:
		case NC_MATERIAL:
			/* sending by texture render job and needed to properly update displaying
			 * brush texture icon */
			ED_region_tag_redraw(ar);
			break;
	}
}

/* *********************** scopes region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void image_scope_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	ED_region_panels_init(wm, ar);
	
	keymap = WM_keymap_find(wm->defaultconf, "Image Generic", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void image_scope_area_draw(const bContext *C, ARegion *ar)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	void *lock;
	ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock);
	if (ibuf) {
		if (!sima->scopes.ok) {
			BKE_histogram_update_sample_line(&sima->sample_line_hist, ibuf, scene->r.color_mgt_flag & R_COLOR_MANAGEMENT);
		}
		scopes_update(&sima->scopes, ibuf, scene->r.color_mgt_flag & R_COLOR_MANAGEMENT);
	}
	ED_space_image_release_buffer(sima, lock);
	
	ED_region_panels(C, ar, 1, NULL, -1);
}

static void image_scope_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
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
			ED_region_tag_redraw(ar);
			break;
		case NC_NODE:
			ED_region_tag_redraw(ar);
			break;
			
	}
}

/************************* header region **************************/

/* add handlers, stuff you only do once or on area/region changes */
static void image_header_area_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void image_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void image_header_area_listener(ARegion *ar, wmNotifier *wmn)
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
	
	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_GPENCIL;
	art->init = image_main_area_init;
	art->draw = image_main_area_draw;
	art->listener = image_main_area_listener;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: listview/buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex = 220; // XXX
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = image_buttons_area_listener;
	art->init = image_buttons_area_init;
	art->draw = image_buttons_area_draw;
	BLI_addhead(&st->regiontypes, art);

	image_buttons_register(art);
	ED_uvedit_buttons_register(art);
	
	/* regions: statistics/scope buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_PREVIEW;
	art->prefsizex = 220; // XXX
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = image_scope_area_listener;
	art->init = image_scope_area_init;
	art->draw = image_scope_area_draw;
	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	art->listener = image_header_area_listener;
	art->init = image_header_area_init;
	art->draw = image_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	BKE_spacetype_register(st);
}

