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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_image/image_buttons.c
 *  \ingroup spimage
 */



#include <string.h>
#include <stdio.h>

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_image.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_screen.h"

#include "RE_pipeline.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_gpencil.h"
#include "ED_image.h"
#include "ED_screen.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "image_intern.h"

#define B_REDR                1
#define B_IMAGECHANGED        2
#define B_NOP                 0
#define B_TWINANIM            5
#define B_SIMAGETILE          6
#define B_IDNAME             10
#define B_FACESEL_PAINT_TEST 11
#define B_SIMA_RECORD        12
#define B_SIMA_PLAY          13

#define B_SIMANOTHING        16
#define B_SIMABRUSHCHANGE    17
#define B_SIMABRUSHBROWSE    18
#define B_SIMABRUSHLOCAL     19
#define B_SIMABRUSHDELETE    20
#define B_KEEPDATA           21
#define B_SIMABTEXBROWSE     22
#define B_SIMABTEXDELETE     23
#define B_VPCOLSLI           24
#define B_SIMACLONEBROWSE    25
#define B_SIMACLONEDELETE    26

/* proto */

static void image_info(Scene *scene, ImageUser *iuser, Image *ima, ImBuf *ibuf, char *str)
{
	int ofs = 0;

	str[0] = 0;
	
	if (ima == NULL) return;

	if (ibuf == NULL) {
		ofs += sprintf(str, "Can't Load Image");
	}
	else {
		if (ima->source == IMA_SRC_MOVIE) {
			ofs += sprintf(str, "Movie");
			if (ima->anim)
				ofs += sprintf(str + ofs, "%d frs", IMB_anim_get_duration(ima->anim, IMB_TC_RECORD_RUN));
		}
		else
			ofs += sprintf(str, "Image");

		ofs += sprintf(str + ofs, ": size %d x %d,", ibuf->x, ibuf->y);

		if (ibuf->rect_float) {
			if (ibuf->channels != 4) {
				ofs += sprintf(str + ofs, "%d float channel(s)", ibuf->channels);
			}
			else if (ibuf->planes == R_IMF_PLANES_RGBA)
				ofs += sprintf(str + ofs, " RGBA float");
			else
				ofs += sprintf(str + ofs, " RGB float");
		}
		else {
			if (ibuf->planes == R_IMF_PLANES_RGBA)
				ofs += sprintf(str + ofs, " RGBA byte");
			else
				ofs += sprintf(str + ofs, " RGB byte");
		}
		if (ibuf->zbuf || ibuf->zbuf_float)
			ofs += sprintf(str + ofs, " + Z");

		if (ima->source == IMA_SRC_SEQUENCE) {
			char *file = BLI_last_slash(ibuf->name);
			if (file == NULL) file = ibuf->name;
			else file++;
			ofs += sprintf(str + ofs, ", %s", file);
		}
	}

	/* the frame number, even if we cant */
	if (ima->source == IMA_SRC_SEQUENCE) {
		/* don't use iuser->framenr directly because it may not be updated if auto-refresh is off */
		const int framenr = BKE_image_user_get_frame(iuser, CFRA, 0);
		ofs += sprintf(str + ofs, ", Frame: %d", framenr);
	}

	(void)ofs;
}

/* gets active viewer user */
struct ImageUser *ntree_get_active_iuser(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree)
		for (node = ntree->nodes.first; node; node = node->next)
			if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
				if (node->flag & NODE_DO_OUTPUT)
					return node->storage;
	return NULL;
}


/* ************ panel stuff ************* */

/* is used for both read and write... */

static int image_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ImBuf *ibuf;
	void *lock;
	int result;

	ibuf = ED_space_image_acquire_buffer(sima, &lock);
	result = ibuf && ibuf->rect_float;
	ED_space_image_release_buffer(sima, lock);
	
	return result;
}

static void image_panel_curves(const bContext *C, Panel *pa)
{
	bScreen *sc = CTX_wm_screen(C);
	SpaceImage *sima = CTX_wm_space_image(C);
	ImBuf *ibuf;
	PointerRNA simaptr;
	int levels;
	void *lock;
	
	ibuf = ED_space_image_acquire_buffer(sima, &lock);
	
	if (ibuf) {
		if (sima->cumap == NULL)
			sima->cumap = curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);

		/* curvemap black/white levels only works for RGBA */
		levels = (ibuf->channels == 4);

		RNA_pointer_create(&sc->id, &RNA_SpaceImageEditor, sima, &simaptr);
		uiTemplateCurveMapping(pa->layout, &simaptr, "curve", 'c', levels, 0);
	}

	ED_space_image_release_buffer(sima, lock);
}

#if 0
/* 0: disable preview 
 * otherwise refresh preview
 *
 * XXX if you put this back, also check XXX in image_main_area_draw() */
 * /
void image_preview_event(int event)
{
	int exec = 0;
	
	if (event == 0) {
		G.scene->r.scemode &= ~R_COMP_CROP;
		exec = 1;
	}
	else {
		if (image_preview_active(curarea, NULL, NULL)) {
			G.scene->r.scemode |= R_COMP_CROP;
			exec = 1;
		}
		else
			G.scene->r.scemode &= ~R_COMP_CROP;
	}
	
	if (exec && G.scene->nodetree) {
		/* should work when no node editor in screen..., so we execute right away */
		
		ntreeCompositTagGenerators(G.scene->nodetree);

		G.afbreek = 0;
		G.scene->nodetree->timecursor = set_timecursor;
		G.scene->nodetree->test_break = blender_test_break;
		
		BIF_store_spare();
		
		ntreeCompositExecTree(G.scene->nodetree, &G.scene->r, 1);   /* 1 is do_previews */
		
		G.scene->nodetree->timecursor = NULL;
		G.scene->nodetree->test_break = NULL;
		
		scrarea_do_windraw(curarea);
		waitcursor(0);
		
		WM_event_add_notifier(C, NC_IMAGE, ima_v);
	}	
}


/* nothing drawn here, we use it to store values */
static void preview_cb(struct ScrArea *sa, struct uiBlock *block)
{
	SpaceImage *sima = sa->spacedata.first;
	rctf dispf;
	rcti *disprect = &G.scene->r.disprect;
	int winx = (G.scene->r.size * G.scene->r.xsch) / 100;
	int winy = (G.scene->r.size * G.scene->r.ysch) / 100;
	int mval[2];
	
	if (G.scene->r.mode & R_BORDER) {
		winx *= (G.scene->r.border.xmax - G.scene->r.border.xmin);
		winy *= (G.scene->r.border.ymax - G.scene->r.border.ymin);
	}
	
	/* while dragging we need to update the rects, otherwise it doesn't end with correct one */

	BLI_init_rctf(&dispf, 15.0f, (block->maxx - block->minx) - 15.0f, 15.0f, (block->maxy - block->miny) - 15.0f);
	ui_graphics_to_window_rct(sa->win, &dispf, disprect);
	
	/* correction for gla draw */
	BLI_translate_rcti(disprect, -curarea->winrct.xmin, -curarea->winrct.ymin);
	
	calc_image_view(sima, 'p');
//	printf("winrct %d %d %d %d\n", disprect->xmin, disprect->ymin,disprect->xmax, disprect->ymax);
	/* map to image space coordinates */
	mval[0] = disprect->xmin; mval[1] = disprect->ymin;
	areamouseco_to_ipoco(v2d, mval, &dispf.xmin, &dispf.ymin);
	mval[0] = disprect->xmax; mval[1] = disprect->ymax;
	areamouseco_to_ipoco(v2d, mval, &dispf.xmax, &dispf.ymax);
	
	/* map to render coordinates */
	disprect->xmin = dispf.xmin;
	disprect->xmax = dispf.xmax;
	disprect->ymin = dispf.ymin;
	disprect->ymax = dispf.ymax;
	
	CLAMP(disprect->xmin, 0, winx);
	CLAMP(disprect->xmax, 0, winx);
	CLAMP(disprect->ymin, 0, winy);
	CLAMP(disprect->ymax, 0, winy);
//	printf("drawrct %d %d %d %d\n", disprect->xmin, disprect->ymin,disprect->xmax, disprect->ymax);

}

static int is_preview_allowed(ScrArea *cur)
{
	SpaceImage *sima = cur->spacedata.first;
	ScrArea *sa;

	/* check if another areawindow has preview set */
	for (sa = G.curscreen->areabase.first; sa; sa = sa->next) {
		if (sa != cur && sa->spacetype == SPACE_IMAGE) {
			if (image_preview_active(sa, NULL, NULL))
				return 0;
		}
	}
	/* check image type */
	if (sima->image == NULL || sima->image->type != IMA_TYPE_COMPOSITE)
		return 0;
	
	return 1;
}


static void image_panel_preview(ScrArea *sa, short cntrl)   // IMAGE_HANDLER_PREVIEW
{
	uiBlock *block;
	SpaceImage *sima = sa->spacedata.first;
	int ofsx, ofsy;
	
	if (is_preview_allowed(sa) == 0) {
		rem_blockhandler(sa, IMAGE_HANDLER_PREVIEW);
		G.scene->r.scemode &= ~R_COMP_CROP; /* quite weak */
		return;
	}
	
	block = uiBeginBlock(C, ar, __func__, UI_EMBOSS);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | UI_PNL_SCALE | cntrl);
	uiSetPanelHandler(IMAGE_HANDLER_PREVIEW);  // for close and esc
	
	ofsx = -150 + (sa->winx / 2) / sima->blockscale;
	ofsy = -100 + (sa->winy / 2) / sima->blockscale;
	if (uiNewPanel(C, ar, block, "Preview", "Image", ofsx, ofsy, 300, 200) == 0) return;
	
	uiBlockSetDrawExtraFunc(block, preview_cb);
	
}
#endif


/* ********************* callbacks for standard image buttons *************** */

static char *slot_menu(void)
{
	char *str;
	int a, slot;
	
	str = MEM_callocN(IMA_MAX_RENDER_SLOT * 32, "menu slots");
	
	strcpy(str, "Slot %t");
	a = strlen(str);

	for (slot = 0; slot < IMA_MAX_RENDER_SLOT; slot++)
		a += sprintf(str + a, "|Slot %d %%x%d", slot + 1, slot);
	
	return str;
}

/* TODO, curlay should be removed? */
static char *layer_menu(RenderResult *rr, short *UNUSED(curlay))
{
	RenderLayer *rl;
	int len = 64 + 32 * BLI_countlist(&rr->layers);
	short a, nr = 0;
	char *str = MEM_callocN(len, "menu layers");
	
	strcpy(str, "Layer %t");
	a = strlen(str);
	
	/* compo result */
	if (rr->rectf) {
		a += sprintf(str + a, "|Composite %%x0");
		nr = 1;
	}
	else if (rr->rect32) {
		a += sprintf(str + a, "|Sequence %%x0");
		nr = 1;
	}
	for (rl = rr->layers.first; rl; rl = rl->next, nr++) {
		a += sprintf(str + a, "|%s %%x%d", rl->name, nr);
	}
	
	/* no curlay clip here, on render (redraws) the amount of layers can be 1 fir single-layer render */
	
	return str;
}

/* rl==NULL means composite result */
static char *pass_menu(RenderLayer *rl, short *curpass)
{
	RenderPass *rpass;
	int len = 64 + 32 * (rl ? BLI_countlist(&rl->passes) : 1);
	short a, nr = 0;
	char *str = MEM_callocN(len, "menu layers");
	
	strcpy(str, "Pass %t");
	a = strlen(str);
	
	/* rendered results don't have a Combined pass */
	if (rl == NULL || rl->rectf) {
		a += sprintf(str + a, "|Combined %%x0");
		nr = 1;
	}
	
	if (rl)
		for (rpass = rl->passes.first; rpass; rpass = rpass->next, nr++)
			a += sprintf(str + a, "|%s %%x%d", rpass->name, nr);
	
	if (*curpass >= nr)
		*curpass = 0;
	
	return str;
}

static void set_frames_cb(bContext *C, void *ima_v, void *iuser_v)
{
	Scene *scene = CTX_data_scene(C);
	Image *ima = ima_v;
	ImageUser *iuser = iuser_v;
	
	if (ima->anim) {
		iuser->frames = IMB_anim_get_duration(ima->anim, IMB_TC_RECORD_RUN);
		BKE_image_user_calc_frame(iuser, scene->r.cfra, 0);
	}
}

/* 5 layer button callbacks... */
static void image_multi_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	ImageUser *iuser = iuser_v;

	BKE_image_multilayer_index(rr_v, iuser); 
	WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
}
static void image_multi_inclay_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	RenderResult *rr = rr_v;
	ImageUser *iuser = iuser_v;
	int tot = BLI_countlist(&rr->layers);

	if (rr->rectf || rr->rect32)
		tot++;  /* fake compo/sequencer layer */

	if (iuser->layer < tot - 1) {
		iuser->layer++;
		BKE_image_multilayer_index(rr, iuser); 
		WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
	}
}
static void image_multi_declay_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	ImageUser *iuser = iuser_v;

	if (iuser->layer > 0) {
		iuser->layer--;
		BKE_image_multilayer_index(rr_v, iuser); 
		WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
	}
}
static void image_multi_incpass_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	RenderResult *rr = rr_v;
	ImageUser *iuser = iuser_v;
	RenderLayer *rl = BLI_findlink(&rr->layers, iuser->layer);

	if (rl) {
		int tot = BLI_countlist(&rl->passes);

		if (rr->rectf || rr->rect32)
			tot++;  /* fake compo/sequencer layer */

		if (iuser->pass < tot - 1) {
			iuser->pass++;
			BKE_image_multilayer_index(rr, iuser); 
			WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
		}
	}
}
static void image_multi_decpass_cb(bContext *C, void *rr_v, void *iuser_v) 
{
	ImageUser *iuser = iuser_v;

	if (iuser->pass > 0) {
		iuser->pass--;
		BKE_image_multilayer_index(rr_v, iuser); 
		WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);
	}
}

#if 0
static void image_freecache_cb(bContext *C, void *ima_v, void *unused) 
{
	Scene *scene = CTX_data_scene(C);
	BKE_image_free_anim_ibufs(ima_v, scene->r.cfra);
	WM_event_add_notifier(C, NC_IMAGE, ima_v);
}
#endif

#if 0
static void image_user_change(bContext *C, void *iuser_v, void *unused)
{
	Scene *scene = CTX_data_scene(C);
	BKE_image_user_calc_imanr(iuser_v, scene->r.cfra, 0);
}
#endif

static void uiblock_layer_pass_buttons(uiLayout *layout, RenderResult *rr, ImageUser *iuser, int w, short *render_slot)
{
	uiBlock *block = uiLayoutGetBlock(layout);
	uiBut *but;
	RenderLayer *rl = NULL;
	int wmenu1, wmenu2, wmenu3, layer;
	char *strp;

	uiLayoutRow(layout, 1);

	/* layer menu is 1/3 larger than pass */
	wmenu1 = (2 * w) / 5;
	wmenu2 = (3 * w) / 5;
	wmenu3 = (3 * w) / 6;
	
	/* menu buts */
	if (render_slot) {
		strp = slot_menu();
		but = uiDefButS(block, MENU, 0, strp,                   0, 0, wmenu1, UI_UNIT_Y, render_slot, 0, 0, 0, 0, "Select Slot");
		uiButSetFunc(but, image_multi_cb, rr, iuser);
		MEM_freeN(strp);
	}

	if (rr) {
		strp = layer_menu(rr, &iuser->layer);
		but = uiDefButS(block, MENU, 0, strp,                   0, 0, wmenu2, UI_UNIT_Y, &iuser->layer, 0, 0, 0, 0, "Select Layer");
		uiButSetFunc(but, image_multi_cb, rr, iuser);
		MEM_freeN(strp);

		layer = iuser->layer;
		if (rr->rectf || rr->rect32)
			layer--;  /* fake compo/sequencer layer */
		
		rl = BLI_findlink(&rr->layers, layer); /* return NULL is meant to be */
		strp = pass_menu(rl, &iuser->pass);
		but = uiDefButS(block, MENU, 0, strp,                   0, 0, wmenu3, UI_UNIT_Y, &iuser->pass, 0, 0, 0, 0, "Select Pass");
		uiButSetFunc(but, image_multi_cb, rr, iuser);
		MEM_freeN(strp);	
	}
}

static void uiblock_layer_pass_arrow_buttons(uiLayout *layout, RenderResult *rr, ImageUser *iuser, short *render_slot)
{
	uiBlock *block = uiLayoutGetBlock(layout);
	uiLayout *row;
	uiBut *but;
	const float dpi_fac = UI_DPI_FAC;
	
	row = uiLayoutRow(layout, 1);

	if (rr == NULL || iuser == NULL)
		return;
	if (rr->layers.first == NULL) {
		uiItemL(row, "No Layers in Render Result", ICON_NONE);
		return;
	}

	/* decrease, increase arrows */
	but = uiDefIconBut(block, BUT, 0, ICON_TRIA_LEFT,   0, 0, 17, 20, NULL, 0, 0, 0, 0, "Previous Layer");
	uiButSetFunc(but, image_multi_declay_cb, rr, iuser);
	but = uiDefIconBut(block, BUT, 0, ICON_TRIA_RIGHT,  0, 0, 18, 20, NULL, 0, 0, 0, 0, "Next Layer");
	uiButSetFunc(but, image_multi_inclay_cb, rr, iuser);

	uiblock_layer_pass_buttons(row, rr, iuser, 230 * dpi_fac, render_slot);

	/* decrease, increase arrows */
	but = uiDefIconBut(block, BUT, 0, ICON_TRIA_LEFT,   0, 0, 17, 20, NULL, 0, 0, 0, 0, "Previous Pass");
	uiButSetFunc(but, image_multi_decpass_cb, rr, iuser);
	but = uiDefIconBut(block, BUT, 0, ICON_TRIA_RIGHT,  0, 0, 18, 20, NULL, 0, 0, 0, 0, "Next Pass");
	uiButSetFunc(but, image_multi_incpass_cb, rr, iuser);

	uiBlockEndAlign(block);
}

// XXX HACK!
// static int packdummy=0;

typedef struct RNAUpdateCb {
	PointerRNA ptr;
	PropertyRNA *prop;
	ImageUser *iuser;
} RNAUpdateCb;

static void rna_update_cb(bContext *C, void *arg_cb, void *UNUSED(arg))
{
	RNAUpdateCb *cb = (RNAUpdateCb *)arg_cb;

	/* ideally this would be done by RNA itself, but there we have
	 * no image user available, so we just update this flag here */
	cb->iuser->ok = 1;

	/* we call update here on the pointer property, this way the
	 * owner of the image pointer can still define it's own update
	 * and notifier */
	RNA_property_update(C, &cb->ptr, cb->prop);
}

void uiTemplateImage(uiLayout *layout, bContext *C, PointerRNA *ptr, const char *propname, PointerRNA *userptr, int compact)
{
	PropertyRNA *prop;
	PointerRNA imaptr;
	RNAUpdateCb *cb;
	Image *ima;
	ImageUser *iuser;
	ImBuf *ibuf;
	Scene *scene = CTX_data_scene(C);
	uiLayout *row, *split, *col;
	uiBlock *block;
	uiBut *but;
	char str[128];
	void *lock;

	if (!ptr->data)
		return;

	prop = RNA_struct_find_property(ptr, propname);
	if (!prop) {
		printf("%s: property not found: %s.%s\n",
		       __func__, RNA_struct_identifier(ptr->type), propname);
		return;
	}

	if (RNA_property_type(prop) != PROP_POINTER) {
		printf("%s: expected pointer property for %s.%s\n",
		       __func__, RNA_struct_identifier(ptr->type), propname);
		return;
	}

	block = uiLayoutGetBlock(layout);

	imaptr = RNA_property_pointer_get(ptr, prop);
	ima = imaptr.data;
	iuser = userptr->data;

	cb = MEM_callocN(sizeof(RNAUpdateCb), "RNAUpdateCb");
	cb->ptr = *ptr;
	cb->prop = prop;
	cb->iuser = iuser;

	uiLayoutSetContextPointer(layout, "edit_image", &imaptr);

	if (!compact)
		uiTemplateID(layout, C, ptr, propname, "IMAGE_OT_new", "IMAGE_OT_open", NULL);

	if (ima) {
		uiBlockSetNFunc(block, rna_update_cb, MEM_dupallocN(cb), NULL);

		if (ima->source == IMA_SRC_VIEWER) {
			ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);
			image_info(scene, iuser, ima, ibuf, str);
			BKE_image_release_ibuf(ima, lock);

			uiItemL(layout, ima->id.name + 2, ICON_NONE);
			uiItemL(layout, str, ICON_NONE);

			if (ima->type == IMA_TYPE_COMPOSITE) {
				// XXX not working yet
#if 0
				iuser = ntree_get_active_iuser(scene->nodetree);
				if (iuser) {
					uiBlockBeginAlign(block);
					uiDefIconTextBut(block, BUT, B_SIMA_RECORD, ICON_REC, "Record", 10, 120, 100, 20, 0, 0, 0, 0, 0, "");
					uiDefIconTextBut(block, BUT, B_SIMA_PLAY, ICON_PLAY, "Play",    110, 120, 100, 20, 0, 0, 0, 0, 0, "");
					but = uiDefBut(block, BUT, B_NOP, "Free Cache", 210, 120, 100, 20, 0, 0, 0, 0, 0, "");
					uiButSetFunc(but, image_freecache_cb, ima, NULL);
					
					if (iuser->frames)
						BLI_snprintf(str, sizeof(str), "(%d) Frames:", iuser->framenr);
					else strcpy(str, "Frames:");
					uiBlockBeginAlign(block);
					uiDefButI(block, NUM, imagechanged, str,        10, 90, 150, 20, &iuser->frames, 0.0, MAXFRAMEF, 0, 0, "Number of images of a movie to use");
					uiDefButI(block, NUM, imagechanged, "StartFr:", 160, 90, 150, 20, &iuser->sfra, 1.0, MAXFRAMEF, 0, 0, "Global starting frame of the movie");
				}
#endif
			}
			else if (ima->type == IMA_TYPE_R_RESULT) {
				/* browse layer/passes */
				Render *re = RE_GetRender(scene->id.name);
				RenderResult *rr = RE_AcquireResultRead(re);
				uiblock_layer_pass_arrow_buttons(layout, rr, iuser, &ima->render_slot);
				RE_ReleaseResult(re);
			}
		}
		else {
			uiItemR(layout, &imaptr, "source", 0, NULL, ICON_NONE);

			if (ima->source != IMA_SRC_GENERATED) {
				row = uiLayoutRow(layout, 1);
				if (ima->packedfile)
					uiItemO(row, "", ICON_PACKAGE, "image.unpack");
				else
					uiItemO(row, "", ICON_UGLYPACKAGE, "image.pack");
				
				row = uiLayoutRow(row, 0);
				uiLayoutSetEnabled(row, ima->packedfile == NULL);
				uiItemR(row, &imaptr, "filepath", 0, "", ICON_NONE);
				uiItemO(row, "", ICON_FILE_REFRESH, "image.reload");
			}

			// XXX what was this for?
#if 0
			/* check for re-render, only buttons */
			if (imagechanged == B_IMAGECHANGED) {
				if (iuser->flag & IMA_ANIM_REFRESHED) {
					iuser->flag &= ~IMA_ANIM_REFRESHED;
					WM_event_add_notifier(C, NC_IMAGE, ima);
				}
			}
#endif

			/* multilayer? */
			if (ima->type == IMA_TYPE_MULTILAYER && ima->rr) {
				uiblock_layer_pass_arrow_buttons(layout, ima->rr, iuser, NULL);
			}
			else if (ima->source != IMA_SRC_GENERATED) {
				if (compact == 0) {
					ibuf = BKE_image_acquire_ibuf(ima, iuser, &lock);
					image_info(scene, iuser, ima, ibuf, str);
					BKE_image_release_ibuf(ima, lock);
					uiItemL(layout, str, ICON_NONE);
				}
			}
			
			if (ima->source != IMA_SRC_GENERATED) {
				if (compact == 0) { /* background image view doesnt need these */
					uiItemS(layout);

					split = uiLayoutSplit(layout, 0, 0);

					col = uiLayoutColumn(split, 0);
					uiItemR(col, &imaptr, "use_fields", 0, NULL, ICON_NONE);
					row = uiLayoutRow(col, 0);
					uiLayoutSetActive(row, RNA_boolean_get(&imaptr, "use_fields"));
					uiItemR(row, &imaptr, "field_order", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
					
					row = uiLayoutRow(layout, 0);
					uiItemR(row, &imaptr, "use_premultiply", 0, NULL, ICON_NONE);
					uiItemR(row, &imaptr, "use_color_unpremultiply", 0, NULL, ICON_NONE);
				}
			}

			if (ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
				uiItemS(layout);
				
				split = uiLayoutSplit(layout, 0, 0);

				col = uiLayoutColumn(split, 0);
				 
				BLI_snprintf(str, sizeof(str), "(%d) Frames", iuser->framenr);
				uiItemR(col, userptr, "frame_duration", 0, str, ICON_NONE);
				if (ima->anim) {
					block = uiLayoutGetBlock(col);
					but = uiDefBut(block, BUT, 0, "Match Movie Length", 0, 0, UI_UNIT_X * 2, UI_UNIT_Y, NULL, 0, 0, 0, 0, "Set the number of frames to match the movie or sequence");
					uiButSetFunc(but, set_frames_cb, ima, iuser);
				}

				uiItemR(col, userptr, "frame_start", 0, "Start", ICON_NONE);
				uiItemR(col, userptr, "frame_offset", 0, NULL, ICON_NONE);

				col = uiLayoutColumn(split, 0);
				row = uiLayoutRow(col, 0);
				uiLayoutSetActive(row, RNA_boolean_get(&imaptr, "use_fields"));
				uiItemR(row, userptr, "fields_per_frame", 0, "Fields", ICON_NONE);
				uiItemR(col, userptr, "use_auto_refresh", 0, NULL, ICON_NONE);
				uiItemR(col, userptr, "use_cyclic", 0, NULL, ICON_NONE);
			}
			else if (ima->source == IMA_SRC_GENERATED) {
				split = uiLayoutSplit(layout, 0, 0);

				col = uiLayoutColumn(split, 1);
				uiItemR(col, &imaptr, "generated_width", 0, "X", ICON_NONE);
				uiItemR(col, &imaptr, "generated_height", 0, "Y", ICON_NONE);
				uiItemR(col, &imaptr, "use_generated_float", 0, NULL, ICON_NONE);

				uiItemR(split, &imaptr, "generated_type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
			}

		}

		uiBlockSetNFunc(block, NULL, NULL, NULL);
	}

	MEM_freeN(cb);
}

void uiTemplateImageSettings(uiLayout *layout, PointerRNA *imfptr)
{
	ImageFormatData *imf = imfptr->data;
	ID *id = imfptr->id.data;
	const int depth_ok = BKE_imtype_valid_depths(imf->imtype);
	/* some settings depend on this being a scene thats rendered */
	const short is_render_out = (id && GS(id->name) == ID_SCE);

	uiLayout *col, *row, *split, *sub;

	col = uiLayoutColumn(layout, 0);

	split = uiLayoutSplit(col, 0.5f, 0);
	
	uiItemR(split, imfptr, "file_format", 0, "", ICON_NONE);
	sub = uiLayoutRow(split, 0);
	uiItemR(sub, imfptr, "color_mode", UI_ITEM_R_EXPAND, "Color", ICON_NONE);

	/* only display depth setting if multiple depths can be used */
	if ((ELEM6(depth_ok,
	           R_IMF_CHAN_DEPTH_1,
	           R_IMF_CHAN_DEPTH_8,
	           R_IMF_CHAN_DEPTH_12,
	           R_IMF_CHAN_DEPTH_16,
	           R_IMF_CHAN_DEPTH_24,
	           R_IMF_CHAN_DEPTH_32)) == 0)
	{
		row = uiLayoutRow(col, 0);
		uiItemR(row, imfptr, "color_depth", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
	}

	if (BKE_imtype_supports_quality(imf->imtype)) {
		uiItemR(col, imfptr, "quality", 0, NULL, ICON_NONE);
	}

	if (BKE_imtype_supports_compress(imf->imtype)) {
		uiItemR(col, imfptr, "compression", 0, NULL, ICON_NONE);
	}

	if (ELEM(imf->imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
		uiItemR(col, imfptr, "exr_codec", 0, NULL, ICON_NONE);
	}
	
	row = uiLayoutRow(col, 0);
	if (BKE_imtype_supports_zbuf(imf->imtype)) {
		uiItemR(row, imfptr, "use_zbuffer", 0, NULL, ICON_NONE);
	}

	if (is_render_out && (imf->imtype == R_IMF_IMTYPE_OPENEXR)) {
		uiItemR(row, imfptr, "use_preview", 0, NULL, ICON_NONE);
	}

	if (imf->imtype == R_IMF_IMTYPE_JP2) {
		row = uiLayoutRow(col, 0);
		uiItemR(row, imfptr, "use_jpeg2k_cinema_preset", 0, NULL, ICON_NONE);
		uiItemR(row, imfptr, "use_jpeg2k_cinema_48", 0, NULL, ICON_NONE);
		
		uiItemR(col, imfptr, "use_jpeg2k_ycc", 0, NULL, ICON_NONE);
	}

	if (imf->imtype == R_IMF_IMTYPE_CINEON) {
#if 1
		uiItemL(col, "Hard coded Non-Linear, Gamma:1.0", ICON_NONE);
#else
		uiItemR(col, imfptr, "use_cineon_log", 0, NULL, ICON_NONE);
		uiItemR(col, imfptr, "cineon_black", 0, NULL, ICON_NONE);
		uiItemR(col, imfptr, "cineon_white", 0, NULL, ICON_NONE);
		uiItemR(col, imfptr, "cineon_gamma", 0, NULL, ICON_NONE);
#endif
	}
}

void uiTemplateImageLayers(uiLayout *layout, bContext *C, Image *ima, ImageUser *iuser)
{
	Scene *scene = CTX_data_scene(C);
	Render *re;
	RenderResult *rr;

	/* render layers and passes */
	if (ima && iuser) {
		const float dpi_fac = UI_DPI_FAC;
		re = RE_GetRender(scene->id.name);
		rr = RE_AcquireResultRead(re);
		uiblock_layer_pass_buttons(layout, rr, iuser, 160 * dpi_fac, (ima->type == IMA_TYPE_R_RESULT) ? &ima->render_slot : NULL);
		RE_ReleaseResult(re);
	}
}

void image_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), "spacetype image panel curves");
	strcpy(pt->idname, "IMAGE_PT_curves");
	strcpy(pt->label, "Curves");
	pt->draw = image_panel_curves;
	pt->poll = image_panel_poll;
	pt->flag |= PNL_DEFAULT_CLOSED;
	BLI_addtail(&art->paneltypes, pt);
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype image panel gpencil");
	strcpy(pt->idname, "IMAGE_PT_gpencil");
	strcpy(pt->label, "Grease Pencil");
	pt->draw = gpencil_panel_standard;
	BLI_addtail(&art->paneltypes, pt);
}

static int image_properties(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = image_has_buttons_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void IMAGE_OT_properties(wmOperatorType *ot)
{
	ot->name = "Properties";
	ot->idname = "IMAGE_OT_properties";
	ot->description = "Toggle display properties panel";
	
	ot->exec = image_properties;
	ot->poll = ED_operator_image_active;
	
	/* flags */
	ot->flag = 0;
}

static int image_scopes(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = image_has_scope_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_scopes(wmOperatorType *ot)
{
	ot->name = "Scopes";
	ot->idname = "IMAGE_OT_scopes";
	ot->description = "Toggle display scopes panel";
	
	ot->exec = image_scopes;
	ot->poll = ED_operator_image_active;
	
	/* flags */
	ot->flag = 0;
}

