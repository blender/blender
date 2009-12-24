/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_gpencil.h"
#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_uvedit.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "image_intern.h"

/* ******************** manage regions ********************* */

ARegion *image_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;
	
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_UI)
			return ar;
	
	/* add subdiv level; after header */
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_HEADER)
			break;
	
	/* is error! */
	if(ar==NULL) return NULL;
	
	arnew= MEM_callocN(sizeof(ARegion), "buttons for image");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype= RGN_TYPE_UI;
	arnew->alignment= RGN_ALIGN_LEFT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

/* ******************** default callbacks for image space ***************** */

static SpaceLink *image_new(const bContext *C)
{
	ARegion *ar;
	SpaceImage *simage;
	
	simage= MEM_callocN(sizeof(SpaceImage), "initimage");
	simage->spacetype= SPACE_IMAGE;
	simage->zoom= 1;
	simage->lock= 1;
	
	simage->iuser.ok= 1;
	simage->iuser.fie_ima= 2;
	simage->iuser.frames= 100;

	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* buttons/list view */
	ar= MEM_callocN(sizeof(ARegion), "buttons for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype= RGN_TYPE_UI;
	ar->alignment= RGN_ALIGN_LEFT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	return (SpaceLink *)simage;
}

/* not spacelink itself */
static void image_free(SpaceLink *sl)
{	
	SpaceImage *simage= (SpaceImage*) sl;
	
	if(simage->cumap)
		curvemapping_free(simage->cumap);
//	if(simage->gpd)
// XXX		free_gpencil_data(simage->gpd);
	
}


/* spacetype; init callback */
static void image_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *image_duplicate(SpaceLink *sl)
{
	SpaceImage *simagen= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	if(simagen->cumap)
		simagen->cumap= curvemapping_copy(simagen->cumap);
	
	return (SpaceLink *)simagen;
}

void image_operatortypes(void)
{
	WM_operatortype_append(IMAGE_OT_view_all);
	WM_operatortype_append(IMAGE_OT_view_pan);
	WM_operatortype_append(IMAGE_OT_view_selected);
	WM_operatortype_append(IMAGE_OT_view_zoom);
	WM_operatortype_append(IMAGE_OT_view_zoom_in);
	WM_operatortype_append(IMAGE_OT_view_zoom_out);
	WM_operatortype_append(IMAGE_OT_view_zoom_ratio);

	WM_operatortype_append(IMAGE_OT_new);
	WM_operatortype_append(IMAGE_OT_open);
	WM_operatortype_append(IMAGE_OT_replace);
	WM_operatortype_append(IMAGE_OT_reload);
	WM_operatortype_append(IMAGE_OT_save);
	WM_operatortype_append(IMAGE_OT_save_as);
	WM_operatortype_append(IMAGE_OT_save_sequence);
	WM_operatortype_append(IMAGE_OT_pack);
	WM_operatortype_append(IMAGE_OT_unpack);

	WM_operatortype_append(IMAGE_OT_sample);
	WM_operatortype_append(IMAGE_OT_curves_point_set);

	WM_operatortype_append(IMAGE_OT_record_composite);

	WM_operatortype_append(IMAGE_OT_toolbox);
	WM_operatortype_append(IMAGE_OT_properties);
}

void image_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap= WM_keymap_find(keyconf, "Image Generic", SPACE_IMAGE, 0);
	
	WM_keymap_add_item(keymap, "IMAGE_OT_new", NKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_open", OKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_reload", RKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_save", SKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_save_as", F3KEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_properties", NKEY, KM_PRESS, 0, 0);
	
	keymap= WM_keymap_find(keyconf, "Image", SPACE_IMAGE, 0);
	
	WM_keymap_add_item(keymap, "IMAGE_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_pan", MIDDLEMOUSE, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_in", WHEELINMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_out", WHEELOUTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_in", PADPLUSKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_out", PADMINUS, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom", MIDDLEMOUSE, KM_PRESS, KM_CTRL, 0);

	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD8, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 8.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD4, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 4.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD2, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 2.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD1, KM_PRESS, 0, 0)->ptr, "ratio", 1.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD2, KM_PRESS, 0, 0)->ptr, "ratio", 0.5f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD4, KM_PRESS, 0, 0)->ptr, "ratio", 0.25f);
	RNA_float_set(WM_keymap_add_item(keymap, "IMAGE_OT_view_zoom_ratio", PAD8, KM_PRESS, 0, 0)->ptr, "ratio", 0.125f);

	WM_keymap_add_item(keymap, "PAINT_OT_grab_clone", RIGHTMOUSE, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "IMAGE_OT_sample", ACTIONMOUSE, KM_PRESS, 0, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "IMAGE_OT_curves_point_set", ACTIONMOUSE, KM_PRESS, KM_CTRL, 0)->ptr, "point", 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "IMAGE_OT_curves_point_set", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "point", 1);

	WM_keymap_add_item(keymap, "IMAGE_OT_toolbox", SPACEKEY, KM_PRESS, 0, 0);
}

static void image_refresh(const bContext *C, ScrArea *sa)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima;

	ima= ED_space_image(sima);

	/* check if we have to set the image from the editmesh */
	if(ima && (ima->source==IMA_SRC_VIEWER || sima->pin));
	else if(obedit && obedit->type == OB_MESH) {
		Mesh *me= (Mesh*)obedit->data;
		EditMesh *em= BKE_mesh_get_editmesh(me);
		MTFace *tf;
		
		if(em && EM_texFaceCheck(em)) {
			sima->image= ima= NULL;
			
			tf = EM_get_active_mtface(em, NULL, NULL, 1); /* partially selected face is ok */
			
			if(tf && (tf->mode & TF_TEX)) {
				/* don't need to check for pin here, see above */
				sima->image= ima= tf->tpage;
				
				if(sima->flag & SI_EDITTILE);
				else sima->curtile= tf->tile;
			}
		}

		BKE_mesh_end_editmesh(obedit->data, em);
	}
}

static void image_listener(ScrArea *sa, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCENE:
			switch(wmn->data) {
				case ND_MODE:
				case ND_RENDER_RESULT:
				case ND_COMPO_RESULT:
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_IMAGE:	
			ED_area_tag_redraw(sa);
			break;
		case NC_SPACE:	
			if(wmn->data == ND_SPACE_IMAGE)
				ED_area_tag_redraw(sa);
			break;
		case NC_GEOM:
			switch(wmn->data) {
				case ND_DATA:
				case ND_SELECT:
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
	}
}

static int image_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceImage *sima= CTX_wm_space_image(C);

	if(CTX_data_dir(member)) {
		static const char *dir[] = {"edit_image", NULL};
		CTX_data_dir_set(result, dir);
	}
	else if(CTX_data_equals(member, "edit_image")) {
		CTX_data_id_pointer_set(result, (ID*)ED_space_image(sima));
		return 1;
	}

	return 0;
}

/************************** main region ***************************/

/* sets up the fields of the View2D from zoom and offset */
static void image_main_area_set_view2d(SpaceImage *sima, ARegion *ar)
{
	Image *ima= ED_space_image(sima);
	float x1, y1, w, h;
	int width, height, winx, winy;
	
#if 0
	if(image_preview_active(curarea, &width, &height));
	else
#endif
	ED_space_image_size(sima, &width, &height);

	w= width;
	h= height;
	
	if(ima)
		h *= ima->aspy/ima->aspx;

	winx= ar->winrct.xmax - ar->winrct.xmin + 1;
	winy= ar->winrct.ymax - ar->winrct.ymin + 1;
		
	ar->v2d.tot.xmin= 0;
	ar->v2d.tot.ymin= 0;
	ar->v2d.tot.xmax= w;
	ar->v2d.tot.ymax= h;
	
	ar->v2d.mask.xmin= ar->v2d.mask.ymin= 0;
	ar->v2d.mask.xmax= winx;
	ar->v2d.mask.ymax= winy;

	/* which part of the image space do we see? */
	x1= ar->winrct.xmin+(winx-sima->zoom*w)/2.0f;
	y1= ar->winrct.ymin+(winy-sima->zoom*h)/2.0f;

	x1-= sima->zoom*sima->xof;
	y1-= sima->zoom*sima->yof;
	
	/* relative display right */
	ar->v2d.cur.xmin= ((ar->winrct.xmin - (float)x1)/sima->zoom);
	ar->v2d.cur.xmax= ar->v2d.cur.xmin + ((float)winx/sima->zoom);
	
	/* relative display left */
	ar->v2d.cur.ymin= ((ar->winrct.ymin-(float)y1)/sima->zoom);
	ar->v2d.cur.ymax= ar->v2d.cur.ymin + ((float)winy/sima->zoom);
	
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

	/* image paint polls for mode */
	keymap= WM_keymap_find(wm->defaultconf, "Image Paint", 0, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	keymap= WM_keymap_find(wm->defaultconf, "UV Editor", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	/* own keymaps */
	keymap= WM_keymap_find(wm->defaultconf, "Image Generic", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	keymap= WM_keymap_find(wm->defaultconf, "Image", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void image_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceImage *sima= CTX_wm_space_image(C);
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene= CTX_data_scene(C);
	View2D *v2d= &ar->v2d;
	//View2DScrollers *scrollers;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	/* put scene context variable in iuser */
	sima->iuser.scene= scene;

	/* we set view2d from own zoom and offset each time */
	image_main_area_set_view2d(sima, ar);
	
	/* we draw image in pixelspace */
	draw_image_main(sima, ar, scene);

	/* and uvs in 0.0-1.0 space */
	UI_view2d_view_ortho(C, v2d);
	draw_uvedit_main(sima, ar, scene, obedit);

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);
		
	/* Grease Pencil too (in addition to UV's) */
	draw_image_grease_pencil((bContext *)C, 1); 

	UI_view2d_view_restore(C);

	/* draw Grease Pencil - screen space only */
	draw_image_grease_pencil((bContext *)C, 0);

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_PIXEL);
	
	/* scrollers? */
	/*scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_UNIT_VALUES, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);*/
}

static void image_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		/* nothing yet */
	}
}

/* *********************** buttons region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void image_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);
	
	keymap= WM_keymap_find(wm->defaultconf, "Image Generic", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void image_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

static void image_buttons_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_BRUSH:
			if(wmn->action==NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
	}
}

/************************* header region **************************/

/* add handlers, stuff you only do once or on area/region changes */
static void image_header_area_init(wmWindowManager *wm, ARegion *ar)
{
#if 0
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
#else
	ED_region_header_init(ar);
#endif
}

static void image_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
#if 0
	float col[3];
	
	/* clear */
	if(ED_screen_area_active(C))
		UI_GetThemeColor3fv(TH_HEADER, col);
	else
		UI_GetThemeColor3fv(TH_HEADERDESEL, col);
	
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(C, &ar->v2d);
	
	image_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
#endif
}

/**************************** spacetype *****************************/

/* only called once, from space/spacetypes.c */
void ED_spacetype_image(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype image");
	ARegionType *art;
	
	st->spaceid= SPACE_IMAGE;
	strncpy(st->name, "Image", BKE_ST_MAXNAME);
	
	st->new= image_new;
	st->free= image_free;
	st->init= image_init;
	st->duplicate= image_duplicate;
	st->operatortypes= image_operatortypes;
	st->keymap= image_keymap;
	st->refresh= image_refresh;
	st->listener= image_listener;
	st->context= image_context;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag= ED_KEYMAP_FRAMES|ED_KEYMAP_GPENCIL;
	art->init= image_main_area_init;
	art->draw= image_main_area_draw;
	art->listener= image_main_area_listener;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: listview/buttons */
	art= MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_UI;
	art->minsizex= 220; // XXX
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_FRAMES;
	art->listener= image_buttons_area_listener;
	art->init= image_buttons_area_init;
	art->draw= image_buttons_area_draw;
	BLI_addhead(&st->regiontypes, art);

	image_buttons_register(art);

	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES|ED_KEYMAP_HEADER;
	art->init= image_header_area_init;
	art->draw= image_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	BKE_spacetype_register(st);
}

/**************************** common state *****************************/

/* note; image_panel_properties() uses pointer to sima->image directly */
Image *ED_space_image(SpaceImage *sima)
{
	return sima->image;
}

/* called to assign images to UV faces */
void ED_space_image_set(bContext *C, SpaceImage *sima, Scene *scene, Object *obedit, Image *ima)
{
	ED_uvedit_assign_image(scene, obedit, ima, sima->image);

	/* change the space ima after because uvedit_face_visible uses the space ima
	 * to check if the face is displayed in UV-localview */
	sima->image= ima;

	if(ima == NULL || ima->type==IMA_TYPE_R_RESULT || ima->type==IMA_TYPE_COMPOSITE)
		sima->flag &= ~SI_DRAWTOOL;

	if(sima->image)
		BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);

	if(sima->image && sima->image->id.us==0)
		sima->image->id.us= 1;

	if(C) {
		if(obedit)
			WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

		ED_area_tag_redraw(CTX_wm_area(C));
	}
}

ImBuf *ED_space_image_acquire_buffer(SpaceImage *sima, void **lock_r)
{
	ImBuf *ibuf;

	if(sima && sima->image) {
#if 0
		if(sima->image->type==IMA_TYPE_R_RESULT && BIF_show_render_spare())
			return BIF_render_spare_imbuf();
		else
#endif
			ibuf= BKE_image_acquire_ibuf(sima->image, &sima->iuser, lock_r);

		if(ibuf && (ibuf->rect || ibuf->rect_float))
			return ibuf;
	}

	return NULL;
}

void ED_space_image_release_buffer(SpaceImage *sima, void *lock)
{
	if(sima && sima->image)
		BKE_image_release_ibuf(sima->image, lock);
}

int ED_space_image_has_buffer(SpaceImage *sima)
{
	ImBuf *ibuf;
	void *lock;
	int has_buffer;

	ibuf= ED_space_image_acquire_buffer(sima, &lock);
	has_buffer= (ibuf != NULL);
	ED_space_image_release_buffer(sima, lock);

	return has_buffer;
}

void ED_image_size(Image *ima, int *width, int *height)
{
	ImBuf *ibuf= NULL;
	void *lock;

	if(ima)
		ibuf= BKE_image_acquire_ibuf(ima, NULL, &lock);

	if(ibuf && ibuf->x > 0 && ibuf->y > 0) {
		*width= ibuf->x;
		*height= ibuf->y;
	}
	else {
		*width= 256;
		*height= 256;
	}

	if(ima)
		BKE_image_release_ibuf(ima, lock);
}

void ED_space_image_size(SpaceImage *sima, int *width, int *height)
{
	Scene *scene= sima->iuser.scene;
	ImBuf *ibuf;
	void *lock;

	ibuf= ED_space_image_acquire_buffer(sima, &lock);

	if(ibuf && ibuf->x > 0 && ibuf->y > 0) {
		*width= ibuf->x;
		*height= ibuf->y;
	}
	else if(sima->image && sima->image->type==IMA_TYPE_R_RESULT && scene) {
		/* not very important, just nice */
		*width= (scene->r.xsch*scene->r.size)/100;
		*height= (scene->r.ysch*scene->r.size)/100;
	}
	/* I know a bit weak... but preview uses not actual image size */
	// XXX else if(image_preview_active(sima, width, height));
	else {
		*width= 256;
		*height= 256;
	}

	ED_space_image_release_buffer(sima, lock);
}

void ED_image_aspect(Image *ima, float *aspx, float *aspy)
{
	*aspx= *aspy= 1.0;

	if((ima == NULL) || (ima->type == IMA_TYPE_R_RESULT) || (ima->type == IMA_TYPE_COMPOSITE) ||
	   (ima->aspx==0.0 || ima->aspy==0.0))
		return;

	/* x is always 1 */
	*aspy = ima->aspy/ima->aspx;
}

void ED_space_image_aspect(SpaceImage *sima, float *aspx, float *aspy)
{
	ED_image_aspect(ED_space_image(sima), aspx, aspy);
}

void ED_space_image_zoom(SpaceImage *sima, ARegion *ar, float *zoomx, float *zoomy)
{
	int width, height;

	ED_space_image_size(sima, &width, &height);

	*zoomx= (float)(ar->winrct.xmax - ar->winrct.xmin)/(float)((ar->v2d.cur.xmax - ar->v2d.cur.xmin)*width);
	*zoomy= (float)(ar->winrct.ymax - ar->winrct.ymin)/(float)((ar->v2d.cur.ymax - ar->v2d.cur.ymin)*height);
}

void ED_space_image_uv_aspect(SpaceImage *sima, float *aspx, float *aspy)
{
	int w, h;

	ED_space_image_aspect(sima, aspx, aspy);
	ED_space_image_size(sima, &w, &h);

	*aspx *= (float)w/256.0f;
	*aspy *= (float)h/256.0f;
}

void ED_image_uv_aspect(Image *ima, float *aspx, float *aspy)
{
	int w, h;

	ED_image_aspect(ima, aspx, aspy);
	ED_image_size(ima, &w, &h);

	*aspx *= (float)w;
	*aspy *= (float)h;
}

int ED_space_image_show_render(SpaceImage *sima)
{
	return (sima->image && ELEM(sima->image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE));
}

int ED_space_image_show_paint(SpaceImage *sima)
{
	if(ED_space_image_show_render(sima))
		return 0;

	return (sima->flag & SI_DRAWTOOL);
}

int ED_space_image_show_uvedit(SpaceImage *sima, Object *obedit)
{
	if(ED_space_image_show_render(sima))
		return 0;
	if(ED_space_image_show_paint(sima))
		return 0;

	if(obedit && obedit->type == OB_MESH) {
		EditMesh *em = BKE_mesh_get_editmesh(obedit->data);
		int ret;
	
		ret = EM_texFaceCheck(em);

		BKE_mesh_end_editmesh(obedit->data, em);
		return ret;
	}

	return 0;
}

int ED_space_image_show_uvshadow(SpaceImage *sima, Object *obedit)
{
	if(ED_space_image_show_render(sima))
		return 0;

	if(ED_space_image_show_paint(sima))
		if(obedit && obedit->type == OB_MESH) {
			EditMesh *em = BKE_mesh_get_editmesh(obedit->data);
			int ret;

			ret = EM_texFaceCheck(em);

			BKE_mesh_end_editmesh(obedit->data, em);
			return ret;
		}

	return 0;
}

