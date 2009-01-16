/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_node.h"
#include "BKE_packedFile.h"
#include "BKE_screen.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_pipeline.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "ED_screen.h"
#include "ED_uvedit.h"

#include "WM_api.h"
#include "WM_types.h"

#include "image_intern.h"

void imagespace_composite_flipbook(SpaceImage *sima, Scene *scene)
{
	ImBuf *ibuf;
	int cfrao= scene->r.cfra;
	int sfra, efra;
	
	if(sima->iuser.frames<2)
		return;
	if(scene->nodetree==NULL)
		return;
	
	sfra= sima->iuser.sfra;
	efra= sima->iuser.sfra + sima->iuser.frames-1;
	scene->nodetree->test_break= NULL; // XXX blender_test_break;
	
	for(scene->r.cfra=sfra; scene->r.cfra<=efra; scene->r.cfra++) {
		
		// XXX set_timecursor(CFRA);
		
		BKE_image_all_free_anim_ibufs(CFRA);
		ntreeCompositTagAnimated(scene->nodetree);
		ntreeCompositExecTree(scene->nodetree, &scene->r, scene->r.cfra!=cfrao);	/* 1 is no previews */
		
		// XXX force_draw(0);
		
		ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);
		/* save memory in flipbooks */
		if(ibuf)
			imb_freerectfloatImBuf(ibuf);
		
		// XXX if(blender_test_break())
		// XXX	break;
	}
	scene->nodetree->test_break= NULL;
	// XXX waitcursor(0);
	
	// XXX play_anim(0);
	
	// XXX allqueue(REDRAWNODE, 1);
	// XXX allqueue(REDRAWIMAGE, 1);
	
	scene->r.cfra= cfrao;
}

/******************** view navigation utilities *********************/

static void sima_zoom_set(SpaceImage *sima, ARegion *ar, float zoom)
{
	float oldzoom= sima->zoom;
	int width, height;

	sima->zoom= zoom;

	if (sima->zoom > 0.1f && sima->zoom < 4.0f)
		return;

	/* check zoom limits */
	get_space_image_size(sima, &width, &height);

	width *= sima->zoom;
	height *= sima->zoom;

	if((width < 4) && (height < 4))
		sima->zoom= oldzoom;
	else if((ar->winrct.xmax - ar->winrct.xmin) <= sima->zoom)
		sima->zoom= oldzoom;
	else if((ar->winrct.ymax - ar->winrct.ymin) <= sima->zoom)
		sima->zoom= oldzoom;
}

static void sima_zoom_set_factor(SpaceImage *sima, ARegion *ar, float zoomfac)
{
	sima_zoom_set(sima, ar, sima->zoom*zoomfac);
}

static int space_image_main_area_poll(bContext *C)
{
	SpaceLink *slink= CTX_wm_space_data(C);
	ARegion *ar= CTX_wm_region(C);

	if(slink && (slink->spacetype == SPACE_IMAGE))
		return (ar && ar->type->regionid == RGN_TYPE_WINDOW);
	
	return 0;
}

/********************** view pan operator *********************/

typedef struct ViewPanData {
	float x, y;
	float xof, yof;
} ViewPanData;

static void view_pan_init(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ViewPanData *vpd;

	op->customdata= vpd= MEM_callocN(sizeof(ViewPanData), "ImageViewPanData");
	WM_cursor_modal(CTX_wm_window(C), BC_NSEW_SCROLLCURSOR);

	vpd->x= event->x;
	vpd->y= event->y;
	vpd->xof= sima->xof;
	vpd->yof= sima->yof;

	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
}

static void view_pan_exit(bContext *C, wmOperator *op, int cancel)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ViewPanData *vpd= op->customdata;

	if(cancel) {
		sima->xof= vpd->xof;
		sima->yof= vpd->yof;
		ED_area_tag_redraw(CTX_wm_area(C));
	}

	WM_cursor_restore(CTX_wm_window(C));
	MEM_freeN(op->customdata);
}

static int view_pan_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	float offset[2];

	RNA_float_get_array(op->ptr, "offset", offset);
	sima->xof += offset[0];
	sima->yof += offset[1];

	ED_area_tag_redraw(CTX_wm_area(C));

	/* XXX notifier? */
#if 0
	if(image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
#endif
	
	return OPERATOR_FINISHED;
}

static int view_pan_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	view_pan_init(C, op, event);
	return OPERATOR_RUNNING_MODAL;
}

static int view_pan_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ViewPanData *vpd= op->customdata;
	float offset[2];

	switch(event->type) {
		case MOUSEMOVE:
			sima->xof= vpd->xof;
			sima->yof= vpd->yof;
			offset[0]= (vpd->x - event->x)/sima->zoom;
			offset[1]= (vpd->y - event->y)/sima->zoom;
			RNA_float_set_array(op->ptr, "offset", offset);
			view_pan_exec(C, op);
			break;
		case MIDDLEMOUSE:
			if(event->val==0) {
				view_pan_exit(C, op, 0);
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int view_pan_cancel(bContext *C, wmOperator *op)
{
	view_pan_exit(C, op, 1);
	return OPERATOR_CANCELLED;
}

void IMAGE_OT_view_pan(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View Pan";
	ot->idname= "IMAGE_OT_view_pan";
	
	/* api callbacks */
	ot->exec= view_pan_exec;
	ot->invoke= view_pan_invoke;
	ot->modal= view_pan_modal;
	ot->cancel= view_pan_cancel;
	ot->poll= space_image_main_area_poll;

	/* properties */
	RNA_def_float_vector(ot->srna, "offset", 2, NULL, -FLT_MAX, FLT_MAX,
		"Offset", "Offset in floating point units, 1.0 is the width and height of the image.", -FLT_MAX, FLT_MAX);
}

/********************** view zoom operator *********************/

typedef struct ViewZoomData {
	float x, y;
	float zoom;
} ViewZoomData;

static void view_zoom_init(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ViewZoomData *vpd;

	op->customdata= vpd= MEM_callocN(sizeof(ViewZoomData), "ImageViewZoomData");
	WM_cursor_modal(CTX_wm_window(C), BC_NSEW_SCROLLCURSOR);

	vpd->x= event->x;
	vpd->y= event->y;
	vpd->zoom= sima->zoom;

	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
}

static void view_zoom_exit(bContext *C, wmOperator *op, int cancel)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ViewZoomData *vpd= op->customdata;

	if(cancel) {
		sima->zoom= vpd->zoom;
		ED_area_tag_redraw(CTX_wm_area(C));
	}

	WM_cursor_restore(CTX_wm_window(C));
	MEM_freeN(op->customdata);
}

static int view_zoom_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ARegion *ar= CTX_wm_region(C);

	sima_zoom_set_factor(sima, ar, RNA_float_get(op->ptr, "factor"));

	ED_area_tag_redraw(CTX_wm_area(C));

	/* XXX notifier? */
#if 0
	if(image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
#endif
	
	return OPERATOR_FINISHED;
}

static int view_zoom_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	view_zoom_init(C, op, event);
	return OPERATOR_RUNNING_MODAL;
}

static int view_zoom_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ARegion *ar= CTX_wm_region(C);
	ViewZoomData *vpd= op->customdata;
	float factor;

	switch(event->type) {
		case MOUSEMOVE:
			factor= 1.0 + (vpd->x-event->x+vpd->y-event->y)/300.0f;
			RNA_float_set(op->ptr, "factor", factor);
			sima_zoom_set(sima, ar, vpd->zoom*factor);
			ED_area_tag_redraw(CTX_wm_area(C));
			break;
		case MIDDLEMOUSE:
			if(event->val==0) {
				view_zoom_exit(C, op, 0);
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int view_zoom_cancel(bContext *C, wmOperator *op)
{
	view_zoom_exit(C, op, 1);
	return OPERATOR_CANCELLED;
}

void IMAGE_OT_view_zoom(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View Zoom";
	ot->idname= "IMAGE_OT_view_zoom";
	
	/* api callbacks */
	ot->exec= view_zoom_exec;
	ot->invoke= view_zoom_invoke;
	ot->modal= view_zoom_modal;
	ot->cancel= view_zoom_cancel;
	ot->poll= space_image_main_area_poll;

	/* properties */
	RNA_def_float(ot->srna, "factor", 0.0f, 0.0f, FLT_MAX,
		"Factor", "Zoom factor, values higher than 1.0 zoom in, lower values zoom out.", -FLT_MAX, FLT_MAX);
}

/********************** view all operator *********************/

/* Updates the fields of the View2D member of the SpaceImage struct.
 * Default behavior is to reset the position of the image and set the zoom to 1
 * If the image will not fit within the window rectangle, the zoom is adjusted */

static int view_all_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima;
	ARegion *ar;
	Scene *scene;
	Object *obedit;
	Image *ima;
	ImBuf *ibuf;
	float aspx, aspy, zoomx, zoomy, w, h;
	int width, height;

	/* retrieve state */
	sima= (SpaceImage*)CTX_wm_space_data(C);
	ar= CTX_wm_region(C);
	scene= (Scene*)CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);

	ima= get_space_image(sima);
	ibuf= get_space_image_buffer(sima);
	get_space_image_size(sima, &width, &height);
	get_space_image_aspect(sima, &aspx, &aspy);

	w= width*aspx;
	h= height*aspy;
	
	/* check if the image will fit in the image with zoom==1 */
	width = ar->winrct.xmax - ar->winrct.xmin + 1;
	height = ar->winrct.ymax - ar->winrct.ymin + 1;

	if((w >= width || h >= height) && (width > 0 && height > 0)) {
		/* find the zoom value that will fit the image in the image space */
		zoomx= width/w;
		zoomy= height/h;
		sima_zoom_set(sima, ar, 1.0f/power_of_2(1/MIN2(zoomx, zoomy)));
	}
	else
		sima_zoom_set(sima, ar, 1.0f);

	sima->xof= sima->yof= 0.0f;

	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View All";
	ot->idname= "IMAGE_OT_view_all";
	
	/* api callbacks */
	ot->exec= view_all_exec;
	ot->poll= space_image_main_area_poll;
}

/********************** view selected operator *********************/

static int view_selected_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima;
	ARegion *ar;
	Scene *scene;
	Object *obedit;
	Image *ima;
	ImBuf *ibuf;
	float size, min[2], max[2], d[2];
	int width, height;

	/* retrieve state */
	sima= (SpaceImage*)CTX_wm_space_data(C);
	ar= CTX_wm_region(C);
	scene= (Scene*)CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);

	ima= get_space_image(sima);
	ibuf= get_space_image_buffer(sima);
	get_space_image_size(sima, &width, &height);

	/* get bounds */
	if(!ED_uvedit_minmax(scene, ima, obedit, min, max))
		return OPERATOR_CANCELLED;

	/* adjust offset and zoom */
	sima->xof= (int)(((min[0] + max[0])*0.5f - 0.5f)*width);
	sima->yof= (int)(((min[1] + max[1])*0.5f - 0.5f)*height);

	d[0]= max[0] - min[0];
	d[1]= max[1] - min[1];
	size= 0.5*MAX2(d[0], d[1])*MAX2(width, height)/256.0f;
	
	if(size<=0.01) size= 0.01;
	sima_zoom_set(sima, ar, 0.7/size);

	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View Center";
	ot->idname= "IMAGE_OT_view_selected";
	
	/* api callbacks */
	ot->exec= view_selected_exec;
	ot->poll= ED_operator_uvedit;
}

/********************** view zoom in/out operator *********************/

static int view_zoom_in_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ARegion *ar= CTX_wm_region(C);

	sima_zoom_set_factor(sima, ar, 1.25f);

	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_zoom_in(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View Zoom In";
	ot->idname= "IMAGE_OT_view_zoom_in";
	
	/* api callbacks */
	ot->exec= view_zoom_in_exec;
	ot->poll= space_image_main_area_poll;
}

static int view_zoom_out_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ARegion *ar= CTX_wm_region(C);

	sima_zoom_set_factor(sima, ar, 0.8f);

	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_zoom_out(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View Zoom Out";
	ot->idname= "IMAGE_OT_view_zoom_out";
	
	/* api callbacks */
	ot->exec= view_zoom_out_exec;
	ot->poll= space_image_main_area_poll;
}

/********************** view zoom ratio operator *********************/

static int view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= (SpaceImage*)CTX_wm_space_data(C);
	ARegion *ar= CTX_wm_region(C);

	sima_zoom_set(sima, ar, RNA_float_get(op->ptr, "ratio"));
	
	/* ensure pixel exact locations for draw */
	sima->xof= (int)sima->xof;
	sima->yof= (int)sima->yof;

	/* XXX notifier? */
#if 0
	if(image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
#endif

	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_zoom_ratio(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View Zoom Ratio";
	ot->idname= "IMAGE_OT_view_zoom_ratio";
	
	/* api callbacks */
	ot->exec= view_zoom_ratio_exec;
	ot->poll= space_image_main_area_poll;
	
	/* properties */
	RNA_def_float(ot->srna, "ratio", 0.0f, 0.0f, FLT_MAX,
		"Ratio", "Zoom ratio, 1.0 is 1:1, higher is zoomed in, lower is zoomed out.", -FLT_MAX, FLT_MAX);
}

/* Image functions */

#if 0
static void load_image_filesel(SpaceImage *sima, Scene *scene, Object *obedit, char *str)	/* called from fileselect */
{
	Image *ima= NULL;

	ima= BKE_add_image_file(str, scene->r.cfra);
	if(ima) {
		BKE_image_signal(ima, &sima->iuser, IMA_SIGNAL_RELOAD);
		set_space_image(sima, scene, obedit, ima);
	}
	// XXX BIF_undo_push("Load image UV");
	// XXX allqueue(REDRAWIMAGE, 0);
}

static void replace_image_filesel(SpaceImage *sima, char *str)		/* called from fileselect */
{
	if (!sima->image)
		return;
	
	BLI_strncpy(sima->image->name, str, sizeof(sima->image->name)-1); /* we cant do much if the str is longer then 240 :/ */
	BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_RELOAD);
	// XXX BIF_undo_push("Replace image UV");
	// XXX allqueue(REDRAWIMAGE, 0);
	// XXX allqueue(REDRAWVIEW3D, 0);
}
#endif

static void save_image_doit(SpaceImage *sima, Scene *scene, char *name)
{
	Image *ima= get_space_image(sima);
	ImBuf *ibuf= get_space_image_buffer(sima);
	int len;
	char str[FILE_MAXDIR+FILE_MAXFILE];

	if (ibuf) {
		BLI_strncpy(str, name, sizeof(str));

		BLI_convertstringcode(str, G.sce);
		BLI_convertstringframe(str, scene->r.cfra);
		
		
		if(scene->r.scemode & R_EXTENSION)  {
			BKE_add_image_extension(scene, str, sima->imtypenr);
			BKE_add_image_extension(scene, name, sima->imtypenr);
		}
		
		if (1) { // XXX saveover(str)) {
			
			/* enforce user setting for RGB or RGBA, but skip BW */
			if(scene->r.planes==32)
				ibuf->depth= 32;
			else if(scene->r.planes==24)
				ibuf->depth= 24;
			
			// XXX waitcursor(1);
			if(sima->imtypenr==R_MULTILAYER) {
				RenderResult *rr= BKE_image_get_renderresult(scene, ima);
				if(rr) {
					RE_WriteRenderResult(rr, str, scene->r.quality);
					
					BLI_strncpy(ima->name, name, sizeof(ima->name));
					BLI_strncpy(ibuf->name, str, sizeof(ibuf->name));
					
					/* should be function? nevertheless, saving only happens here */
					for(ibuf= ima->ibufs.first; ibuf; ibuf= ibuf->next)
						ibuf->userflags &= ~IB_BITMAPDIRTY;
					
				}
				else; // XXX error("Did not write, no Multilayer Image");
			}
			else if (BKE_write_ibuf(scene, ibuf, str, sima->imtypenr, scene->r.subimtype, scene->r.quality)) {
				BLI_strncpy(ima->name, name, sizeof(ima->name));
				BLI_strncpy(ibuf->name, str, sizeof(ibuf->name));
				
				ibuf->userflags &= ~IB_BITMAPDIRTY;
				
				/* change type? */
				if( ELEM(ima->source, IMA_SRC_GENERATED, IMA_SRC_VIEWER)) {
					ima->source= IMA_SRC_FILE;
					ima->type= IMA_TYPE_IMAGE;
				}
				if(ima->type==IMA_TYPE_R_RESULT)
					ima->type= IMA_TYPE_IMAGE;
				
				/* name image as how we saved it */
				len= strlen(str);
				while (len > 0 && str[len - 1] != '/' && str[len - 1] != '\\') len--;
				rename_id(&ima->id, str+len);
			} 
			else {
				; // XXX error("Couldn't write image: %s", str);
			}

			// XXX allqueue(REDRAWHEADERS, 0);
			// XXX allqueue(REDRAWBUTSSHADING, 0);

			// XXX waitcursor(0);
		}
	}
}

void open_image_sima(SpaceImage *sima, short imageselect)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];

	if(sima->image)
		BLI_strncpy(name, sima->image->name, sizeof(name));
	else
		BLI_strncpy(name, U.textudir, sizeof(name));

	if(imageselect)
		; // XXX activate_imageselect(FILE_SPECIAL, "Open Image", name, load_image_filesel);
	else
		; // XXX activate_fileselect(FILE_SPECIAL, "Open Image", name, load_image_filesel);
}

void replace_image_sima(SpaceImage *sima, short imageselect)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];

	if(sima->image)
		BLI_strncpy(name, sima->image->name, sizeof(name));
	else
		BLI_strncpy(name, U.textudir, sizeof(name));
	
	if(imageselect)
		; // XXX activate_imageselect(FILE_SPECIAL, "Replace Image", name, replace_image_filesel);
	else
		; // XXX activate_fileselect(FILE_SPECIAL, "Replace Image", name, replace_image_filesel);
}


static char *filesel_imagetype_string(Image *ima)
{
	char *strp, *str= MEM_callocN(14*32, "menu for filesel");
	
	strp= str;
	str += sprintf(str, "Save Image as: %%t|");
	str += sprintf(str, "Targa %%x%d|", R_TARGA);
	str += sprintf(str, "Targa Raw %%x%d|", R_RAWTGA);
	str += sprintf(str, "PNG %%x%d|", R_PNG);
	str += sprintf(str, "BMP %%x%d|", R_BMP);
	str += sprintf(str, "Jpeg %%x%d|", R_JPEG90);
	str += sprintf(str, "Iris %%x%d|", R_IRIS);
	if(G.have_libtiff)
		str += sprintf(str, "Tiff %%x%d|", R_TIFF);
	str += sprintf(str, "Radiance HDR %%x%d|", R_RADHDR);
	str += sprintf(str, "Cineon %%x%d|", R_CINEON);
	str += sprintf(str, "DPX %%x%d|", R_DPX);
#ifdef WITH_OPENEXR
	str += sprintf(str, "OpenEXR %%x%d|", R_OPENEXR);
	/* saving sequences of multilayer won't work, they copy buffers  */
	if(ima->source==IMA_SRC_SEQUENCE && ima->type==IMA_TYPE_MULTILAYER);
	else str += sprintf(str, "MultiLayer %%x%d|", R_MULTILAYER);
#endif	
	return strp;
}

/* always opens fileselect */
void save_as_image_sima(SpaceImage *sima, Scene *scene)
{
	Image *ima = sima->image;
	ImBuf *ibuf= get_space_image_buffer(sima);
	char name[FILE_MAXDIR+FILE_MAXFILE];

	if (ima) {
		strcpy(name, ima->name);

		if (ibuf) {
			char *strp;
			
			strp= filesel_imagetype_string(ima);
			
			/* cant save multilayer sequence, ima->rr isn't valid for a specific frame */
			if(ima->rr && !(ima->source==IMA_SRC_SEQUENCE && ima->type==IMA_TYPE_MULTILAYER))
				sima->imtypenr= R_MULTILAYER;
			else if(ima->type==IMA_TYPE_R_RESULT)
				sima->imtypenr= scene->r.imtype;
			else sima->imtypenr= BKE_ftype_to_imtype(ibuf->ftype);
			
			// XXX activate_fileselect_menu(FILE_SPECIAL, "Save Image", name, strp, &sima->imtypenr, save_image_doit);
		}
	}
}

/* if exists, saves over without fileselect */
void save_image_sima(SpaceImage *sima, Scene *scene)
{
	Image *ima = get_space_image(sima);
	ImBuf *ibuf= get_space_image_buffer(sima);
	char name[FILE_MAXDIR+FILE_MAXFILE];

	if (ima) {
		strcpy(name, ima->name);

		if (ibuf) {
			if (BLI_exists(ibuf->name)) {
				if(BKE_image_get_renderresult(scene, ima)) 
					sima->imtypenr= R_MULTILAYER;
				else 
					sima->imtypenr= BKE_ftype_to_imtype(ibuf->ftype);
				
				save_image_doit(sima, scene, ibuf->name);
			}
			else
				save_as_image_sima(sima, scene);
		}
	}
}

void save_image_sequence_sima(SpaceImage *sima)
{
	ImBuf *ibuf;
	int tot= 0;
	char di[FILE_MAX], fi[FILE_MAX];
	
	if(sima->image==NULL)
		return;
	if(sima->image->source!=IMA_SRC_SEQUENCE)
		return;
	if(sima->image->type==IMA_TYPE_MULTILAYER) {
		// XXX error("Cannot save Multilayer Sequences");
		return;
	}
	
	/* get total */
	for(ibuf= sima->image->ibufs.first; ibuf; ibuf= ibuf->next) 
		if(ibuf->userflags & IB_BITMAPDIRTY)
			tot++;
	
	if(tot==0) {
		// XXX notice("No Images have been changed");
		return;
	}
	/* get a filename for menu */
	for(ibuf= sima->image->ibufs.first; ibuf; ibuf= ibuf->next) 
		if(ibuf->userflags & IB_BITMAPDIRTY)
			break;
	
	BLI_strncpy(di, ibuf->name, FILE_MAX);
	BLI_splitdirstring(di, fi);
	
	sprintf(fi, "%d Image(s) will be saved in %s", tot, di);
	if(1) { // XXX okee(fi)) {
		
		for(ibuf= sima->image->ibufs.first; ibuf; ibuf= ibuf->next) {
			if(ibuf->userflags & IB_BITMAPDIRTY) {
				char name[FILE_MAX];
				BLI_strncpy(name, ibuf->name, sizeof(name));
				
				BLI_convertstringcode(name, G.sce);

				if(0 == IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat)) {
					// XXX error("Could not write image", name);
					break;
				}
				printf("Saved: %s\n", ibuf->name);
				ibuf->userflags &= ~IB_BITMAPDIRTY;
			}
		}
	}
}

void reload_image_sima(SpaceImage *sima)
{
	if (sima ) {
		BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_RELOAD);
		/* set_space_image(sima, scene, obedit, NULL); - do we really need this? */
	}

	// XXX allqueue(REDRAWIMAGE, 0);
	// XXX allqueue(REDRAWVIEW3D, 0);
	// XXX BIF_preview_changed(ID_TE);
}

void new_image_sima(SpaceImage *sima, Scene *scene, Object *obedit)
{
	static int width= 1024, height= 1024;
	static short uvtestgrid= 0;
	static int floatbuf=0;
	static float color[] = {0, 0, 0, 1};
	char name[22];
	Image *ima;
	
	strcpy(name, "Untitled");

#if 0
	add_numbut(0, TEX, "Name:", 0, 21, name, NULL);
	add_numbut(1, NUM|INT, "Width:", 1, 16384, &width, NULL);
	add_numbut(2, NUM|INT, "Height:", 1, 16384, &height, NULL);
	add_numbut(3, COL, "", 0, 0, &color, NULL);
	add_numbut(4, NUM|FLO, "Alpha:", 0.0, 1.0, &color[3], NULL);
	add_numbut(5, TOG|SHO, "UV Test Grid", 0, 0, &uvtestgrid, NULL);
	add_numbut(6, TOG|INT, "32 bit Float", 0, 0, &floatbuf, NULL);
	if (!do_clever_numbuts("New Image", 7, REDRAW))
 		return;
#endif

	ima = BKE_add_image_size(width, height, name, floatbuf, uvtestgrid, color);
	set_space_image(sima, scene, obedit, ima);
	BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
	// XXX BIF_undo_push("Add image");

	// XXX allqueue(REDRAWIMAGE, 0);
	// XXX allqueue(REDRAWVIEW3D, 0);
}

void pack_image_sima(SpaceImage *sima)
{
	Image *ima = sima->image;

	if (ima) {
		if(ima->source!=IMA_SRC_SEQUENCE && ima->source!=IMA_SRC_MOVIE) {
			if (ima->packedfile) {
				if (G.fileflags & G_AUTOPACK)
					if (1) // XXX okee("Disable AutoPack?"))
						G.fileflags &= ~G_AUTOPACK;
				
				if ((G.fileflags & G_AUTOPACK) == 0) {
					unpackImage(ima, PF_ASK);
					// XXX BIF_undo_push("Unpack image");
				}
			}
			else {
				ImBuf *ibuf= get_space_image_buffer(sima);
				if (ibuf && (ibuf->userflags & IB_BITMAPDIRTY)) {
					if(1) // XXX okee("Can't pack painted image. Use Repack as PNG?"))
						BKE_image_memorypack(ima);
				}
				else {
					ima->packedfile = newPackedFile(ima->name);
					// XXX BIF_undo_push("Pack image");
				}
			}

			// XXX allqueue(REDRAWBUTSSHADING, 0);
			// XXX allqueue(REDRAWHEADERS, 0);
		}
	}
}

/* XXX notifier? */
#if 0
/* goes over all ImageUsers, and sets frame numbers if auto-refresh is set */
void BIF_image_update_frame(void)
{
	Tex *tex;
	
	/* texture users */
	for(tex= G.main->tex.first; tex; tex= tex->id.next) {
		if(tex->type==TEX_IMAGE && tex->ima)
			if(ELEM(tex->ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE))
				if(tex->iuser.flag & IMA_ANIM_ALWAYS)
					BKE_image_user_calc_imanr(&tex->iuser, scene->r.cfra, 0);
		
	}
	/* image window, compo node users */
	if(G.curscreen) {
		ScrArea *sa;
		for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
			if(sa->spacetype==SPACE_VIEW3D) {
				View3D *v3d= sa->spacedata.first;
				if(v3d->bgpic)
					if(v3d->bgpic->iuser.flag & IMA_ANIM_ALWAYS)
						BKE_image_user_calc_imanr(&v3d->bgpic->iuser, scene->r.cfra, 0);
			}
			else if(sa->spacetype==SPACE_IMAGE) {
				SpaceImage *sima= sa->spacedata.first;
				if(sima->iuser.flag & IMA_ANIM_ALWAYS)
					BKE_image_user_calc_imanr(&sima->iuser, scene->r.cfra, 0);
			}
			else if(sa->spacetype==SPACE_NODE) {
				SpaceNode *snode= sa->spacedata.first;
				if((snode->treetype==NTREE_COMPOSIT) && (snode->nodetree)) {
					bNode *node;
					for(node= snode->nodetree->nodes.first; node; node= node->next) {
						if(node->id && node->type==CMP_NODE_IMAGE) {
							Image *ima= (Image *)node->id;
							ImageUser *iuser= node->storage;
							if(ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE))
								if(iuser->flag & IMA_ANIM_ALWAYS)
									BKE_image_user_calc_imanr(iuser, scene->r.cfra, 0);
						}
					}
				}
			}
		}
	}
}
#endif

void image_pixel_aspect(Image *image, float *x, float *y)
{
	*x = *y = 1.0;
	
	if(		(image == NULL) ||
			(image->type == IMA_TYPE_R_RESULT) ||
			(image->type == IMA_TYPE_COMPOSITE) ||
			(image->tpageflag & IMA_TILES) ||
			(image->aspx==0.0 || image->aspy==0.0)
	) {
		return;
	}
	
	/* x is always 1 */
	*y = image->aspy / image->aspx;
}

void image_final_aspect(Image *image, float *x, float *y)
{
	*x = *y = 1.0;
	
	if(		(image == NULL) ||
			(image->type == IMA_TYPE_R_RESULT) ||
			(image->type == IMA_TYPE_COMPOSITE) ||
			(image->tpageflag & IMA_TILES) ||
			(image->aspx==0.0 || image->aspy==0.0)
	) {
		return;
	} else {
		ImBuf *ibuf= BKE_image_get_ibuf(image, NULL);
		if (ibuf && ibuf->x && ibuf->y)  {
			*y = (image->aspy * ibuf->y) / (image->aspx * ibuf->x);
		} else {
			/* x is always 1 */
			*y = image->aspy / image->aspx;
		}
	}
}

void sima_sample_color(SpaceImage *sima)
{
	ImBuf *ibuf= get_space_image_buffer(sima);
	float fx, fy;
	short mval[2], mvalo[2], firsttime=1;
	
	if(ibuf==NULL)
		return;
	
	// XXX calc_image_view(sima, 'f');
	// XXX getmouseco_areawin(mvalo);
	
	while(0) { // XXX get_mbut() & L_MOUSE) {
		
		// XXX getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || firsttime) {
			firsttime= 0;
			// XXX areamouseco_to_ipoco(G.v2d, mval, &fx, &fy);
			
			if(fx>=0.0 && fy>=0.0 && fx<1.0 && fy<1.0) {
				float *fp= NULL, *zpf= NULL;
				float vec[3];
				int *zp= NULL;
				char *cp= NULL;
				
				int x= (int) (fx*ibuf->x);
				int y= (int) (fy*ibuf->y);
				
				if(x>=ibuf->x) x= ibuf->x-1;
				if(y>=ibuf->y) y= ibuf->y-1;
				
				if(ibuf->rect)
					cp= (char *)(ibuf->rect + y*ibuf->x + x);
				if(ibuf->zbuf)
					zp= ibuf->zbuf + y*ibuf->x + x;
				if(ibuf->zbuf_float)
					zpf= ibuf->zbuf_float + y*ibuf->x + x;
				if(ibuf->rect_float)
					fp= (ibuf->rect_float + (ibuf->channels)*(y*ibuf->x + x));
					
				if(fp==NULL) {
					fp= vec;
					vec[0]= (float)cp[0]/255.0f;
					vec[1]= (float)cp[1]/255.0f;
					vec[2]= (float)cp[2]/255.0f;
				}
				
				if(sima->cumap) {
					
					if(ibuf->channels==4) {
						if(0) { // XXX G.qual & LR_CTRLKEY) {
							curvemapping_set_black_white(sima->cumap, NULL, fp);
							curvemapping_do_ibuf(sima->cumap, ibuf);
						}
						else if(0) { // XXX G.qual & LR_SHIFTKEY) {
							curvemapping_set_black_white(sima->cumap, fp, NULL);
							curvemapping_do_ibuf(sima->cumap, ibuf);
						}
					}
				}
				
#if 0
				{
					ScrArea *sa, *cur= curarea;
					
					node_curvemap_sample(fp);	/* sends global to node editor */
					for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
						if(sa->spacetype==SPACE_NODE) {
							areawinset(sa->win);
							scrarea_do_windraw(sa);
						}
					}
					node_curvemap_sample(NULL);		/* clears global in node editor */
					curarea= cur;
				}
				
				areawinset(curarea->win);
				scrarea_do_windraw(curarea);
				myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);
				glLoadIdentity();
				
				sima_show_info(ibuf->channels, x, y, cp, (ibuf->rect_float)?fp:NULL, zp, zpf);
				
				screen_swapbuffers();
#endif
				
			}
		}
		// XXX BIF_wait_for_statechange();
	}
	
	// XXX scrarea_queue_winredraw(curarea);
}

void mouseco_to_curtile(SpaceImage *sima, struct Object *obedit)
{
	float fx, fy;
	short mval[2];
	int show_uvedit;
	
	show_uvedit= get_space_image_show_uvedit(sima, obedit);
	if(!show_uvedit) return;

	if(sima->image && sima->image->tpageflag & IMA_TILES) {
		
		sima->flag |= SI_EDITTILE;
		
		while(0) { // XXX get_mbut()&L_MOUSE) {
			
			// XXX calc_image_view(sima, 'f');
			
			// XXX getmouseco_areawin(mval);
			// XXX areamouseco_to_ipoco(G.v2d, mval, &fx, &fy);

			if(fx>=0.0 && fy>=0.0 && fx<1.0 && fy<1.0) {
			
				fx= (fx)*sima->image->xrep;
				fy= (fy)*sima->image->yrep;
				
				mval[0]= fx;
				mval[1]= fy;
				
				sima->curtile= mval[1]*sima->image->xrep + mval[0];
			}

			// XXX scrarea_do_windraw(curarea);
			// XXX screen_swapbuffers();
		}
		
		sima->flag &= ~SI_EDITTILE;

		// XXX image_set_tile(sima, 2);

		// XXX allqueue(REDRAWVIEW3D, 0);
		// XXX scrarea_queue_winredraw(curarea);
	}
}

/* Could be used for other 2D views also */
void mouseco_to_cursor_sima(void)
{
	// XXX short mval[2];
	// XXX getmouseco_areawin(mval);
	// XXX areamouseco_to_ipoco(G.v2d, mval, &G.v2d->cursor[0], &G.v2d->cursor[1]);
	// XXX scrarea_queue_winredraw(curarea);
}

