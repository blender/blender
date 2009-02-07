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
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

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

/* ******************** default callbacks for image space ***************** */

static SpaceLink *image_new(const bContext *C)
{
	ARegion *ar;
	SpaceImage *simage;
	
	simage= MEM_callocN(sizeof(SpaceImage), "initimage");
	simage->spacetype= SPACE_IMAGE;
	simage->zoom= 1;
	
	simage->iuser.ok= 1;
	simage->iuser.fie_ima= 2;
	simage->iuser.frames= 100;
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for image");
	
	BLI_addtail(&simage->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	/* channel list region XXX */

	
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

	WM_operatortype_append(IMAGE_OT_toolbox);
}

void image_keymap(struct wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "Image", SPACE_IMAGE, 0);
	
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

	WM_keymap_add_item(keymap, "IMAGE_OT_toolbox", SPACEKEY, KM_PRESS, 0, 0);
}

static void image_refresh(const bContext *C, ScrArea *sa)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima;

	ima= get_space_image(sima);

	/* check if we have to set the image from the editmesh */
	if(ima && (ima->source==IMA_SRC_VIEWER || sima->pin));
	else if(obedit && obedit->type == OB_MESH) {
		Mesh *me= (Mesh*)obedit->data;
		EditMesh *em= me->edit_mesh;
		MTFace *tf;
		
		if(EM_texFaceCheck(em)) {
			sima->image= ima= NULL;
			
			tf = EM_get_active_mtface(em, NULL, NULL, 1); /* partially selected face is ok */
			
			if(tf && (tf->mode & TF_TEX)) {
				/* don't need to check for pin here, see above */
				sima->image= ima= tf->tpage;
				
				if(sima->flag & SI_EDITTILE);
				else sima->curtile= tf->tile;
				
				if(ima) {
					if(tf->mode & TF_TILES)
						ima->tpageflag |= IMA_TILES;
					else
						ima->tpageflag &= ~IMA_TILES;
				}
			}
		}
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
	}
}

static int image_context(const bContext *C, bContextDataMember member, bContextDataResult *result)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);

	if(member == CTX_DATA_EDIT_IMAGE) {
		CTX_data_pointer_set(result, get_space_image(sima));
		return 1;
	}
	else if(member == CTX_DATA_EDIT_IMAGE_BUFFER) {
		CTX_data_pointer_set(result, get_space_image_buffer(sima));
		return 1;
	}

	return 0;
}

/************************** main region ***************************/

/* sets up the fields of the View2D from zoom and offset */
static void image_main_area_set_view2d(SpaceImage *sima, ARegion *ar)
{
	Image *ima= get_space_image(sima);
	float x1, y1, w, h;
	int width, height, winx, winy;
	
#if 0
	if(image_preview_active(curarea, &xim, &yim));
	else if(sima->image) {
		ImBuf *ibuf= imagewindow_get_ibuf(sima);
		float xuser_asp, yuser_asp;
		
		image_pixel_aspect(sima->image, &xuser_asp, &yuser_asp);
		if(ibuf) {
			xim= ibuf->x * xuser_asp;
			yim= ibuf->y * yuser_asp;
		}
		else if( sima->image->type==IMA_TYPE_R_RESULT ) {
			/* not very important, just nice */
			xim= (G.scene->r.xsch*G.scene->r.size)/100;
			yim= (G.scene->r.ysch*G.scene->r.size)/100;
		}
	}
#endif

	get_space_image_size(sima, &width, &height);

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
	/* same calculation as in lrectwrite: area left and down*/
	x1= ar->winrct.xmin+(winx-sima->zoom*w)/2;
	y1= ar->winrct.ymin+(winy-sima->zoom*h)/2;

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
	ListBase *keymap;
	
	// image space manages own v2d
	// UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "Image", SPACE_IMAGE, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void image_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene= CTX_data_scene(C);
	View2D *v2d= &ar->v2d;
	//View2DScrollers *scrollers;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* we set view2d from own zoom and offset each time */
	image_main_area_set_view2d(sima, ar);
		
	/* we draw image in pixelspace */
	draw_image_main(sima, ar, scene);

	/* and uvs in 0.0-1.0 space */
	UI_view2d_view_ortho(C, v2d);
	draw_uvedit_main(sima, ar, scene, obedit);
	UI_view2d_view_restore(C);
	
	/* scrollers? */
	/*scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_UNIT_VALUES, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);*/
}

static void image_modal_keymaps(wmWindowManager *wm, ARegion *ar, int stype)
{
	ListBase *keymap;
	
	keymap= WM_keymap_listbase(wm, "UVEdit", 0, 0);
	if(stype==NS_EDITMODE_MESH)
		WM_event_add_keymap_handler(&ar->handlers, keymap);
	else
		WM_event_remove_keymap_handler(&ar->handlers, keymap);
}

static void image_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCENE:
			switch(wmn->data) {
				case ND_MODE:
					image_modal_keymaps(wmn->wm, ar, wmn->subtype);
					break;
			}
			break;
		case NC_OBJECT:
			switch(wmn->data) {
				case ND_GEOM_SELECT:
				case ND_GEOM_DATA:
					ED_region_tag_redraw(ar);
					break;
			}
	}
}

/************************* header region **************************/

/* add handlers, stuff you only do once or on area/region changes */
static void image_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void image_header_area_draw(const bContext *C, ARegion *ar)
{
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
}

/**************************** spacetype *****************************/

/* only called once, from space/spacetypes.c */
void ED_spacetype_image(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype image");
	ARegionType *art;
	
	st->spaceid= SPACE_IMAGE;
	
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
	art->keymapflag= ED_KEYMAP_FRAMES;
	art->init= image_main_area_init;
	art->draw= image_main_area_draw;
	art->listener= image_main_area_listener;
	art->keymapflag= 0;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype image region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;
	
	art->init= image_header_area_init;
	art->draw= image_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

/**************************** common state *****************************/

Image *get_space_image(SpaceImage *sima)
{
	return sima->image;
}

/* called to assign images to UV faces */
void set_space_image(SpaceImage *sima, Scene *scene, Object *obedit, Image *ima)
{
	ED_uvedit_assign_image(scene, obedit, ima, sima->image);

	/* change the space ima after because uvedit_face_visible uses the space ima
	 * to check if the face is displayed in UV-localview */
	sima->image= ima;

	if(ima == NULL || ima->type==IMA_TYPE_R_RESULT || ima->type==IMA_TYPE_COMPOSITE)
		sima->flag &= ~SI_DRAWTOOL;

	if(sima->image)
		BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
}

ImBuf *get_space_image_buffer(SpaceImage *sima)
{
	ImBuf *ibuf;

	if(sima->image) {
#if 0
		if(sima->image->type==IMA_TYPE_R_RESULT && BIF_show_render_spare())
			return BIF_render_spare_imbuf();
		else
#endif
			ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);

		if(ibuf && (ibuf->rect || ibuf->rect_float))
			return ibuf;
	}

	return NULL;
}

void get_space_image_size(SpaceImage *sima, int *width, int *height)
{
	ImBuf *ibuf;

	ibuf= get_space_image_buffer(sima);

	if(ibuf && ibuf->x > 0 && ibuf->y > 0) {
		*width= ibuf->x;
		*height= ibuf->y;
	}
	/* I know a bit weak... but preview uses not actual image size */
	// XXX else if(image_preview_active(sima, width, height));
	else {
		*width= 256;
		*height= 256;
	}
}

void get_space_image_aspect(SpaceImage *sima, float *aspx, float *aspy)
{
	Image *ima;

	ima= get_space_image(sima);

	*aspx= *aspy= 1.0;

	if((ima == NULL) || (ima->type == IMA_TYPE_R_RESULT) || (ima->type == IMA_TYPE_COMPOSITE) ||
	   (ima->tpageflag & IMA_TILES) || (ima->aspx==0.0 || ima->aspy==0.0))
		return;

	/* x is always 1 */
	*aspy = ima->aspy/ima->aspx;
}

void get_space_image_zoom(SpaceImage *sima, ARegion *ar, float *zoomx, float *zoomy)
{
	int width, height;

	get_space_image_size(sima, &width, &height);

	*zoomx= (float)(ar->winrct.xmax - ar->winrct.xmin)/(float)((ar->v2d.cur.xmax - ar->v2d.cur.xmin)*width);
	*zoomy= (float)(ar->winrct.ymax - ar->winrct.ymin)/(float)((ar->v2d.cur.ymax - ar->v2d.cur.ymin)*height);
}

void get_space_image_uv_aspect(SpaceImage *sima, float *aspx, float *aspy)
{
	int w, h;

	get_space_image_aspect(sima, aspx, aspy);
	get_space_image_size(sima, &w, &h);

	*aspx *= (float)w;
	*aspy *= (float)h;
}

int get_space_image_show_render(SpaceImage *sima)
{
	return (sima->image && ELEM(sima->image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE));
}

int get_space_image_show_paint(SpaceImage *sima)
{
	if(get_space_image_show_render(sima))
		return 0;

	return (sima->flag & SI_DRAWTOOL);
}

int get_space_image_show_uvedit(SpaceImage *sima, Object *obedit)
{
	if(get_space_image_show_render(sima))
		return 0;
	if(get_space_image_show_paint(sima))
		return 0;

	if(obedit && obedit->type == OB_MESH)
		return EM_texFaceCheck(((Mesh*)obedit->data)->edit_mesh);

	return 0;
}

int get_space_image_show_uvshadow(SpaceImage *sima, Object *obedit)
{
	if(get_space_image_show_render(sima))
		return 0;

	if(get_space_image_show_paint(sima))
		if(obedit && obedit->type == OB_MESH)
			return EM_texFaceCheck(((Mesh*)obedit->data)->edit_mesh);

	return 0;
}

/* Exported Functions */

Image *ED_space_image(SpaceImage *sima)
{
	return get_space_image(sima);
}

void ED_space_image_size(SpaceImage *sima, int *width, int *height)
{
	get_space_image_size(sima, width, height);
}

void ED_space_image_uv_aspect(SpaceImage *sima, float *aspx, float *aspy)
{
	get_space_image_uv_aspect(sima, aspx, aspy);
}


