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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform.c
 *  \ingroup edtransform
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"		/* PET modes			*/

#include "RNA_access.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_nla.h"
#include "BKE_bmesh.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_global.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_unit.h"

#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_markers.h"
#include "ED_view3d.h"
#include "ED_mesh.h"
#include "ED_clip.h"

#include "UI_view2d.h"
#include "WM_types.h"
#include "WM_api.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_smallhash.h"
#include "BLI_array.h"

#include "UI_resources.h"

//#include "blendef.h"
//
//#include "mydevice.h"

#include "transform.h"

#include <stdio.h>

static void drawTransformApply(const struct bContext *C, struct ARegion *ar, void *arg);
static int doEdgeSlide(TransInfo *t, float perc);

/* ************************** SPACE DEPENDANT CODE **************************** */

void setTransformViewMatrices(TransInfo *t)
{
	if (t->spacetype==SPACE_VIEW3D && t->ar && t->ar->regiontype == RGN_TYPE_WINDOW) {
		RegionView3D *rv3d = t->ar->regiondata;

		copy_m4_m4(t->viewmat, rv3d->viewmat);
		copy_m4_m4(t->viewinv, rv3d->viewinv);
		copy_m4_m4(t->persmat, rv3d->persmat);
		copy_m4_m4(t->persinv, rv3d->persinv);
		t->persp = rv3d->persp;
	}
	else {
		unit_m4(t->viewmat);
		unit_m4(t->viewinv);
		unit_m4(t->persmat);
		unit_m4(t->persinv);
		t->persp = RV3D_ORTHO;
	}

	calculateCenter2D(t);
}

static void convertViewVec2D(View2D *v2d, float vec[3], int dx, int dy)
{
	float divx, divy;
	
	divx= v2d->mask.xmax - v2d->mask.xmin;
	divy= v2d->mask.ymax - v2d->mask.ymin;

	vec[0]= (v2d->cur.xmax - v2d->cur.xmin) * dx / divx;
	vec[1]= (v2d->cur.ymax - v2d->cur.ymin) * dy / divy;
	vec[2]= 0.0f;
}

void convertViewVec(TransInfo *t, float r_vec[3], int dx, int dy)
{
	if ((t->spacetype == SPACE_VIEW3D) && (t->ar->regiontype == RGN_TYPE_WINDOW)) {
		float mval_f[2];
		mval_f[0] = dx;
		mval_f[1] = dy;
		ED_view3d_win_to_delta(t->ar, mval_f, r_vec);
	}
	else if (t->spacetype==SPACE_IMAGE) {
		float aspx, aspy;

		convertViewVec2D(t->view, r_vec, dx, dy);

		ED_space_image_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);
		r_vec[0] *= aspx;
		r_vec[1] *= aspy;
	}
	else if (ELEM(t->spacetype, SPACE_IPO, SPACE_NLA)) {
		convertViewVec2D(t->view, r_vec, dx, dy);
	}
	else if (ELEM(t->spacetype, SPACE_NODE, SPACE_SEQ)) {
		convertViewVec2D(&t->ar->v2d, r_vec, dx, dy);
	}
	else if (t->spacetype==SPACE_CLIP) {
		View2D *v2d = t->view;
		float divx, divy;

		divx = v2d->mask.xmax-v2d->mask.xmin;
		divy = v2d->mask.ymax-v2d->mask.ymin;

		r_vec[0] = (v2d->cur.xmax-v2d->cur.xmin)*(dx)/divx;
		r_vec[1] = (v2d->cur.ymax-v2d->cur.ymin)*(dy)/divy;
		r_vec[2] = 0.0f;
	}
	else {
		printf("%s: called in an invalid context\n", __func__);
		zero_v3(r_vec);
	}
}

void projectIntView(TransInfo *t, const float vec[3], int adr[2])
{
	if (t->spacetype==SPACE_VIEW3D) {
		if (t->ar->regiontype == RGN_TYPE_WINDOW)
			project_int_noclip(t->ar, vec, adr);
	}
	else if (t->spacetype==SPACE_IMAGE) {
		float aspx, aspy, v[2];

		ED_space_image_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);
		v[0]= vec[0]/aspx;
		v[1]= vec[1]/aspy;

		UI_view2d_to_region_no_clip(t->view, v[0], v[1], adr, adr+1);
	}
	else if (t->spacetype == SPACE_ACTION) {
		int out[2] = {0, 0};
#if 0
		SpaceAction *sact = t->sa->spacedata.first;

		if (sact->flag & SACTION_DRAWTIME) {
			//vec[0] = vec[0]/((t->scene->r.frs_sec / t->scene->r.frs_sec_base));
			/* same as below */
			UI_view2d_to_region_no_clip((View2D *)t->view, vec[0], vec[1], out, out+1);
		} 
		else
#endif
		{
			UI_view2d_to_region_no_clip((View2D *)t->view, vec[0], vec[1], out, out+1);
		}

		adr[0]= out[0];
		adr[1]= out[1];
	}
	else if (ELEM(t->spacetype, SPACE_IPO, SPACE_NLA)) {
		int out[2] = {0, 0};

		UI_view2d_to_region_no_clip((View2D *)t->view, vec[0], vec[1], out, out+1);
		adr[0]= out[0];
		adr[1]= out[1];
	}
	else if (t->spacetype==SPACE_SEQ) { /* XXX not tested yet, but should work */
		int out[2] = {0, 0};

		UI_view2d_to_region_no_clip((View2D *)t->view, vec[0], vec[1], out, out+1);
		adr[0]= out[0];
		adr[1]= out[1];
	}
	else if (t->spacetype==SPACE_CLIP) {
		UI_view2d_to_region_no_clip(t->view, vec[0], vec[1], adr, adr+1);
	}
}

void projectFloatView(TransInfo *t, const float vec[3], float adr[2])
{
	switch (t->spacetype) {
		case SPACE_VIEW3D:
		{
			if (t->ar->regiontype == RGN_TYPE_WINDOW) {
				project_float_noclip(t->ar, vec, adr);
				return;
			}
			break;
		}
		case SPACE_IMAGE:
		case SPACE_CLIP:
		case SPACE_IPO:
		case SPACE_NLA:
		{
			int a[2];
			projectIntView(t, vec, a);
			adr[0] = a[0];
			adr[1] = a[1];
			return;
		}
	}

	zero_v2(adr);
}

void applyAspectRatio(TransInfo *t, float *vec)
{
	if ((t->spacetype==SPACE_IMAGE) && (t->mode==TFM_TRANSLATION)) {
		SpaceImage *sima= t->sa->spacedata.first;
		float aspx, aspy;

		if ((sima->flag & SI_COORDFLOATS)==0) {
			int width, height;
			ED_space_image_size(sima, &width, &height);

			vec[0] *= width;
			vec[1] *= height;
		}

		ED_space_image_uv_aspect(sima, &aspx, &aspy);
		vec[0] /= aspx;
		vec[1] /= aspy;
	}
}

void removeAspectRatio(TransInfo *t, float *vec)
{
	if ((t->spacetype==SPACE_IMAGE) && (t->mode==TFM_TRANSLATION)) {
		SpaceImage *sima= t->sa->spacedata.first;
		float aspx, aspy;

		if ((sima->flag & SI_COORDFLOATS)==0) {
			int width, height;
			ED_space_image_size(sima, &width, &height);

			vec[0] /= width;
			vec[1] /= height;
		}

		ED_space_image_uv_aspect(sima, &aspx, &aspy);
		vec[0] *= aspx;
		vec[1] *= aspy;
	}
}

static void viewRedrawForce(const bContext *C, TransInfo *t)
{
	if (t->spacetype == SPACE_VIEW3D)
	{
		/* Do we need more refined tags? */
		if (t->flag & T_POSE)
			WM_event_add_notifier(C, NC_OBJECT|ND_POSE, NULL);
		else
			WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
		
		/* for realtime animation record - send notifiers recognised by animation editors */
		// XXX: is this notifier a lame duck?
		if ((t->animtimer) && IS_AUTOKEY_ON(t->scene))
			WM_event_add_notifier(C, NC_OBJECT|ND_KEYS, NULL);
		
	}
	else if (t->spacetype == SPACE_ACTION) {
		//SpaceAction *saction= (SpaceAction *)t->sa->spacedata.first;
		WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|NA_EDITED, NULL);
	}
	else if (t->spacetype == SPACE_IPO) {
		//SpaceIpo *sipo= (SpaceIpo *)t->sa->spacedata.first;
		WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME|NA_EDITED, NULL);
	}
	else if (t->spacetype == SPACE_NLA) {
		WM_event_add_notifier(C, NC_ANIMATION|ND_NLA|NA_EDITED, NULL);
	}
	else if (t->spacetype == SPACE_NODE) {
		//ED_area_tag_redraw(t->sa);
		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_NODE_VIEW, NULL);
	}
	else if (t->spacetype == SPACE_SEQ) {
		WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, NULL);
	}
	else if (t->spacetype==SPACE_IMAGE) {
		// XXX how to deal with lock?
		SpaceImage *sima= (SpaceImage*)t->sa->spacedata.first;
		if (sima->lock) WM_event_add_notifier(C, NC_GEOM|ND_DATA, t->obedit->data);
		else ED_area_tag_redraw(t->sa);
	}
	else if (t->spacetype==SPACE_CLIP) {
		SpaceClip *sc = (SpaceClip*)t->sa->spacedata.first;
		MovieClip *clip = ED_space_clip(sc);

		/* objects could be parented to tracking data, so send this for viewport refresh */
		WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);

		WM_event_add_notifier(C, NC_MOVIECLIP|NA_EDITED, clip);
	}
}

static void viewRedrawPost(bContext *C, TransInfo *t)
{
	ED_area_headerprint(t->sa, NULL);
	
	if (t->spacetype == SPACE_VIEW3D) {
		/* if autokeying is enabled, send notifiers that keyframes were added */
		if (IS_AUTOKEY_ON(t->scene))
			WM_main_add_notifier(NC_ANIMATION|ND_KEYFRAME|NA_EDITED, NULL);
		
		/* XXX temp, first hack to get auto-render in compositor work (ton) */
		WM_event_add_notifier(C, NC_SCENE|ND_TRANSFORM_DONE, CTX_data_scene(C));

	}
	
#if 0 // TRANSFORM_FIX_ME
	if (t->spacetype==SPACE_VIEW3D) {
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
	else if (t->spacetype==SPACE_IMAGE) {
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
	else if (ELEM3(t->spacetype, SPACE_ACTION, SPACE_NLA, SPACE_IPO)) {
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWTIME, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
	}

	scrarea_queue_headredraw(curarea);
#endif
}

/* ************************** TRANSFORMATIONS **************************** */

void BIF_selectOrientation(void)
{
#if 0 // TRANSFORM_FIX_ME
	short val;
	char *str_menu = BIF_menustringTransformOrientation("Orientation");
	val= pupmenu(str_menu);
	MEM_freeN(str_menu);

	if (val >= 0) {
		G.vd->twmode = val;
	}
#endif
}

static void view_editmove(unsigned short UNUSED(event))
{
#if 0 // TRANSFORM_FIX_ME
	int refresh = 0;
	/* Regular:   Zoom in */
	/* Shift:     Scroll up */
	/* Ctrl:      Scroll right */
	/* Alt-Shift: Rotate up */
	/* Alt-Ctrl:  Rotate right */

	/* only work in 3D window for now
	 * In the end, will have to send to event to a 2D window handler instead
	 */
	if (Trans.flag & T_2D_EDIT)
		return;

	switch(event) {
		case WHEELUPMOUSE:

			if ( G.qual & LR_SHIFTKEY ) {
				if ( G.qual & LR_ALTKEY ) {
					G.qual &= ~LR_SHIFTKEY;
					persptoetsen(PAD2);
					G.qual |= LR_SHIFTKEY;
				}
				else {
					persptoetsen(PAD2);
				}
			}
			else if ( G.qual & LR_CTRLKEY ) {
				if ( G.qual & LR_ALTKEY ) {
					G.qual &= ~LR_CTRLKEY;
					persptoetsen(PAD4);
					G.qual |= LR_CTRLKEY;
				}
				else {
					persptoetsen(PAD4);
				}
			}
			else if (U.uiflag & USER_WHEELZOOMDIR)
				persptoetsen(PADMINUS);
			else
				persptoetsen(PADPLUSKEY);

			refresh = 1;
			break;
		case WHEELDOWNMOUSE:
			if ( G.qual & LR_SHIFTKEY ) {
				if ( G.qual & LR_ALTKEY ) {
					G.qual &= ~LR_SHIFTKEY;
					persptoetsen(PAD8);
					G.qual |= LR_SHIFTKEY;
				}
				else {
					persptoetsen(PAD8);
				}
			}
			else if ( G.qual & LR_CTRLKEY ) {
				if ( G.qual & LR_ALTKEY ) {
					G.qual &= ~LR_CTRLKEY;
					persptoetsen(PAD6);
					G.qual |= LR_CTRLKEY;
				}
				else {
					persptoetsen(PAD6);
				}
			}
			else if (U.uiflag & USER_WHEELZOOMDIR)
				persptoetsen(PADPLUSKEY);
			else
				persptoetsen(PADMINUS);

			refresh = 1;
			break;
	}

	if (refresh)
		setTransformViewMatrices(&Trans);
#endif
}

/* ************************************************* */

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
#define TFM_MODAL_CANCEL		1
#define TFM_MODAL_CONFIRM		2
#define TFM_MODAL_TRANSLATE		3
#define TFM_MODAL_ROTATE		4
#define TFM_MODAL_RESIZE		5
#define TFM_MODAL_SNAP_INV_ON	6
#define TFM_MODAL_SNAP_INV_OFF	7
#define TFM_MODAL_SNAP_TOGGLE	8
#define TFM_MODAL_AXIS_X		9
#define TFM_MODAL_AXIS_Y		10
#define TFM_MODAL_AXIS_Z		11
#define TFM_MODAL_PLANE_X		12
#define TFM_MODAL_PLANE_Y		13
#define TFM_MODAL_PLANE_Z		14
#define TFM_MODAL_CONS_OFF		15
#define TFM_MODAL_ADD_SNAP		16
#define TFM_MODAL_REMOVE_SNAP	17
/*	18 and 19 used by numinput, defined in transform.h
 * */
#define TFM_MODAL_PROPSIZE_UP	20
#define TFM_MODAL_PROPSIZE_DOWN	21
#define TFM_MODAL_AUTOIK_LEN_INC 22
#define TFM_MODAL_AUTOIK_LEN_DEC 23

/* called in transform_ops.c, on each regeneration of keymaps */
wmKeyMap* transform_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
	{TFM_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
	{TFM_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
	{TFM_MODAL_TRANSLATE, "TRANSLATE", 0, "Translate", ""},
	{TFM_MODAL_ROTATE, "ROTATE", 0, "Rotate", ""},
	{TFM_MODAL_RESIZE, "RESIZE", 0, "Resize", ""},
	{TFM_MODAL_SNAP_INV_ON, "SNAP_INV_ON", 0, "Invert Snap On", ""},
	{TFM_MODAL_SNAP_INV_OFF, "SNAP_INV_OFF", 0, "Invert Snap Off", ""},
	{TFM_MODAL_SNAP_TOGGLE, "SNAP_TOGGLE", 0, "Snap Toggle", ""},
	{TFM_MODAL_AXIS_X, "AXIS_X", 0, "Orientation X axis", ""},
	{TFM_MODAL_AXIS_Y, "AXIS_Y", 0, "Orientation Y axis", ""},
	{TFM_MODAL_AXIS_Z, "AXIS_Z", 0, "Orientation Z axis", ""},
	{TFM_MODAL_PLANE_X, "PLANE_X", 0, "Orientation X plane", ""},
	{TFM_MODAL_PLANE_Y, "PLANE_Y", 0, "Orientation Y plane", ""},
	{TFM_MODAL_PLANE_Z, "PLANE_Z", 0, "Orientation Z plane", ""},
	{TFM_MODAL_CONS_OFF, "CONS_OFF", 0, "Remove Constraints", ""},
	{TFM_MODAL_ADD_SNAP, "ADD_SNAP", 0, "Add Snap Point", ""},
	{TFM_MODAL_REMOVE_SNAP, "REMOVE_SNAP", 0, "Remove Last Snap Point", ""},
	{NUM_MODAL_INCREMENT_UP, "INCREMENT_UP", 0, "Numinput Increment Up", ""},
	{NUM_MODAL_INCREMENT_DOWN, "INCREMENT_DOWN", 0, "Numinput Increment Down", ""},
	{TFM_MODAL_PROPSIZE_UP, "PROPORTIONAL_SIZE_UP", 0, "Increase Proportional Influence", ""},
	{TFM_MODAL_PROPSIZE_DOWN, "PROPORTIONAL_SIZE_DOWN", 0, "Decrease Proportional Influence", ""},
	{TFM_MODAL_AUTOIK_LEN_INC, "AUTOIK_CHAIN_LEN_UP", 0, "Increase Max AutoIK Chain Length", ""},
	{TFM_MODAL_AUTOIK_LEN_DEC, "AUTOIK_CHAIN_LEN_DOWN", 0, "Decrease Max AutoIK Chain Length", ""},
	{0, NULL, 0, NULL, NULL}};
	
	wmKeyMap *keymap= WM_modalkeymap_get(keyconf, "Transform Modal Map");
	
	/* this function is called for each spacetype, only needs to add map once */
	if (keymap) return NULL;
	
	keymap= WM_modalkeymap_add(keyconf, "Transform Modal Map", modal_items);
	
	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, TFM_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, TFM_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, TFM_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, TFM_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, GKEY, KM_PRESS, 0, 0, TFM_MODAL_TRANSLATE);
	WM_modalkeymap_add_item(keymap, RKEY, KM_PRESS, 0, 0, TFM_MODAL_ROTATE);
	WM_modalkeymap_add_item(keymap, SKEY, KM_PRESS, 0, 0, TFM_MODAL_RESIZE);
	
	WM_modalkeymap_add_item(keymap, TABKEY, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_SNAP_TOGGLE);

	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, TFM_MODAL_SNAP_INV_ON);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, TFM_MODAL_SNAP_INV_OFF);

	WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_PRESS, KM_ANY, 0, TFM_MODAL_SNAP_INV_ON);
	WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_RELEASE, KM_ANY, 0, TFM_MODAL_SNAP_INV_OFF);
	
	WM_modalkeymap_add_item(keymap, AKEY, KM_PRESS, 0, 0, TFM_MODAL_ADD_SNAP);
	WM_modalkeymap_add_item(keymap, AKEY, KM_PRESS, KM_ALT, 0, TFM_MODAL_REMOVE_SNAP);

	WM_modalkeymap_add_item(keymap, PAGEUPKEY, KM_PRESS, 0, 0, TFM_MODAL_PROPSIZE_UP);
	WM_modalkeymap_add_item(keymap, PAGEDOWNKEY, KM_PRESS, 0, 0, TFM_MODAL_PROPSIZE_DOWN);
	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, 0, 0, TFM_MODAL_PROPSIZE_UP);
	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, 0, 0, TFM_MODAL_PROPSIZE_DOWN);
	
	WM_modalkeymap_add_item(keymap, PAGEUPKEY, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_AUTOIK_LEN_INC);
	WM_modalkeymap_add_item(keymap, PAGEDOWNKEY, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_AUTOIK_LEN_DEC);
	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_AUTOIK_LEN_INC);
	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_AUTOIK_LEN_DEC);
	
	return keymap;
}


int transformEvent(TransInfo *t, wmEvent *event)
{
	float mati[3][3]= MAT3_UNITY;
	char cmode = constraintModeToChar(t);
	int handled = 1;

	t->redraw |= handleMouseInput(t, &t->mouse, event);

	if (event->type == MOUSEMOVE)
	{
		if (t->modifiers & MOD_CONSTRAINT_SELECT)
			t->con.mode |= CON_SELECT;

		copy_v2_v2_int(t->mval, event->mval);

		// t->redraw |= TREDRAW_SOFT; /* Use this for soft redraw. Might cause flicker in object mode */
		t->redraw |= TREDRAW_HARD;


		if (t->state == TRANS_STARTING) {
			t->state = TRANS_RUNNING;
		}

		applyMouseInput(t, &t->mouse, t->mval, t->values);

		// Snapping mouse move events
		t->redraw |= handleSnapping(t, event);
	}

	/* handle modal keymap first */
	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case TFM_MODAL_CANCEL:
				t->state = TRANS_CANCEL;
				break;
			case TFM_MODAL_CONFIRM:
				t->state = TRANS_CONFIRM;
				break;
			case TFM_MODAL_TRANSLATE:
				/* only switch when... */
				if ( ELEM3(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL) ) {
					resetTransRestrictions(t);
					restoreTransObjects(t);
					initTranslation(t);
					initSnapping(t, NULL); // need to reinit after mode change
					t->redraw |= TREDRAW_HARD;
				}
				else if (t->mode == TFM_TRANSLATION) {
					if (t->options&CTX_MOVIECLIP) {
						restoreTransObjects(t);

						t->flag^= T_ALT_TRANSFORM;
						t->redraw |= TREDRAW_HARD;
					}
				}
				break;
			case TFM_MODAL_ROTATE:
				/* only switch when... */
				if (!(t->options & CTX_TEXTURE) && !(t->options & CTX_MOVIECLIP)) {
					if ( ELEM4(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL, TFM_TRANSLATION) ) {
						
						resetTransRestrictions(t);
						
						if (t->mode == TFM_ROTATION) {
							restoreTransObjects(t);
							initTrackball(t);
						}
						else {
							restoreTransObjects(t);
							initRotation(t);
						}
						initSnapping(t, NULL); // need to reinit after mode change
						t->redraw |= TREDRAW_HARD;
					}
				}
				break;
			case TFM_MODAL_RESIZE:
				/* only switch when... */
				if ( ELEM3(t->mode, TFM_ROTATION, TFM_TRANSLATION, TFM_TRACKBALL) ) {
					resetTransRestrictions(t);
					restoreTransObjects(t);
					initResize(t);
					initSnapping(t, NULL); // need to reinit after mode change
					t->redraw |= TREDRAW_HARD;
				}
				break;
				
			case TFM_MODAL_SNAP_INV_ON:
				t->modifiers |= MOD_SNAP_INVERT;
				t->redraw |= TREDRAW_HARD;
				break;
			case TFM_MODAL_SNAP_INV_OFF:
				t->modifiers &= ~MOD_SNAP_INVERT;
				t->redraw |= TREDRAW_HARD;
				break;
			case TFM_MODAL_SNAP_TOGGLE:
				t->modifiers ^= MOD_SNAP;
				t->redraw |= TREDRAW_HARD;
				break;
			case TFM_MODAL_AXIS_X:
				if ((t->flag & T_NO_CONSTRAINT)==0) {
					if (cmode == 'X') {
						stopConstraint(t);
					}
					else {
						if (t->flag & T_2D_EDIT) {
							setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS0), "along X");
						}
						else {
							setUserConstraint(t, t->current_orientation, (CON_AXIS0), "along %s X");
						}
					}
					t->redraw |= TREDRAW_HARD;
				}
				break;
			case TFM_MODAL_AXIS_Y:
				if ((t->flag & T_NO_CONSTRAINT)==0) {
					if (cmode == 'Y') {
						stopConstraint(t);
					}
					else {
						if (t->flag & T_2D_EDIT) {
							setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS1), "along Y");
						}
						else {
							setUserConstraint(t, t->current_orientation, (CON_AXIS1), "along %s Y");
						}
					}
					t->redraw |= TREDRAW_HARD;
				}
				break;
			case TFM_MODAL_AXIS_Z:
				if ((t->flag & (T_NO_CONSTRAINT|T_2D_EDIT))== 0) {
					if (cmode == 'Z') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, t->current_orientation, (CON_AXIS2), "along %s Z");
					}
					t->redraw |= TREDRAW_HARD;
				}
				break;
			case TFM_MODAL_PLANE_X:
				if ((t->flag & (T_NO_CONSTRAINT|T_2D_EDIT))== 0) {
					if (cmode == 'X') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, t->current_orientation, (CON_AXIS1|CON_AXIS2), "locking %s X");
					}
					t->redraw |= TREDRAW_HARD;
				}
				break;
			case TFM_MODAL_PLANE_Y:
				if ((t->flag & (T_NO_CONSTRAINT|T_2D_EDIT))== 0) {
					if (cmode == 'Y') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, t->current_orientation, (CON_AXIS0|CON_AXIS2), "locking %s Y");
					}
					t->redraw |= TREDRAW_HARD;
				}
				break;
			case TFM_MODAL_PLANE_Z:
				if ((t->flag & (T_NO_CONSTRAINT|T_2D_EDIT))== 0) {
					if (cmode == 'Z') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, t->current_orientation, (CON_AXIS0|CON_AXIS1), "locking %s Z");
					}
					t->redraw |= TREDRAW_HARD;
				}
				break;
			case TFM_MODAL_CONS_OFF:
				if ((t->flag & T_NO_CONSTRAINT)==0) {
					stopConstraint(t);
					t->redraw |= TREDRAW_HARD;
				}
				break;
			case TFM_MODAL_ADD_SNAP:
				addSnapPoint(t);
				t->redraw |= TREDRAW_HARD;
				break;
			case TFM_MODAL_REMOVE_SNAP:
				removeSnapPoint(t);
				t->redraw |= TREDRAW_HARD;
				break;
			case TFM_MODAL_PROPSIZE_UP:
				if (t->flag & T_PROP_EDIT) {
					t->prop_size*= 1.1f;
					if (t->spacetype==SPACE_VIEW3D && t->persp != RV3D_ORTHO)
						t->prop_size= MIN2(t->prop_size, ((View3D *)t->view)->far);
					calculatePropRatio(t);
				}
				t->redraw |= TREDRAW_HARD;
				break;
			case TFM_MODAL_PROPSIZE_DOWN:
				if (t->flag & T_PROP_EDIT) {
					t->prop_size*= 0.90909090f;
					calculatePropRatio(t);
				}
				t->redraw |= TREDRAW_HARD;
				break;
			case TFM_MODAL_AUTOIK_LEN_INC:
				if (t->flag & T_AUTOIK)
					transform_autoik_update(t, 1);
				t->redraw |= TREDRAW_HARD;
				break;
			case TFM_MODAL_AUTOIK_LEN_DEC:
				if (t->flag & T_AUTOIK) 
					transform_autoik_update(t, -1);
				t->redraw |= TREDRAW_HARD;
				break;
			default:
				handled = 0;
				break;
		}

		// Modal numinput events
		t->redraw |= handleNumInput(&(t->num), event);
	}
	/* else do non-mapped events */
	else if (event->val==KM_PRESS) {
		switch (event->type) {
		case RIGHTMOUSE:
			t->state = TRANS_CANCEL;
			break;
		/* enforce redraw of transform when modifiers are used */
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			t->modifiers |= MOD_CONSTRAINT_PLANE;
			t->redraw |= TREDRAW_HARD;
			break;

		case SPACEKEY:
			if ((t->spacetype==SPACE_VIEW3D) && event->alt) {
#if 0 // TRANSFORM_FIX_ME
				int mval[2];

				getmouseco_sc(mval);
				BIF_selectOrientation();
				calc_manipulator_stats(curarea);
				copy_m3_m4(t->spacemtx, G.vd->twmat);
				warp_pointer(mval[0], mval[1]);
#endif
			}
			else {
				t->state = TRANS_CONFIRM;
			}
			break;

		case MIDDLEMOUSE:
			if ((t->flag & T_NO_CONSTRAINT)==0) {
				/* exception for switching to dolly, or trackball, in camera view */
				if (t->flag & T_CAMERA) {
					if (t->mode==TFM_TRANSLATION)
						setLocalConstraint(t, (CON_AXIS2), "along local Z");
					else if (t->mode==TFM_ROTATION) {
						restoreTransObjects(t);
						initTrackball(t);
					}
				}
				else {
					t->modifiers |= MOD_CONSTRAINT_SELECT;
					if (t->con.mode & CON_APPLY) {
						stopConstraint(t);
					}
					else {
						if (event->shift) {
							initSelectConstraint(t, t->spacemtx);
						}
						else {
							/* bit hackish... but it prevents mmb select to print the orientation from menu */
							strcpy(t->spacename, "global");
							initSelectConstraint(t, mati);
						}
						postSelectConstraint(t);
					}
				}
				t->redraw |= TREDRAW_HARD;
			}
			break;
		case ESCKEY:
			t->state = TRANS_CANCEL;
			break;
		case PADENTER:
		case RETKEY:
			t->state = TRANS_CONFIRM;
			break;
		case GKEY:
			/* only switch when... */
			if ( ELEM3(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL) ) {
				resetTransRestrictions(t);
				restoreTransObjects(t);
				initTranslation(t);
				initSnapping(t, NULL); // need to reinit after mode change
				t->redraw |= TREDRAW_HARD;
			}
			break;
		case SKEY:
			/* only switch when... */
			if ( ELEM3(t->mode, TFM_ROTATION, TFM_TRANSLATION, TFM_TRACKBALL) ) {
				resetTransRestrictions(t);
				restoreTransObjects(t);
				initResize(t);
				initSnapping(t, NULL); // need to reinit after mode change
				t->redraw |= TREDRAW_HARD;
			}
			break;
		case RKEY:
			/* only switch when... */
			if (!(t->options & CTX_TEXTURE) && !(t->options & CTX_MOVIECLIP)) {
				if ( ELEM4(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL, TFM_TRANSLATION) ) {

					resetTransRestrictions(t);

					if (t->mode == TFM_ROTATION) {
						restoreTransObjects(t);
						initTrackball(t);
					}
					else {
						restoreTransObjects(t);
						initRotation(t);
					}
					initSnapping(t, NULL); // need to reinit after mode change
					t->redraw |= TREDRAW_HARD;
				}
			}
			break;
		case CKEY:
			if (event->alt) {
				t->flag ^= T_PROP_CONNECTED;
				sort_trans_data_dist(t);
				calculatePropRatio(t);
				t->redraw= 1;
			}
			else {
				stopConstraint(t);
				t->redraw |= TREDRAW_HARD;
			}
			break;
		case XKEY:
			if ((t->flag & T_NO_CONSTRAINT)==0) {
				if (t->flag & T_2D_EDIT) {
					if (cmode == 'X') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS0), "along X");
					}
				}
				else {
					if (cmode == 'X') {
						if (t->con.orientation != V3D_MANIP_GLOBAL) {
							stopConstraint(t);
						}
						else {
							short orientation = t->current_orientation != V3D_MANIP_GLOBAL ? t->current_orientation : V3D_MANIP_LOCAL;
							if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
								setUserConstraint(t, orientation, (CON_AXIS0), "along %s X");
							else if (t->modifiers & MOD_CONSTRAINT_PLANE)
								setUserConstraint(t, orientation, (CON_AXIS1|CON_AXIS2), "locking %s X");
						}
					}
					else {
						if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
							setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS0), "along %s X");
						else if (t->modifiers & MOD_CONSTRAINT_PLANE)
							setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS1|CON_AXIS2), "locking %s X");
					}
				}
				t->redraw |= TREDRAW_HARD;
			}
			break;
		case YKEY:
			if ((t->flag & T_NO_CONSTRAINT)==0) {
				if (t->flag & T_2D_EDIT) {
					if (cmode == 'Y') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS1), "along Y");
					}
				}
				else {
					if (cmode == 'Y') {
						if (t->con.orientation != V3D_MANIP_GLOBAL) {
							stopConstraint(t);
						}
						else {
							short orientation = t->current_orientation != V3D_MANIP_GLOBAL ? t->current_orientation : V3D_MANIP_LOCAL;
							if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
								setUserConstraint(t, orientation, (CON_AXIS1), "along %s Y");
							else if (t->modifiers & MOD_CONSTRAINT_PLANE)
								setUserConstraint(t, orientation, (CON_AXIS0|CON_AXIS2), "locking %s Y");
						}
					}
					else {
						if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
							setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS1), "along %s Y");
						else if (t->modifiers & MOD_CONSTRAINT_PLANE)
							setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS0|CON_AXIS2), "locking %s Y");
					}
				}
				t->redraw |= TREDRAW_HARD;
			}
			break;
		case ZKEY:
			if ((t->flag & (T_NO_CONSTRAINT|T_2D_EDIT))==0) {
				if (cmode == 'Z') {
					if (t->con.orientation != V3D_MANIP_GLOBAL) {
						stopConstraint(t);
					}
					else {
						short orientation = t->current_orientation != V3D_MANIP_GLOBAL ? t->current_orientation : V3D_MANIP_LOCAL;
						if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
							setUserConstraint(t, orientation, (CON_AXIS2), "along %s Z");
						else if (t->modifiers & MOD_CONSTRAINT_PLANE)
							setUserConstraint(t, orientation, (CON_AXIS0|CON_AXIS1), "locking %s Z");
					}
				}
				else {
					if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
						setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS2), "along %s Z");
					else if (t->modifiers & MOD_CONSTRAINT_PLANE)
						setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS0|CON_AXIS1), "locking %s Z");
				}
				t->redraw |= TREDRAW_HARD;
			}
			break;
		case OKEY:
			if (t->flag & T_PROP_EDIT && event->shift) {
				t->prop_mode = (t->prop_mode + 1) % PROP_MODE_MAX;
				calculatePropRatio(t);
				t->redraw |= TREDRAW_HARD;
			}
			break;
		case PADPLUSKEY:
			if (event->alt && t->flag & T_PROP_EDIT) {
				t->prop_size *= 1.1f;
				if (t->spacetype==SPACE_VIEW3D && t->persp != RV3D_ORTHO)
					t->prop_size= MIN2(t->prop_size, ((View3D *)t->view)->far);
				calculatePropRatio(t);
			}
			t->redraw= 1;
			break;
		case PAGEUPKEY:
		case WHEELDOWNMOUSE:
			if (t->flag & T_AUTOIK) {
				transform_autoik_update(t, 1);
			}
			else view_editmove(event->type);
			t->redraw= 1;
			break;
		case PADMINUS:
			if (event->alt && t->flag & T_PROP_EDIT) {
				t->prop_size*= 0.90909090f;
				calculatePropRatio(t);
			}
			t->redraw= 1;
			break;
		case PAGEDOWNKEY:
		case WHEELUPMOUSE:
			if (t->flag & T_AUTOIK) {
				transform_autoik_update(t, -1);
			}
			else view_editmove(event->type);
			t->redraw= 1;
			break;
		default:
			handled = 0;
			break;
		}

		// Numerical input events
		t->redraw |= handleNumInput(&(t->num), event);

		// Snapping key events
		t->redraw |= handleSnapping(t, event);

	}
	else if (event->val==KM_RELEASE) {
		switch (event->type) {
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			t->modifiers &= ~MOD_CONSTRAINT_PLANE;
			t->redraw |= TREDRAW_HARD;
			break;

		case MIDDLEMOUSE:
			if ((t->flag & T_NO_CONSTRAINT)==0) {
				t->modifiers &= ~MOD_CONSTRAINT_SELECT;
				postSelectConstraint(t);
				t->redraw |= TREDRAW_HARD;
			}
			break;
//		case LEFTMOUSE:
//		case RIGHTMOUSE:
//			if (WM_modal_tweak_exit(event, t->event_type))
////			if (t->options & CTX_TWEAK)
//				t->state = TRANS_CONFIRM;
//			break;
		default:
			handled = 0;
			break;
		}

		/* confirm transform if launch key is released after mouse move */
		if (t->flag & T_RELEASE_CONFIRM)
		{
			/* XXX Keyrepeat bug in Xorg fucks this up, will test when fixed */
			if (event->type == t->launch_event && (t->launch_event == LEFTMOUSE || t->launch_event == RIGHTMOUSE))
			{
				t->state = TRANS_CONFIRM;
			}
		}
	}

	// Per transform event, if present
	if (t->handleEvent)
		t->redraw |= t->handleEvent(t, event);

	if (handled || t->redraw)
		return 0;
	else
		return OPERATOR_PASS_THROUGH;
}

int calculateTransformCenter(bContext *C, int centerMode, float *vec)
{
	TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data");
	int success = 1;

	t->state = TRANS_RUNNING;

	t->options = CTX_NONE;

	t->mode = TFM_DUMMY;

	initTransInfo(C, t, NULL, NULL);	// internal data, mouse, vectors

	createTransData(C, t);				// make TransData structs from selection

	t->around = centerMode; 			// override userdefined mode

	if (t->total == 0) {
		success = 0;
	}
	else {
		success = 1;

		calculateCenter(t);

		// Copy center from constraint center. Transform center can be local
		copy_v3_v3(vec, t->con.center);
	}


	/* aftertrans does insert ipos and action channels, and clears base flags, doesnt read transdata */
	special_aftertrans_update(C, t);

	postTrans(C, t);

	MEM_freeN(t);

	return success;
}

typedef enum {
	UP,
	DOWN,
	LEFT,
	RIGHT
} ArrowDirection;
static void drawArrow(ArrowDirection d, short offset, short length, short size)
{
	switch(d)
	{
		case LEFT:
			offset = -offset;
			length = -length;
			size = -size;
		case RIGHT:
			glBegin(GL_LINES);
			glVertex2s( offset, 0);
			glVertex2s( offset + length, 0);
			glVertex2s( offset + length, 0);
			glVertex2s( offset + length - size, -size);
			glVertex2s( offset + length, 0);
			glVertex2s( offset + length - size,  size);
			glEnd();
			break;
		case DOWN:
			offset = -offset;
			length = -length;
			size = -size;
		case UP:
			glBegin(GL_LINES);
			glVertex2s( 0, offset);
			glVertex2s( 0, offset + length);
			glVertex2s( 0, offset + length);
			glVertex2s(-size, offset + length - size);
			glVertex2s( 0, offset + length);
			glVertex2s( size, offset + length - size);
			glEnd();
			break;
	}
}

static void drawArrowHead(ArrowDirection d, short size)
{
	switch(d)
	{
		case LEFT:
			size = -size;
		case RIGHT:
			glBegin(GL_LINES);
			glVertex2s( 0, 0);
			glVertex2s( -size, -size);
			glVertex2s( 0, 0);
			glVertex2s( -size,  size);
			glEnd();
			break;
		case DOWN:
			size = -size;
		case UP:
			glBegin(GL_LINES);
			glVertex2s( 0, 0);
			glVertex2s(-size, -size);
			glVertex2s( 0, 0);
			glVertex2s( size, -size);
			glEnd();
			break;
	}
}

static void drawArc(float size, float angle_start, float angle_end, int segments)
{
	float delta = (angle_end - angle_start) / segments;
	float angle;

	glBegin(GL_LINE_STRIP);

	for ( angle = angle_start; angle < angle_end; angle += delta)
	{
		glVertex2f( cosf(angle) * size, sinf(angle) * size);
	}
	glVertex2f( cosf(angle_end) * size, sinf(angle_end) * size);

	glEnd();
}

static int helpline_poll(bContext *C)
{
	ARegion *ar= CTX_wm_region(C);
	
	if (ar && ar->regiontype==RGN_TYPE_WINDOW)
		return 1;
	return 0;
}

static void drawHelpline(bContext *UNUSED(C), int x, int y, void *customdata)
{
	TransInfo *t = (TransInfo*)customdata;

	if (t->helpline != HLP_NONE && !(t->flag & T_USES_MANIPULATOR))
	{
		float vecrot[3], cent[2];
		int mval[2];

		mval[0]= x;
		mval[1]= y;

		copy_v3_v3(vecrot, t->center);
		if (t->flag & T_EDIT) {
			Object *ob= t->obedit;
			if (ob) mul_m4_v3(ob->obmat, vecrot);
		}
		else if (t->flag & T_POSE) {
			Object *ob=t->poseobj;
			if (ob) mul_m4_v3(ob->obmat, vecrot);
		}

		projectFloatView(t, vecrot, cent);	// no overflow in extreme cases

		glPushMatrix();

		switch(t->helpline)
		{
			case HLP_SPRING:
				UI_ThemeColor(TH_WIRE);

				setlinestyle(3);
				glBegin(GL_LINE_STRIP);
				glVertex2iv(t->mval);
				glVertex2fv(cent);
				glEnd();

				glTranslatef(mval[0], mval[1], 0);
				glRotatef(-RAD2DEGF(atan2f(cent[0] - t->mval[0], cent[1] - t->mval[1])), 0, 0, 1);

				setlinestyle(0);
				glLineWidth(3.0);
				drawArrow(UP, 5, 10, 5);
				drawArrow(DOWN, 5, 10, 5);
				glLineWidth(1.0);
				break;
			case HLP_HARROW:
				UI_ThemeColor(TH_WIRE);

				glTranslatef(mval[0], mval[1], 0);

				glLineWidth(3.0);
				drawArrow(RIGHT, 5, 10, 5);
				drawArrow(LEFT, 5, 10, 5);
				glLineWidth(1.0);
				break;
			case HLP_VARROW:
				UI_ThemeColor(TH_WIRE);

				glTranslatef(mval[0], mval[1], 0);

				glLineWidth(3.0);
				glBegin(GL_LINES);
				drawArrow(UP, 5, 10, 5);
				drawArrow(DOWN, 5, 10, 5);
				glLineWidth(1.0);
				break;
			case HLP_ANGLE:
				{
					float dx = t->mval[0] - cent[0], dy = t->mval[1] - cent[1];
					float angle = atan2f(dy, dx);
					float dist = sqrtf(dx*dx + dy*dy);
					float delta_angle = MIN2(15.0f / dist, (float)M_PI/4.0f);
					float spacing_angle = MIN2(5.0f / dist, (float)M_PI/12.0f);
					UI_ThemeColor(TH_WIRE);

					setlinestyle(3);
					glBegin(GL_LINE_STRIP);
					glVertex2iv(t->mval);
					glVertex2fv(cent);
					glEnd();

					glTranslatef(cent[0] - t->mval[0] + mval[0], cent[1] - t->mval[1] + mval[1], 0);

					setlinestyle(0);
					glLineWidth(3.0);
					drawArc(dist, angle - delta_angle, angle - spacing_angle, 10);
					drawArc(dist, angle + spacing_angle, angle + delta_angle, 10);

					glPushMatrix();

					glTranslatef(cosf(angle - delta_angle) * dist, sinf(angle - delta_angle) * dist, 0);
					glRotatef(RAD2DEGF(angle - delta_angle), 0, 0, 1);

					drawArrowHead(DOWN, 5);

					glPopMatrix();

					glTranslatef(cosf(angle + delta_angle) * dist, sinf(angle + delta_angle) * dist, 0);
					glRotatef(RAD2DEGF(angle + delta_angle), 0, 0, 1);

					drawArrowHead(UP, 5);

					glLineWidth(1.0);
					break;
				}
				case HLP_TRACKBALL:
				{
					unsigned char col[3], col2[3];
					UI_GetThemeColor3ubv(TH_GRID, col);

					glTranslatef(mval[0], mval[1], 0);

					glLineWidth(3.0);

					UI_make_axis_color(col, col2, 'X');
					glColor3ubv((GLubyte *)col2);

					drawArrow(RIGHT, 5, 10, 5);
					drawArrow(LEFT, 5, 10, 5);

					UI_make_axis_color(col, col2, 'Y');
					glColor3ubv((GLubyte *)col2);

					drawArrow(UP, 5, 10, 5);
					drawArrow(DOWN, 5, 10, 5);
					glLineWidth(1.0);
					break;
				}
		}

		glPopMatrix();
	}
}

static void drawTransformView(const struct bContext *C, struct ARegion *UNUSED(ar), void *arg)
{
	TransInfo *t = arg;

	drawConstraint(t);
	drawPropCircle(C, t);
	drawSnapping(C, t);
}

#if 0
static void drawTransformPixel(const struct bContext *UNUSED(C), struct ARegion *UNUSED(ar), void *UNUSED(arg))
{
//	TransInfo *t = arg;
//
//	drawHelpline(C, t->mval[0], t->mval[1], t);
}
#endif

void saveTransform(bContext *C, TransInfo *t, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	int constraint_axis[3] = {0, 0, 0};
	int proportional = 0;
	PropertyRNA *prop;

	// Save back mode in case we're in the generic operator
	if ((prop= RNA_struct_find_property(op->ptr, "mode"))) {
		RNA_property_enum_set(op->ptr, prop, t->mode);
	}

	if ((prop= RNA_struct_find_property(op->ptr, "value"))) {
		float *values= (t->flag & T_AUTOVALUES) ? t->auto_values : t->values;
		if (RNA_property_array_check(prop)) {
			RNA_property_float_set_array(op->ptr, prop, values);
		}
		else {
			RNA_property_float_set(op->ptr, prop, values[0]);
		}
	}

	/* convert flag to enum */
	switch(t->flag & (T_PROP_EDIT|T_PROP_CONNECTED))
	{
	case (T_PROP_EDIT|T_PROP_CONNECTED):
		proportional = PROP_EDIT_CONNECTED;
		break;
	case T_PROP_EDIT:
		proportional = PROP_EDIT_ON;
		break;
	default:
		proportional = PROP_EDIT_OFF;
	}

	// If modal, save settings back in scene if not set as operator argument
	if (t->flag & T_MODAL) {

		/* save settings if not set in operator */
		if ( (prop = RNA_struct_find_property(op->ptr, "proportional")) && !RNA_property_is_set(op->ptr, prop))
		{
			if (t->obedit)
				ts->proportional = proportional;
			else
				ts->proportional_objects = (proportional != PROP_EDIT_OFF);
		}

		if ( (prop = RNA_struct_find_property(op->ptr, "proportional_size")) && !RNA_property_is_set(op->ptr, prop))
		{
			ts->proportional_size = t->prop_size;
		}

		if ( (prop = RNA_struct_find_property(op->ptr, "proportional_edit_falloff")) && !RNA_property_is_set(op->ptr, prop))
		{
			ts->prop_mode = t->prop_mode;
		}
		
		/* do we check for parameter? */
		if (t->modifiers & MOD_SNAP) {
			ts->snap_flag |= SCE_SNAP;
		}
		else {
			ts->snap_flag &= ~SCE_SNAP;
		}

		if (t->spacetype == SPACE_VIEW3D) {
			if ( (prop = RNA_struct_find_property(op->ptr, "constraint_orientation")) && !RNA_property_is_set(op->ptr, prop))
			{
				View3D *v3d = t->view;
	
				v3d->twmode = t->current_orientation;
			}
		}
	}
	
	if (RNA_struct_find_property(op->ptr, "proportional"))
	{
		RNA_enum_set(op->ptr, "proportional", proportional);
		RNA_enum_set(op->ptr, "proportional_edit_falloff", t->prop_mode);
		RNA_float_set(op->ptr, "proportional_size", t->prop_size);
	}

	if ((prop = RNA_struct_find_property(op->ptr, "axis")))
	{
		RNA_property_float_set_array(op->ptr, prop, t->axis);
	}

	if ((prop = RNA_struct_find_property(op->ptr, "mirror")))
	{
		RNA_property_boolean_set(op->ptr, prop, t->flag & T_MIRROR);
	}

	if ((prop = RNA_struct_find_property(op->ptr, "constraint_axis")))
	{
		/* constraint orientation can be global, event if user selects something else
		 * so use the orientation in the constraint if set
		 * */
		if (t->con.mode & CON_APPLY) {
			RNA_enum_set(op->ptr, "constraint_orientation", t->con.orientation);
		}
		else {
			RNA_enum_set(op->ptr, "constraint_orientation", t->current_orientation);
		}

		if (t->con.mode & CON_APPLY)
		{
			if (t->con.mode & CON_AXIS0) {
				constraint_axis[0] = 1;
			}
			if (t->con.mode & CON_AXIS1) {
				constraint_axis[1] = 1;
			}
			if (t->con.mode & CON_AXIS2) {
				constraint_axis[2] = 1;
			}
		}

		RNA_property_boolean_set_array(op->ptr, prop, constraint_axis);
	}
}

/* note: caller needs to free 't' on a 0 return */
int initTransform(bContext *C, TransInfo *t, wmOperator *op, wmEvent *event, int mode)
{
	int options = 0;
	PropertyRNA *prop;

	t->context = C;

	/* added initialize, for external calls to set stuff in TransInfo, like undo string */

	t->state = TRANS_STARTING;

	if ( (prop = RNA_struct_find_property(op->ptr, "texture_space")) && RNA_property_is_set(op->ptr, prop))
	{
		if (RNA_property_boolean_get(op->ptr, prop)) {
			options |= CTX_TEXTURE;
		}
	}
	
	t->options = options;

	t->mode = mode;

	t->launch_event = event ? event->type : -1;

	if (t->launch_event == EVT_TWEAK_R)
	{
		t->launch_event = RIGHTMOUSE;
	}
	else if (t->launch_event == EVT_TWEAK_L) {
		t->launch_event = LEFTMOUSE;
	}

	// XXX Remove this when wm_operator_call_internal doesn't use window->eventstate (which can have type = 0)
	// For manipulator only, so assume LEFTMOUSE
	if (t->launch_event == 0)
	{
		t->launch_event = LEFTMOUSE;
	}

	if (!initTransInfo(C, t, op, event))					// internal data, mouse, vectors
	{
		return 0;
	}

	if (t->spacetype == SPACE_VIEW3D)
	{
		//calc_manipulator_stats(curarea);
		initTransformOrientation(C, t);

		t->draw_handle_apply = ED_region_draw_cb_activate(t->ar->type, drawTransformApply, t, REGION_DRAW_PRE_VIEW);
		t->draw_handle_view = ED_region_draw_cb_activate(t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
		//t->draw_handle_pixel = ED_region_draw_cb_activate(t->ar->type, drawTransformPixel, t, REGION_DRAW_POST_PIXEL);
		t->draw_handle_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), helpline_poll, drawHelpline, t);
	}
	else if (t->spacetype == SPACE_IMAGE) {
		unit_m3(t->spacemtx);
		t->draw_handle_view = ED_region_draw_cb_activate(t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
		//t->draw_handle_pixel = ED_region_draw_cb_activate(t->ar->type, drawTransformPixel, t, REGION_DRAW_POST_PIXEL);
		t->draw_handle_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), helpline_poll, drawHelpline, t);
	}
	else if (t->spacetype == SPACE_CLIP) {
		unit_m3(t->spacemtx);
		t->draw_handle_view = ED_region_draw_cb_activate(t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
		t->options |= CTX_MOVIECLIP;
	}
	else
		unit_m3(t->spacemtx);

	createTransData(C, t);			// make TransData structs from selection

	if (t->total == 0) {
		postTrans(C, t);
		return 0;
	}

	/* Stupid code to have Ctrl-Click on manipulator work ok */
	if (event)
	{
		wmKeyMap *keymap = WM_keymap_active(CTX_wm_manager(C), op->type->modalkeymap);
		wmKeyMapItem *kmi;

		for (kmi = keymap->items.first; kmi; kmi = kmi->next)
		{
			if (kmi->propvalue == TFM_MODAL_SNAP_INV_ON && kmi->val == KM_PRESS)
			{
				if ((ELEM(kmi->type, LEFTCTRLKEY, RIGHTCTRLKEY) &&   event->ctrl)  ||
				    (ELEM(kmi->type, LEFTSHIFTKEY, RIGHTSHIFTKEY) && event->shift) ||
				    (ELEM(kmi->type, LEFTALTKEY, RIGHTALTKEY) &&     event->alt)   ||
				    ((kmi->type == OSKEY) &&                         event->oskey) )
				{
					t->modifiers |= MOD_SNAP_INVERT;
				}
				break;
			}
		}

	}

	initSnapping(t, op); // Initialize snapping data AFTER mode flags

	/* EVIL! posemode code can switch translation to rotate when 1 bone is selected. will be removed (ton) */
	/* EVIL2: we gave as argument also texture space context bit... was cleared */
	/* EVIL3: extend mode for animation editors also switches modes... but is best way to avoid duplicate code */
	mode = t->mode;

	calculatePropRatio(t);
	calculateCenter(t);

	initMouseInput(t, &t->mouse, t->center2d, t->imval);

	switch (mode) {
	case TFM_TRANSLATION:
		initTranslation(t);
		break;
	case TFM_ROTATION:
		initRotation(t);
		break;
	case TFM_RESIZE:
		initResize(t);
		break;
	case TFM_TOSPHERE:
		initToSphere(t);
		break;
	case TFM_SHEAR:
		initShear(t);
		break;
	case TFM_WARP:
		initWarp(t);
		break;
	case TFM_SHRINKFATTEN:
		initShrinkFatten(t);
		break;
	case TFM_TILT:
		initTilt(t);
		break;
	case TFM_CURVE_SHRINKFATTEN:
		initCurveShrinkFatten(t);
		break;
	case TFM_TRACKBALL:
		initTrackball(t);
		break;
	case TFM_PUSHPULL:
		initPushPull(t);
		break;
	case TFM_CREASE:
		initCrease(t);
		break;
	case TFM_BONESIZE:
		{	/* used for both B-Bone width (bonesize) as for deform-dist (envelope) */
			bArmature *arm= t->poseobj->data;
			if (arm->drawtype==ARM_ENVELOPE)
				initBoneEnvelope(t);
			else
				initBoneSize(t);
		}
		break;
	case TFM_BONE_ENVELOPE:
		initBoneEnvelope(t);
		break;
	case TFM_EDGE_SLIDE:
		initEdgeSlide(t);
		break;
	case TFM_BONE_ROLL:
		initBoneRoll(t);
		break;
	case TFM_TIME_TRANSLATE:
		initTimeTranslate(t);
		break;
	case TFM_TIME_SLIDE:
		initTimeSlide(t);
		break;
	case TFM_TIME_SCALE:
		initTimeScale(t);
		break;
	case TFM_TIME_DUPLICATE:
		/* same as TFM_TIME_EXTEND, but we need the mode info for later 
		 * so that duplicate-culling will work properly
		 */
		if (ELEM(t->spacetype, SPACE_IPO, SPACE_NLA))
			initTranslation(t);
		else
			initTimeTranslate(t);
		t->mode = mode;
		break;
	case TFM_TIME_EXTEND:
		/* now that transdata has been made, do like for TFM_TIME_TRANSLATE (for most Animation
		 * Editors because they have only 1D transforms for time values) or TFM_TRANSLATION
		 * (for Graph/NLA Editors only since they uses 'standard' transforms to get 2D movement)
		 * depending on which editor this was called from
		 */
		if (ELEM(t->spacetype, SPACE_IPO, SPACE_NLA))
			initTranslation(t);
		else
			initTimeTranslate(t);
		break;
	case TFM_BAKE_TIME:
		initBakeTime(t);
		break;
	case TFM_MIRROR:
		initMirror(t);
		break;
	case TFM_BEVEL:
		initBevel(t);
		break;
	case TFM_BWEIGHT:
		initBevelWeight(t);
		break;
	case TFM_ALIGN:
		initAlign(t);
		break;
	case TFM_SEQ_SLIDE:
		initSeqSlide(t);
		break;
	}

	if (t->state == TRANS_CANCEL)
	{
		postTrans(C, t);
		return 0;
	}


	/* overwrite initial values if operator supplied a non-null vector */
	if ( (prop = RNA_struct_find_property(op->ptr, "value")) && RNA_property_is_set(op->ptr, prop))
	{
		float values[4]= {0}; /* in case value isn't length 4, avoid uninitialized memory  */

		if (RNA_property_array_check(prop)) {
			RNA_float_get_array(op->ptr, "value", values);
		}
		else {
			values[0]= RNA_float_get(op->ptr, "value");
		}

		copy_v4_v4(t->values, values);
		copy_v4_v4(t->auto_values, values);
		t->flag |= T_AUTOVALUES;
	}

	/* Transformation axis from operator */
	if ((prop = RNA_struct_find_property(op->ptr, "axis")) && RNA_property_is_set(op->ptr, prop))
	{
		RNA_property_float_get_array(op->ptr, prop, t->axis);
		normalize_v3(t->axis);
		copy_v3_v3(t->axis_orig, t->axis);
	}

	/* Constraint init from operator */
	if ((prop = RNA_struct_find_property(op->ptr, "constraint_axis")) && RNA_property_is_set(op->ptr, prop))
	{
		int constraint_axis[3];

		RNA_property_boolean_get_array(op->ptr, prop, constraint_axis);

		if (constraint_axis[0] || constraint_axis[1] || constraint_axis[2])
		{
			t->con.mode |= CON_APPLY;

			if (constraint_axis[0]) {
				t->con.mode |= CON_AXIS0;
			}
			if (constraint_axis[1]) {
				t->con.mode |= CON_AXIS1;
			}
			if (constraint_axis[2]) {
				t->con.mode |= CON_AXIS2;
			}

			setUserConstraint(t, t->current_orientation, t->con.mode, "%s");
		}
	}

	t->context = NULL;

	return 1;
}

void transformApply(bContext *C, TransInfo *t)
{
	t->context = C;

	if ((t->redraw & TREDRAW_HARD) || (t->draw_handle_apply == NULL && (t->redraw & TREDRAW_SOFT)))
	{
		selectConstraint(t);
		if (t->transform) {
			t->transform(t, t->mval);  // calls recalcData()
			viewRedrawForce(C, t);
		}
		t->redraw = TREDRAW_NOTHING;
	}
	else if (t->redraw & TREDRAW_SOFT) {
		viewRedrawForce(C, t);
	}

	/* If auto confirm is on, break after one pass */
	if (t->options & CTX_AUTOCONFIRM)
	{
		t->state = TRANS_CONFIRM;
	}

	if (BKE_ptcache_get_continue_physics())
	{
		// TRANSFORM_FIX_ME
		//do_screenhandlers(G.curscreen);
		t->redraw |= TREDRAW_HARD;
	}

	t->context = NULL;
}

static void drawTransformApply(const bContext *C, struct ARegion *UNUSED(ar), void *arg)
{
	TransInfo *t = arg;

	if (t->redraw & TREDRAW_SOFT) {
		t->redraw |= TREDRAW_HARD;
		transformApply((bContext *)C, t);
	}
}

int transformEnd(bContext *C, TransInfo *t)
{
	int exit_code = OPERATOR_RUNNING_MODAL;

	t->context = C;

	if (t->state != TRANS_STARTING && t->state != TRANS_RUNNING)
	{
		/* handle restoring objects */
		if (t->state == TRANS_CANCEL)
		{
			/* exception, edge slide transformed UVs too */
			if (t->mode==TFM_EDGE_SLIDE)
				doEdgeSlide(t, 0.0f);
			
			exit_code = OPERATOR_CANCELLED;
			restoreTransObjects(t);	// calls recalcData()
		}
		else {
			exit_code = OPERATOR_FINISHED;
		}

		/* aftertrans does insert keyframes, and clears base flags, doesnt read transdata */
		special_aftertrans_update(C, t);

		/* free data */
		postTrans(C, t);

		/* send events out for redraws */
		viewRedrawPost(C, t);

		/*  Undo as last, certainly after special_trans_update! */

		if (t->state == TRANS_CANCEL) {
//			if (t->undostr) ED_undo_push(C, t->undostr);
		}
		else {
//			if (t->undostr) ED_undo_push(C, t->undostr);
//			else ED_undo_push(C, transform_to_undostr(t));
		}
		t->undostr= NULL;

		viewRedrawForce(C, t);
	}

	t->context = NULL;

	return exit_code;
}

/* ************************** TRANSFORM LOCKS **************************** */

static void protectedTransBits(short protectflag, float *vec)
{
	if (protectflag & OB_LOCK_LOCX)
		vec[0]= 0.0f;
	if (protectflag & OB_LOCK_LOCY)
		vec[1]= 0.0f;
	if (protectflag & OB_LOCK_LOCZ)
		vec[2]= 0.0f;
}

static void protectedSizeBits(short protectflag, float *size)
{
	if (protectflag & OB_LOCK_SCALEX)
		size[0]= 1.0f;
	if (protectflag & OB_LOCK_SCALEY)
		size[1]= 1.0f;
	if (protectflag & OB_LOCK_SCALEZ)
		size[2]= 1.0f;
}

static void protectedRotateBits(short protectflag, float *eul, float *oldeul)
{
	if (protectflag & OB_LOCK_ROTX)
		eul[0]= oldeul[0];
	if (protectflag & OB_LOCK_ROTY)
		eul[1]= oldeul[1];
	if (protectflag & OB_LOCK_ROTZ)
		eul[2]= oldeul[2];
}


/* this function only does the delta rotation */
/* axis-angle is usually internally stored as quats... */
static void protectedAxisAngleBits(short protectflag, float axis[3], float *angle, float oldAxis[3], float oldAngle)
{
	/* check that protection flags are set */
	if ((protectflag & (OB_LOCK_ROTX|OB_LOCK_ROTY|OB_LOCK_ROTZ|OB_LOCK_ROTW)) == 0)
		return;
	
	if (protectflag & OB_LOCK_ROT4D) {
		/* axis-angle getting limited as 4D entities that they are... */
		if (protectflag & OB_LOCK_ROTW)
			*angle= oldAngle;
		if (protectflag & OB_LOCK_ROTX)
			axis[0]= oldAxis[0];
		if (protectflag & OB_LOCK_ROTY)
			axis[1]= oldAxis[1];
		if (protectflag & OB_LOCK_ROTZ)
			axis[2]= oldAxis[2];
	}
	else {
		/* axis-angle get limited with euler... */
		float eul[3], oldeul[3];
		
		axis_angle_to_eulO( eul, EULER_ORDER_DEFAULT,axis, *angle);
		axis_angle_to_eulO( oldeul, EULER_ORDER_DEFAULT,oldAxis, oldAngle);
		
		if (protectflag & OB_LOCK_ROTX)
			eul[0]= oldeul[0];
		if (protectflag & OB_LOCK_ROTY)
			eul[1]= oldeul[1];
		if (protectflag & OB_LOCK_ROTZ)
			eul[2]= oldeul[2];
		
		eulO_to_axis_angle( axis, angle,eul, EULER_ORDER_DEFAULT);
		
		/* when converting to axis-angle, we need a special exception for the case when there is no axis */
		if (IS_EQF(axis[0], axis[1]) && IS_EQF(axis[1], axis[2])) {
			/* for now, rotate around y-axis then (so that it simply becomes the roll) */
			axis[1]= 1.0f;
		}
	}
}

/* this function only does the delta rotation */
static void protectedQuaternionBits(short protectflag, float *quat, float *oldquat)
{
	/* check that protection flags are set */
	if ((protectflag & (OB_LOCK_ROTX|OB_LOCK_ROTY|OB_LOCK_ROTZ|OB_LOCK_ROTW)) == 0)
		return;
	
	if (protectflag & OB_LOCK_ROT4D) {
		/* quaternions getting limited as 4D entities that they are... */
		if (protectflag & OB_LOCK_ROTW)
			quat[0]= oldquat[0];
		if (protectflag & OB_LOCK_ROTX)
			quat[1]= oldquat[1];
		if (protectflag & OB_LOCK_ROTY)
			quat[2]= oldquat[2];
		if (protectflag & OB_LOCK_ROTZ)
			quat[3]= oldquat[3];
	}
	else {
		/* quaternions get limited with euler... (compatibility mode) */
		float eul[3], oldeul[3], nquat[4], noldquat[4];
		float qlen;

		qlen= normalize_qt_qt(nquat, quat);
		normalize_qt_qt(noldquat, oldquat);

		quat_to_eul(eul, nquat);
		quat_to_eul(oldeul, noldquat);

		if (protectflag & OB_LOCK_ROTX)
			eul[0]= oldeul[0];
		if (protectflag & OB_LOCK_ROTY)
			eul[1]= oldeul[1];
		if (protectflag & OB_LOCK_ROTZ)
			eul[2]= oldeul[2];

		eul_to_quat( quat,eul);

		/* restore original quat size */
		mul_qt_fl(quat, qlen);
		
		/* quaternions flip w sign to accumulate rotations correctly */
		if ( (nquat[0]<0.0f && quat[0]>0.0f) || (nquat[0]>0.0f && quat[0]<0.0f) ) {
			mul_qt_fl(quat, -1.0f);
		}
	}
}

/* ******************* TRANSFORM LIMITS ********************** */

static void constraintTransLim(TransInfo *t, TransData *td)
{
	if (td->con) {
		bConstraintTypeInfo *ctiLoc= get_constraint_typeinfo(CONSTRAINT_TYPE_LOCLIMIT);
		bConstraintTypeInfo *ctiDist= get_constraint_typeinfo(CONSTRAINT_TYPE_DISTLIMIT);
		
		bConstraintOb cob= {NULL};
		bConstraint *con;
		float ctime = (float)(t->scene->r.cfra);
		
		/* Make a temporary bConstraintOb for using these limit constraints
		 * 	- they only care that cob->matrix is correctly set ;-)
		 *	- current space should be local
		 */
		unit_m4(cob.matrix);
		copy_v3_v3(cob.matrix[3], td->loc);
		
		/* Evaluate valid constraints */
		for (con= td->con; con; con= con->next) {
			bConstraintTypeInfo *cti = NULL;
			ListBase targets = {NULL, NULL};
			float tmat[4][4];
			
			/* only consider constraint if enabled */
			if (con->flag & CONSTRAINT_DISABLE) continue;
			if (con->enforce == 0.0f) continue;
			
			/* only use it if it's tagged for this purpose (and the right type) */
			if (con->type == CONSTRAINT_TYPE_LOCLIMIT) {
				bLocLimitConstraint *data= con->data;
				
				if ((data->flag2 & LIMIT_TRANSFORM)==0)
					continue;
				cti = ctiLoc;
			}
			else if (con->type == CONSTRAINT_TYPE_DISTLIMIT) {
				bDistLimitConstraint *data= con->data;
				
				if ((data->flag & LIMITDIST_TRANSFORM)==0)
					continue;
				cti = ctiDist;
			}
			
			if (cti) {
				/* do space conversions */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->mtx (this should be ok) */
					copy_m4_m4(tmat, cob.matrix);
					mul_m4_m3m4(cob.matrix, td->mtx, tmat);
				}
				else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
					/* skip... incompatable spacetype */
					continue;
				}
				
				/* get constraint targets if needed */
				get_constraint_targets_for_solving(con, &cob, &targets, ctime);
				
				/* do constraint */
				cti->evaluate_constraint(con, &cob, &targets);
				
				/* convert spaces again */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->mtx (this should be ok) */
					copy_m4_m4(tmat, cob.matrix);
					mul_m4_m3m4(cob.matrix, td->smtx, tmat);
				}
				
				/* free targets list */
				BLI_freelistN(&targets);
			}
		}
		
		/* copy results from cob->matrix */
		copy_v3_v3(td->loc, cob.matrix[3]);
	}
}

static void constraintob_from_transdata(bConstraintOb *cob, TransData *td)
{
	/* Make a temporary bConstraintOb for use by limit constraints
	 * 	- they only care that cob->matrix is correctly set ;-)
	 *	- current space should be local
	 */
	memset(cob, 0, sizeof(bConstraintOb));
	if (td->ext)
	{
		if (td->ext->rotOrder == ROT_MODE_QUAT) {
			/* quats */
			/* objects and bones do normalization first too, otherwise
			 * we don't necessarily end up with a rotation matrix, and
			 * then conversion back to quat gives a different result */
			float quat[4];
			normalize_qt_qt(quat, td->ext->quat);
			quat_to_mat4(cob->matrix, quat);
		}
		else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
			/* axis angle */
			axis_angle_to_mat4(cob->matrix, &td->ext->quat[1], td->ext->quat[0]);
		}
		else {
			/* eulers */
			eulO_to_mat4(cob->matrix, td->ext->rot, td->ext->rotOrder);
		}
	}
}

static void constraintRotLim(TransInfo *UNUSED(t), TransData *td)
{
	if (td->con) {
		bConstraintTypeInfo *cti= get_constraint_typeinfo(CONSTRAINT_TYPE_ROTLIMIT);
		bConstraintOb cob;
		bConstraint *con;
		int dolimit = 0;
		
		/* Evaluate valid constraints */
		for (con= td->con; con; con= con->next) {
			/* only consider constraint if enabled */
			if (con->flag & CONSTRAINT_DISABLE) continue;
			if (con->enforce == 0.0f) continue;
			
			/* we're only interested in Limit-Rotation constraints */
			if (con->type == CONSTRAINT_TYPE_ROTLIMIT) {
				bRotLimitConstraint *data= con->data;
				float tmat[4][4];
				
				/* only use it if it's tagged for this purpose */
				if ((data->flag2 & LIMIT_TRANSFORM)==0)
					continue;

				/* skip incompatable spacetypes */
				if (!ELEM(con->ownspace, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL))
					continue;

				/* only do conversion if necessary, to preserve quats and eulers */
				if (!dolimit) {
					constraintob_from_transdata(&cob, td);
					dolimit= 1;
				}
				
				/* do space conversions */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->mtx (this should be ok) */
					copy_m4_m4(tmat, cob.matrix);
					mul_m4_m3m4(cob.matrix, td->mtx, tmat);
				}
				
				/* do constraint */
				cti->evaluate_constraint(con, &cob, NULL);
				
				/* convert spaces again */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->mtx (this should be ok) */
					copy_m4_m4(tmat, cob.matrix);
					mul_m4_m3m4(cob.matrix, td->smtx, tmat);
				}
			}
		}
		
		if (dolimit) {
			/* copy results from cob->matrix */
			if (td->ext->rotOrder == ROT_MODE_QUAT) {
				/* quats */
				mat4_to_quat( td->ext->quat,cob.matrix);
			}
			else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
				/* axis angle */
				mat4_to_axis_angle( &td->ext->quat[1], &td->ext->quat[0],cob.matrix);
			}
			else {
				/* eulers */
				mat4_to_eulO( td->ext->rot, td->ext->rotOrder,cob.matrix);
			}
		}
	}
}

static void constraintSizeLim(TransInfo *t, TransData *td)
{
	if (td->con && td->ext) {
		bConstraintTypeInfo *cti= get_constraint_typeinfo(CONSTRAINT_TYPE_SIZELIMIT);
		bConstraintOb cob= {NULL};
		bConstraint *con;
		
		/* Make a temporary bConstraintOb for using these limit constraints
		 * 	- they only care that cob->matrix is correctly set ;-)
		 *	- current space should be local
		 */
		if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
			/* scale val and reset size */
			return; // TODO: fix this case
		}
		else {
			/* Reset val if SINGLESIZE but using a constraint */
			if (td->flag & TD_SINGLESIZE)
				return;
			
			size_to_mat4( cob.matrix,td->ext->size);
		}
		
		/* Evaluate valid constraints */
		for (con= td->con; con; con= con->next) {
			/* only consider constraint if enabled */
			if (con->flag & CONSTRAINT_DISABLE) continue;
			if (con->enforce == 0.0f) continue;
			
			/* we're only interested in Limit-Scale constraints */
			if (con->type == CONSTRAINT_TYPE_SIZELIMIT) {
				bSizeLimitConstraint *data= con->data;
				float tmat[4][4];
				
				/* only use it if it's tagged for this purpose */
				if ((data->flag2 & LIMIT_TRANSFORM)==0)
					continue;
				
				/* do space conversions */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->mtx (this should be ok) */
					copy_m4_m4(tmat, cob.matrix);
					mul_m4_m3m4(cob.matrix, td->mtx, tmat);
				}
				else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
					/* skip... incompatable spacetype */
					continue;
				}
				
				/* do constraint */
				cti->evaluate_constraint(con, &cob, NULL);
				
				/* convert spaces again */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->mtx (this should be ok) */
					copy_m4_m4(tmat, cob.matrix);
					mul_m4_m3m4(cob.matrix, td->smtx, tmat);
				}
			}
		}
		
		/* copy results from cob->matrix */
		if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
			/* scale val and reset size */
			return; // TODO: fix this case
		}
		else {
			/* Reset val if SINGLESIZE but using a constraint */
			if (td->flag & TD_SINGLESIZE)
				return;
			
			mat4_to_size( td->ext->size,cob.matrix);
		}
	}
}

/* ************************** WARP *************************** */

static void postInputWarp(TransInfo *t, float values[3])
{
	mul_v3_fl(values, (float)(M_PI * 2));

	if (t->customData) /* non-null value indicates reversed input */
	{
		negate_v3(values);
	}
}

void initWarp(TransInfo *t)
{
	float max[3], min[3];
	int i;
	
	t->mode = TFM_WARP;
	t->transform = Warp;
	t->handleEvent = handleEventWarp;
	
	setInputPostFct(&t->mouse, postInputWarp);
	initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_RATIO);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 5.0f / 180.0f * (float)M_PI;
	t->snap[2] = 1.0f / 180.0f * (float)M_PI;
	
	t->num.increment = 1.0f;

	t->flag |= T_NO_CONSTRAINT;
	
	/* we need min/max in view space */
	for (i = 0; i < t->total; i++) {
		float center[3];
		copy_v3_v3(center, t->data[i].center);
		mul_m3_v3(t->data[i].mtx, center);
		mul_m4_v3(t->viewmat, center);
		sub_v3_v3(center, t->viewmat[3]);
		if (i)
			minmax_v3v3_v3(min, max, center);
		else {
			copy_v3_v3(max, center);
			copy_v3_v3(min, center);
		}
	}

	mid_v3_v3v3(t->center, min, max);

	if (max[0] == min[0]) max[0] += 0.1f; /* not optimal, but flipping is better than invalid garbage (i.e. division by zero!) */
	t->val= (max[0]-min[0])/2.0f; /* t->val is X dimension projected boundbox */
}

int handleEventWarp(TransInfo *t, wmEvent *event)
{
	int status = 0;
	
	if (event->type == MIDDLEMOUSE && event->val==KM_PRESS)
	{
		// Use customData pointer to signal warp direction
		if	(t->customData == NULL)
			t->customData = (void*)1;
		else
			t->customData = NULL;
		
		status = 1;
	}
	
	return status;
}

int Warp(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float vec[3], circumfac, dist, phi0, co, si, *curs, cursor[3], gcursor[3];
	int i;
	char str[50];
	
	curs= give_cursor(t->scene, t->view);
	/*
	 * gcursor is the one used for helpline.
	 * It has to be in the same space as the drawing loop
	 * (that means it needs to be in the object's space when in edit mode and
	 *  in global space in object mode)
	 *
	 * cursor is used for calculations.
	 * It needs to be in view space, but we need to take object's offset
	 * into account if in Edit mode.
	 */
	copy_v3_v3(cursor, curs);
	copy_v3_v3(gcursor, cursor);
	if (t->flag & T_EDIT) {
		sub_v3_v3(cursor, t->obedit->obmat[3]);
		sub_v3_v3(gcursor, t->obedit->obmat[3]);
		mul_m3_v3(t->data->smtx, gcursor);
	}
	mul_m4_v3(t->viewmat, cursor);
	sub_v3_v3(cursor, t->viewmat[3]);
	
	/* amount of radians for warp */
	circumfac = t->values[0];
	
	snapGrid(t, &circumfac);
	applyNumInput(&t->num, &circumfac);
	
	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];
		
		outputNumInput(&(t->num), c);
		
		sprintf(str, "Warp: %s", c);

		circumfac = DEG2RADF(circumfac);
	}
	else {
		/* default header print */
		sprintf(str, "Warp: %.3f", RAD2DEGF(circumfac));
	}
	
	t->values[0] = circumfac;

	circumfac /= 2; /* only need 180 on each side to make 360 */
	
	for (i = 0; i < t->total; i++, td++) {
		float loc[3];
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		/* translate point to center, rotate in such a way that outline==distance */
		copy_v3_v3(vec, td->iloc);
		mul_m3_v3(td->mtx, vec);
		mul_m4_v3(t->viewmat, vec);
		sub_v3_v3(vec, t->viewmat[3]);
		
		dist= vec[0]-cursor[0];
		
		/* t->val is X dimension projected boundbox */
		phi0= (circumfac*dist/t->val);
		
		vec[1]= (vec[1]-cursor[1]);
		
		co= (float)cos(phi0);
		si= (float)sin(phi0);
		loc[0]= -si*vec[1]+cursor[0];
		loc[1]= co*vec[1]+cursor[1];
		loc[2]= vec[2];
		
		mul_m4_v3(t->viewinv, loc);
		sub_v3_v3(loc, t->viewinv[3]);
		mul_m3_v3(td->smtx, loc);
		
		sub_v3_v3(loc, td->iloc);
		mul_v3_fl(loc, td->factor);
		add_v3_v3v3(td->loc, td->iloc, loc);
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
	
	return 1;
}

/* ************************** SHEAR *************************** */

static void postInputShear(TransInfo *UNUSED(t), float values[3])
{
	mul_v3_fl(values, 0.05f);
}

void initShear(TransInfo *t)
{
	t->mode = TFM_SHEAR;
	t->transform = Shear;
	t->handleEvent = handleEventShear;
	
	setInputPostFct(&t->mouse, postInputShear);
	initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_ABSOLUTE);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	
	t->num.increment = 0.1f;

	t->flag |= T_NO_CONSTRAINT;
}

int handleEventShear(TransInfo *t, wmEvent *event)
{
	int status = 0;
	
	if (event->type == MIDDLEMOUSE && event->val==KM_PRESS)
	{
		// Use customData pointer to signal Shear direction
		if	(t->customData == NULL)
		{
			initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);
			t->customData = (void*)1;
		}
		else {
			initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_ABSOLUTE);
			t->customData = NULL;
		}

		status = 1;
	}
	
	return status;
}


int Shear(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float vec[3];
	float smat[3][3], tmat[3][3], totmat[3][3], persmat[3][3], persinv[3][3];
	float value;
	int i;
	char str[50];
	
	copy_m3_m4(persmat, t->viewmat);
	invert_m3_m3(persinv, persmat);
	
	value = t->values[0];
	
	snapGrid(t, &value);
	
	applyNumInput(&t->num, &value);
	
	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];
		
		outputNumInput(&(t->num), c);
		
		sprintf(str, "Shear: %s %s", c, t->proptext);
	}
	else {
		/* default header print */
		sprintf(str, "Shear: %.3f %s", value, t->proptext);
	}
	
	unit_m3(smat);
	
	// Custom data signals shear direction
	if (t->customData == NULL)
		smat[1][0] = value;
	else
		smat[0][1] = value;
	
	mul_m3_m3m3(tmat, smat, persmat);
	mul_m3_m3m3(totmat, persinv, tmat);
	
	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		if (t->obedit) {
			float mat3[3][3];
			mul_m3_m3m3(mat3, totmat, td->mtx);
			mul_m3_m3m3(tmat, td->smtx, mat3);
		}
		else {
			copy_m3_m3(tmat, totmat);
		}
		sub_v3_v3v3(vec, td->center, t->center);
		
		mul_m3_v3(tmat, vec);
		
		add_v3_v3(vec, t->center);
		sub_v3_v3(vec, td->center);
		
		mul_v3_fl(vec, td->factor);
		
		add_v3_v3v3(td->loc, td->iloc, vec);
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** RESIZE *************************** */

void initResize(TransInfo *t)
{
	t->mode = TFM_RESIZE;
	t->transform = Resize;
	
	initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);
	
	t->flag |= T_NULL_ONE;
	t->num.flag |= NUM_NULL_ONE;
	t->num.flag |= NUM_AFFECT_ALL;
	if (!t->obedit) {
		t->flag |= T_NO_ZERO;
		t->num.flag |= NUM_NO_ZERO;
	}
	
	t->idx_max = 2;
	t->num.idx_max = 2;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	t->num.increment = t->snap[1];
}

static void headerResize(TransInfo *t, float vec[3], char *str)
{
	char tvec[60];
	char *spos= str;
	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec);
	}
	else {
		BLI_snprintf(&tvec[0],  20, "%.4f", vec[0]);
		BLI_snprintf(&tvec[20], 20, "%.4f", vec[1]);
		BLI_snprintf(&tvec[40], 20, "%.4f", vec[2]);
	}
	
	if (t->con.mode & CON_APPLY) {
		switch(t->num.idx_max) {
		case 0:
			spos += sprintf(spos, "Scale: %s%s %s", &tvec[0], t->con.text, t->proptext);
			break;
		case 1:
			spos += sprintf(spos, "Scale: %s : %s%s %s", &tvec[0], &tvec[20], t->con.text, t->proptext);
			break;
		case 2:
			spos += sprintf(spos, "Scale: %s : %s : %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
		}
	}
	else {
		if (t->flag & T_2D_EDIT)
			spos += sprintf(spos, "Scale X: %s   Y: %s%s %s", &tvec[0], &tvec[20], t->con.text, t->proptext);
		else
			spos += sprintf(spos, "Scale X: %s   Y: %s  Z: %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
	}
	
	if (t->flag & (T_PROP_EDIT|T_PROP_CONNECTED)) {
		spos += sprintf(spos, " Proportional size: %.2f", t->prop_size);
	}

	(void)spos;
}

#define SIGN(a)		(a<-FLT_EPSILON?1:a>FLT_EPSILON?2:3)
#define VECSIGNFLIP(a, b) ((SIGN(a[0]) & SIGN(b[0]))==0 || (SIGN(a[1]) & SIGN(b[1]))==0 || (SIGN(a[2]) & SIGN(b[2]))==0)

/* smat is reference matrix, only scaled */
static void TransMat3ToSize( float mat[][3], float smat[][3], float *size)
{
	float vec[3];
	
	copy_v3_v3(vec, mat[0]);
	size[0]= normalize_v3(vec);
	copy_v3_v3(vec, mat[1]);
	size[1]= normalize_v3(vec);
	copy_v3_v3(vec, mat[2]);
	size[2]= normalize_v3(vec);
	
	/* first tried with dotproduct... but the sign flip is crucial */
	if ( VECSIGNFLIP(mat[0], smat[0]) ) size[0]= -size[0];
	if ( VECSIGNFLIP(mat[1], smat[1]) ) size[1]= -size[1];
	if ( VECSIGNFLIP(mat[2], smat[2]) ) size[2]= -size[2];
}


static void ElementResize(TransInfo *t, TransData *td, float mat[3][3])
{
	float tmat[3][3], smat[3][3], center[3];
	float vec[3];
	
	if (t->flag & T_EDIT) {
		mul_m3_m3m3(smat, mat, td->mtx);
		mul_m3_m3m3(tmat, td->smtx, smat);
	}
	else {
		copy_m3_m3(tmat, mat);
	}
	
	if (t->con.applySize) {
		t->con.applySize(t, td, tmat);
	}
	
	/* local constraint shouldn't alter center */
	if ((t->around == V3D_LOCAL) &&
	        (   (t->flag & (T_OBJECT|T_POSE)) ||
	            ((t->flag & T_EDIT) && (t->settings->selectmode & (SCE_SELECT_EDGE|SCE_SELECT_FACE))) ||
	            (t->obedit && t->obedit->type == OB_ARMATURE))
	        )
	{
		copy_v3_v3(center, td->center);
	}
	else {
		copy_v3_v3(center, t->center);
	}

	if (td->ext) {
		float fsize[3];
		
		if (t->flag & (T_OBJECT|T_TEXTURE|T_POSE)) {
			float obsizemat[3][3];
			// Reorient the size mat to fit the oriented object.
			mul_m3_m3m3(obsizemat, tmat, td->axismtx);
			//print_m3("obsizemat", obsizemat);
			TransMat3ToSize(obsizemat, td->axismtx, fsize);
			//print_v3("fsize", fsize);
		}
		else {
			mat3_to_size( fsize,tmat);
		}
		
		protectedSizeBits(td->protectflag, fsize);
		
		if ((t->flag & T_V3D_ALIGN)==0) {	// align mode doesn't resize objects itself
			if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
				/* scale val and reset size */
				 *td->val = td->ival * (1 + (fsize[0] - 1) * td->factor);
				
				td->ext->size[0] = td->ext->isize[0];
				td->ext->size[1] = td->ext->isize[1];
				td->ext->size[2] = td->ext->isize[2];
			 }
			else {
				/* Reset val if SINGLESIZE but using a constraint */
				if (td->flag & TD_SINGLESIZE)
					 *td->val = td->ival;
				
				td->ext->size[0] = td->ext->isize[0] * (1 + (fsize[0] - 1) * td->factor);
				td->ext->size[1] = td->ext->isize[1] * (1 + (fsize[1] - 1) * td->factor);
				td->ext->size[2] = td->ext->isize[2] * (1 + (fsize[2] - 1) * td->factor);
			}
		}
		
		constraintSizeLim(t, td);
	}
	
	/* For individual element center, Editmode need to use iloc */
	if (t->flag & T_POINTS)
		sub_v3_v3v3(vec, td->iloc, center);
	else
		sub_v3_v3v3(vec, td->center, center);
	
	mul_m3_v3(tmat, vec);
	
	add_v3_v3(vec, center);
	if (t->flag & T_POINTS)
		sub_v3_v3(vec, td->iloc);
	else
		sub_v3_v3(vec, td->center);
	
	mul_v3_fl(vec, td->factor);
	
	if (t->flag & (T_OBJECT|T_POSE)) {
		mul_m3_v3(td->smtx, vec);
	}
	
	protectedTransBits(td->protectflag, vec);
	add_v3_v3v3(td->loc, td->iloc, vec);
	
	constraintTransLim(t, td);
}

int Resize(TransInfo *t, const int mval[2])
{
	TransData *td;
	float size[3], mat[3][3];
	float ratio;
	int i;
	char str[200];
	
	/* for manipulator, center handle, the scaling can't be done relative to center */
	if ( (t->flag & T_USES_MANIPULATOR) && t->con.mode==0)
	{
		ratio = 1.0f - ((t->imval[0] - mval[0]) + (t->imval[1] - mval[1]))/100.0f;
	}
	else {
		ratio = t->values[0];
	}
	
	size[0] = size[1] = size[2] = ratio;
	
	snapGrid(t, size);
	
	if (hasNumInput(&t->num)) {
		applyNumInput(&t->num, size);
		constraintNumInput(t, size);
	}
	
	applySnapping(t, size);
	
	if (t->flag & T_AUTOVALUES)
	{
		copy_v3_v3(size, t->auto_values);
	}
	
	copy_v3_v3(t->values, size);
	
	size_to_mat3( mat,size);
	
	if (t->con.applySize) {
		t->con.applySize(t, NULL, mat);
	}
	
	copy_m3_m3(t->mat, mat);	// used in manipulator
	
	headerResize(t, size, str);
	
	for (i = 0, td=t->data; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		ElementResize(t, td, mat);
	}
	
	/* evil hack - redo resize if cliping needed */
	if (t->flag & T_CLIP_UV && clipUVTransform(t, size, 1)) {
		size_to_mat3( mat,size);
		
		if (t->con.applySize)
			t->con.applySize(t, NULL, mat);
		
		for (i = 0, td=t->data; i < t->total; i++, td++)
			ElementResize(t, td, mat);
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
	
	return 1;
}

/* ************************** TOSPHERE *************************** */

void initToSphere(TransInfo *t)
{
	TransData *td = t->data;
	int i;
	
	t->mode = TFM_TOSPHERE;
	t->transform = ToSphere;
	
	initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_RATIO);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	
	t->num.increment = t->snap[1];

	t->num.flag |= NUM_NULL_ONE | NUM_NO_NEGATIVE;
	t->flag |= T_NO_CONSTRAINT;
	
	// Calculate average radius
	for (i = 0 ; i < t->total; i++, td++) {
		t->val += len_v3v3(t->center, td->iloc);
	}
	
	t->val /= (float)t->total;
}

int ToSphere(TransInfo *t, const int UNUSED(mval[2]))
{
	float vec[3];
	float ratio, radius;
	int i;
	char str[64];
	TransData *td = t->data;
	
	ratio = t->values[0];
	
	snapGrid(t, &ratio);
	
	applyNumInput(&t->num, &ratio);
	
	if (ratio < 0)
		ratio = 0.0f;
	else if (ratio > 1)
		ratio = 1.0f;
	
	t->values[0] = ratio;

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];
		
		outputNumInput(&(t->num), c);
		
		sprintf(str, "To Sphere: %s %s", c, t->proptext);
	}
	else {
		/* default header print */
		sprintf(str, "To Sphere: %.4f %s", ratio, t->proptext);
	}
	
	
	for (i = 0 ; i < t->total; i++, td++) {
		float tratio;
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		sub_v3_v3v3(vec, td->iloc, t->center);
		
		radius = normalize_v3(vec);
		
		tratio = ratio * td->factor;
		
		mul_v3_fl(vec, radius * (1.0f - tratio) + t->val * tratio);
		
		add_v3_v3v3(td->loc, t->center, vec);
	}
	
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
	
	return 1;
}

/* ************************** ROTATION *************************** */


static void postInputRotation(TransInfo *t, float values[3])
{
	if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
		t->con.applyRot(t, NULL, t->axis, values);
	}
}

void initRotation(TransInfo *t)
{
	t->mode = TFM_ROTATION;
	t->transform = Rotation;
	
	setInputPostFct(&t->mouse, postInputRotation);
	initMouseInputMode(t, &t->mouse, INPUT_ANGLE);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;
	
	t->num.increment = 1.0f;

	if (t->flag & T_2D_EDIT)
		t->flag |= T_NO_CONSTRAINT;

	negate_v3_v3(t->axis, t->viewinv[2]);
	normalize_v3(t->axis);

	copy_v3_v3(t->axis_orig, t->axis);
}

static void ElementRotation(TransInfo *t, TransData *td, float mat[3][3], short around)
{
	float vec[3], totmat[3][3], smat[3][3];
	float eul[3], fmat[3][3], quat[4];
	float *center = t->center;

	/* local constraint shouldn't alter center */
	if (around == V3D_LOCAL) {
		if (    (t->flag & (T_OBJECT|T_POSE)) ||
	            (t->settings->selectmode & (SCE_SELECT_EDGE|SCE_SELECT_FACE)) ||
		        (t->obedit && t->obedit->type == OB_ARMATURE))
		{
			center = td->center;
		}
	}

	if (t->flag & T_POINTS) {
		mul_m3_m3m3(totmat, mat, td->mtx);
		mul_m3_m3m3(smat, td->smtx, totmat);
		
		sub_v3_v3v3(vec, td->iloc, center);
		mul_m3_v3(smat, vec);
		
		add_v3_v3v3(td->loc, vec, center);
		
		sub_v3_v3v3(vec,td->loc,td->iloc);
		protectedTransBits(td->protectflag, vec);
		add_v3_v3v3(td->loc, td->iloc, vec);
		
		
		if (td->flag & TD_USEQUAT) {
			mul_serie_m3(fmat, td->mtx, mat, td->smtx, NULL, NULL, NULL, NULL, NULL);
			mat3_to_quat( quat,fmat);	// Actual transform
			
			if (td->ext->quat) {
				mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
				
				/* is there a reason not to have this here? -jahka */
				protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
			}
		}
	}
	/**
	 * HACK WARNING
	 *
	 * This is some VERY ugly special case to deal with pose mode.
	 *
	 * The problem is that mtx and smtx include each bone orientation.
	 *
	 * That is needed to rotate each bone properly, HOWEVER, to calculate
	 * the translation component, we only need the actual armature object's
	 * matrix (and inverse). That is not all though. Once the proper translation
	 * has been computed, it has to be converted back into the bone's space.
	 */
	else if (t->flag & T_POSE) {
		float pmtx[3][3], imtx[3][3];
		
		// Extract and invert armature object matrix
		copy_m3_m4(pmtx, t->poseobj->obmat);
		invert_m3_m3(imtx, pmtx);
		
		if ((td->flag & TD_NO_LOC) == 0)
		{
			sub_v3_v3v3(vec, td->center, center);
			
			mul_m3_v3(pmtx, vec);	// To Global space
			mul_m3_v3(mat, vec);		// Applying rotation
			mul_m3_v3(imtx, vec);	// To Local space
			
			add_v3_v3(vec, center);
			/* vec now is the location where the object has to be */
			
			sub_v3_v3v3(vec, vec, td->center); // Translation needed from the initial location
			
			/* special exception, see TD_PBONE_LOCAL_MTX definition comments */
			if (td->flag & TD_PBONE_LOCAL_MTX_P) {
				/* do nothing */
			}
			else if (td->flag & TD_PBONE_LOCAL_MTX_C) {
				mul_m3_v3(pmtx, vec);	// To Global space
				mul_m3_v3(td->ext->l_smtx, vec);// To Pose space (Local Location)
			}
			else {
				mul_m3_v3(pmtx, vec);	// To Global space
				mul_m3_v3(td->smtx, vec);// To Pose space
			}

			protectedTransBits(td->protectflag, vec);
			
			add_v3_v3v3(td->loc, td->iloc, vec);
			
			constraintTransLim(t, td);
		}
		
		/* rotation */
		if ((t->flag & T_V3D_ALIGN)==0) { // align mode doesn't rotate objects itself
			/* euler or quaternion/axis-angle? */
			if (td->ext->rotOrder == ROT_MODE_QUAT) {
				mul_serie_m3(fmat, td->mtx, mat, td->smtx, NULL, NULL, NULL, NULL, NULL);
				
				mat3_to_quat( quat,fmat);	// Actual transform
				
				mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
				/* this function works on end result */
				protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
				
			}
			else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
				/* calculate effect based on quats */
				float iquat[4], tquat[4];
				
				axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);
				
				mul_serie_m3(fmat, td->mtx, mat, td->smtx, NULL, NULL, NULL, NULL, NULL);
				mat3_to_quat( quat,fmat);	// Actual transform
				mul_qt_qtqt(tquat, quat, iquat);
				
				quat_to_axis_angle( td->ext->rotAxis, td->ext->rotAngle,tquat); 
				
				/* this function works on end result */
				protectedAxisAngleBits(td->protectflag, td->ext->rotAxis, td->ext->rotAngle, td->ext->irotAxis, td->ext->irotAngle);
			}
			else { 
				float eulmat[3][3];
				
				mul_m3_m3m3(totmat, mat, td->mtx);
				mul_m3_m3m3(smat, td->smtx, totmat);
				
				/* calculate the total rotatation in eulers */
				copy_v3_v3(eul, td->ext->irot);
				eulO_to_mat3( eulmat,eul, td->ext->rotOrder);
				
				/* mat = transform, obmat = bone rotation */
				mul_m3_m3m3(fmat, smat, eulmat);
				
				mat3_to_compatible_eulO( eul, td->ext->rot, td->ext->rotOrder,fmat);
				
				/* and apply (to end result only) */
				protectedRotateBits(td->protectflag, eul, td->ext->irot);
				copy_v3_v3(td->ext->rot, eul);
			}
			
			constraintRotLim(t, td);
		}
	}
	else {
		if ((td->flag & TD_NO_LOC) == 0)
		{
			/* translation */
			sub_v3_v3v3(vec, td->center, center);
			mul_m3_v3(mat, vec);
			add_v3_v3(vec, center);
			/* vec now is the location where the object has to be */
			sub_v3_v3(vec, td->center);
			mul_m3_v3(td->smtx, vec);
			
			protectedTransBits(td->protectflag, vec);
			
			add_v3_v3v3(td->loc, td->iloc, vec);
		}
		
		
		constraintTransLim(t, td);
		
		/* rotation */
		if ((t->flag & T_V3D_ALIGN)==0) { // align mode doesn't rotate objects itself
			/* euler or quaternion? */
			   if ((td->ext->rotOrder == ROT_MODE_QUAT) || (td->flag & TD_USEQUAT)) {
				mul_serie_m3(fmat, td->mtx, mat, td->smtx, NULL, NULL, NULL, NULL, NULL);
				mat3_to_quat( quat,fmat);	// Actual transform
				
				mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
				/* this function works on end result */
				protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
			}
			else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
				/* calculate effect based on quats */
				float iquat[4], tquat[4];
				
				axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);
				
				mul_serie_m3(fmat, td->mtx, mat, td->smtx, NULL, NULL, NULL, NULL, NULL);
				mat3_to_quat( quat,fmat);	// Actual transform
				mul_qt_qtqt(tquat, quat, iquat);
				
				quat_to_axis_angle( td->ext->rotAxis, td->ext->rotAngle,tquat); 
				
				/* this function works on end result */
				protectedAxisAngleBits(td->protectflag, td->ext->rotAxis, td->ext->rotAngle, td->ext->irotAxis, td->ext->irotAngle);
			}
			else {
				float obmat[3][3];
				
				mul_m3_m3m3(totmat, mat, td->mtx);
				mul_m3_m3m3(smat, td->smtx, totmat);
				
				/* calculate the total rotatation in eulers */
				add_v3_v3v3(eul, td->ext->irot, td->ext->drot); /* we have to correct for delta rot */
				eulO_to_mat3( obmat,eul, td->ext->rotOrder);
				/* mat = transform, obmat = object rotation */
				mul_m3_m3m3(fmat, smat, obmat);
				
				mat3_to_compatible_eulO( eul, td->ext->rot, td->ext->rotOrder,fmat);
				
				/* correct back for delta rot */
				sub_v3_v3v3(eul, eul, td->ext->drot);
				
				/* and apply */
				protectedRotateBits(td->protectflag, eul, td->ext->irot);
				copy_v3_v3(td->ext->rot, eul);
			}
			
			constraintRotLim(t, td);
		}
	}
}

static void applyRotation(TransInfo *t, float angle, float axis[3])
{
	TransData *td = t->data;
	float mat[3][3];
	int i;
	
	vec_rot_to_mat3( mat,axis, angle);
	
	for (i = 0 ; i < t->total; i++, td++) {
		
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		if (t->con.applyRot) {
			t->con.applyRot(t, td, axis, NULL);
			vec_rot_to_mat3( mat,axis, angle * td->factor);
		}
		else if (t->flag & T_PROP_EDIT) {
			vec_rot_to_mat3( mat,axis, angle * td->factor);
		}
		
		ElementRotation(t, td, mat, t->around);
	}
}

int Rotation(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[128], *spos= str;
	
	float final;

	final = t->values[0];
	
	snapGrid(t, &final);
	
	if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
		t->con.applyRot(t, NULL, t->axis, NULL);
	}
	else {
		/* reset axis if constraint is not set */
		copy_v3_v3(t->axis, t->axis_orig);
	}
	
	applySnapping(t, &final);
	
	if (hasNumInput(&t->num)) {
		char c[20];
		
		applyNumInput(&t->num, &final);
		
		outputNumInput(&(t->num), c);
		
		spos+= sprintf(spos, "Rot: %s %s %s", &c[0], t->con.text, t->proptext);

		/* Clamp between -180 and 180 */
		final= angle_wrap_rad(DEG2RADF(final));
	}
	else {
		spos += sprintf(spos, "Rot: %.2f%s %s", RAD2DEGF(final), t->con.text, t->proptext);
	}
	
	if (t->flag & (T_PROP_EDIT|T_PROP_CONNECTED)) {
		spos += sprintf(spos, " Proportional size: %.2f", t->prop_size);
	}
	(void)spos;

	t->values[0] = final;
	
	applyRotation(t, final, t->axis);
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
	
	return 1;
}


/* ************************** TRACKBALL *************************** */

void initTrackball(TransInfo *t)
{
	t->mode = TFM_TRACKBALL;
	t->transform = Trackball;

	initMouseInputMode(t, &t->mouse, INPUT_TRACKBALL);

	t->idx_max = 1;
	t->num.idx_max = 1;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;

	t->num.increment = 1.0f;

	t->flag |= T_NO_CONSTRAINT;
}

static void applyTrackball(TransInfo *t, float axis1[3], float axis2[3], float angles[2])
{
	TransData *td = t->data;
	float mat[3][3], smat[3][3], totmat[3][3];
	int i;

	vec_rot_to_mat3( smat,axis1, angles[0]);
	vec_rot_to_mat3( totmat,axis2, angles[1]);

	mul_m3_m3m3(mat, smat, totmat);

	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (t->flag & T_PROP_EDIT) {
			vec_rot_to_mat3( smat,axis1, td->factor * angles[0]);
			vec_rot_to_mat3( totmat,axis2, td->factor * angles[1]);

			mul_m3_m3m3(mat, smat, totmat);
		}

		ElementRotation(t, td, mat, t->around);
	}
}

int Trackball(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[128], *spos= str;
	float axis1[3], axis2[3];
	float mat[3][3], totmat[3][3], smat[3][3];
	float phi[2];

	copy_v3_v3(axis1, t->persinv[0]);
	copy_v3_v3(axis2, t->persinv[1]);
	normalize_v3(axis1);
	normalize_v3(axis2);

	phi[0] = t->values[0];
	phi[1] = t->values[1];

	snapGrid(t, phi);

	if (hasNumInput(&t->num)) {
		char c[40];

		applyNumInput(&t->num, phi);

		outputNumInput(&(t->num), c);

		spos += sprintf(spos, "Trackball: %s %s %s", &c[0], &c[20], t->proptext);

		phi[0] = DEG2RADF(phi[0]);
		phi[1] = DEG2RADF(phi[1]);
	}
	else {
		spos += sprintf(spos, "Trackball: %.2f %.2f %s", RAD2DEGF(phi[0]), RAD2DEGF(phi[1]), t->proptext);
	}

	if (t->flag & (T_PROP_EDIT|T_PROP_CONNECTED)) {
		spos += sprintf(spos, " Proportional size: %.2f", t->prop_size);
	}
	(void)spos;

	vec_rot_to_mat3( smat,axis1, phi[0]);
	vec_rot_to_mat3( totmat,axis2, phi[1]);

	mul_m3_m3m3(mat, smat, totmat);

	// TRANSFORM_FIX_ME
	//copy_m3_m3(t->mat, mat);	// used in manipulator

	applyTrackball(t, axis1, axis2, phi);

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** TRANSLATION *************************** */

void initTranslation(TransInfo *t)
{
	if (t->spacetype == SPACE_ACTION) {
		/* this space uses time translate */
		t->state = TRANS_CANCEL;
	}

	t->mode = TFM_TRANSLATION;
	t->transform = Translation;

	initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

	t->idx_max = (t->flag & T_2D_EDIT)? 1: 2;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	if (t->spacetype == SPACE_VIEW3D) {
		RegionView3D *rv3d = t->ar->regiondata;

		if (rv3d) {
			t->snap[0] = 0.0f;
			t->snap[1] = rv3d->gridview * 1.0f;
			t->snap[2] = t->snap[1] * 0.1f;
		}
	}
	else if (ELEM(t->spacetype, SPACE_IMAGE, SPACE_CLIP)) {
		t->snap[0] = 0.0f;
		t->snap[1] = 0.125f;
		t->snap[2] = 0.0625f;
	}
	else {
		t->snap[0] = 0.0f;
		t->snap[1] = t->snap[2] = 1.0f;
	}

	t->num.increment = t->snap[1];
}

static void headerTranslation(TransInfo *t, float vec[3], char *str)
{
	char *spos= str;
	char tvec[60];
	char distvec[20];
	char autoik[20];
	float dist;

	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec);
		dist = len_v3(t->num.val);
	}
	else {
		float dvec[3];

		copy_v3_v3(dvec, vec);
		applyAspectRatio(t, dvec);

		dist = len_v3(vec);
		if (!(t->flag & T_2D_EDIT) && t->scene->unit.system) {
			int i, do_split= t->scene->unit.flag & USER_UNIT_OPT_SPLIT ? 1:0;

			for (i=0; i<3; i++)
				bUnit_AsString(&tvec[i*20], 20, dvec[i]*t->scene->unit.scale_length, 4, t->scene->unit.system, B_UNIT_LENGTH, do_split, 1);
		}
		else {
			sprintf(&tvec[0], "%.4f", dvec[0]);
			sprintf(&tvec[20], "%.4f", dvec[1]);
			sprintf(&tvec[40], "%.4f", dvec[2]);
		}
	}

	if (!(t->flag & T_2D_EDIT) && t->scene->unit.system)
		bUnit_AsString(distvec, sizeof(distvec), dist*t->scene->unit.scale_length, 4, t->scene->unit.system, B_UNIT_LENGTH, t->scene->unit.flag & USER_UNIT_OPT_SPLIT, 0);
	else if ( dist > 1e10f || dist < -1e10f )	/* prevent string buffer overflow */
		sprintf(distvec, "%.4e", dist);
	else
		sprintf(distvec, "%.4f", dist);

	if (t->flag & T_AUTOIK) {
		short chainlen= t->settings->autoik_chainlen;

		if (chainlen)
			sprintf(autoik, "AutoIK-Len: %d", chainlen);
		else
			autoik[0]= '\0';
	}
	else
		autoik[0]= '\0';

	if (t->con.mode & CON_APPLY) {
		switch(t->num.idx_max) {
		case 0:
			spos += sprintf(spos, "D: %s (%s)%s %s  %s", &tvec[0], distvec, t->con.text, t->proptext, &autoik[0]);
			break;
		case 1:
			spos += sprintf(spos, "D: %s   D: %s (%s)%s %s  %s", &tvec[0], &tvec[20], distvec, t->con.text, t->proptext, &autoik[0]);
			break;
		case 2:
			spos += sprintf(spos, "D: %s   D: %s  D: %s (%s)%s %s  %s", &tvec[0], &tvec[20], &tvec[40], distvec, t->con.text, t->proptext, &autoik[0]);
		}
	}
	else {
		if (t->flag & T_2D_EDIT)
			spos += sprintf(spos, "Dx: %s   Dy: %s (%s)%s %s", &tvec[0], &tvec[20], distvec, t->con.text, t->proptext);
		else
			spos += sprintf(spos, "Dx: %s   Dy: %s  Dz: %s (%s)%s %s  %s", &tvec[0], &tvec[20], &tvec[40], distvec, t->con.text, t->proptext, &autoik[0]);
	}
	
	if (t->flag & (T_PROP_EDIT|T_PROP_CONNECTED)) {
		spos += sprintf(spos, " Proportional size: %.2f", t->prop_size);
	}
	(void)spos;
}

static void applyTranslation(TransInfo *t, float vec[3])
{
	TransData *td = t->data;
	float tvec[3];
	int i;

	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		/* handle snapping rotation before doing the translation */
		if (usingSnappingNormal(t)) {
			if (validSnappingNormal(t)) {
				float *original_normal = td->axismtx[2];
				float axis[3];
				float quat[4];
				float mat[3][3];
				float angle;
				
				cross_v3_v3v3(axis, original_normal, t->tsnap.snapNormal);
				angle = saacos(dot_v3v3(original_normal, t->tsnap.snapNormal));
				
				axis_angle_to_quat(quat, axis, angle);
				
				quat_to_mat3( mat,quat);
				
				ElementRotation(t, td, mat, V3D_LOCAL);
			}
			else {
				float mat[3][3];
				
				unit_m3(mat);
				
				ElementRotation(t, td, mat, V3D_LOCAL);
			}
		}
		
		if (t->con.applyVec) {
			float pvec[3];
			t->con.applyVec(t, td, vec, tvec, pvec);
		}
		else {
			copy_v3_v3(tvec, vec);
		}
		
		mul_m3_v3(td->smtx, tvec);
		mul_v3_fl(tvec, td->factor);
		
		protectedTransBits(td->protectflag, tvec);
		
		if (td->loc)
			add_v3_v3v3(td->loc, td->iloc, tvec);
		
		constraintTransLim(t, td);
	}
}

/* uses t->vec to store actual translation in */
int Translation(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[250];

	if (t->con.mode & CON_APPLY) {
		float pvec[3] = {0.0f, 0.0f, 0.0f};
		float tvec[3];
		if (hasNumInput(&t->num)) {
			removeAspectRatio(t, t->values);
		}
		applySnapping(t, t->values);
		t->con.applyVec(t, NULL, t->values, tvec, pvec);
		copy_v3_v3(t->values, tvec);
		headerTranslation(t, pvec, str);
	}
	else {
		snapGrid(t, t->values);
		applyNumInput(&t->num, t->values);
		if (hasNumInput(&t->num)) {
			removeAspectRatio(t, t->values);
		}
		applySnapping(t, t->values);
		headerTranslation(t, t->values, str);
	}

	applyTranslation(t, t->values);

	/* evil hack - redo translation if clipping needed */
	if (t->flag & T_CLIP_UV && clipUVTransform(t, t->values, 0))
		applyTranslation(t, t->values);

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** SHRINK/FATTEN *************************** */

void initShrinkFatten(TransInfo *t)
{
	// If not in mesh edit mode, fallback to Resize
	if (t->obedit==NULL || t->obedit->type != OB_MESH) {
		initResize(t);
	}
	else {
		t->mode = TFM_SHRINKFATTEN;
		t->transform = ShrinkFatten;

		initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

		t->idx_max = 0;
		t->num.idx_max = 0;
		t->snap[0] = 0.0f;
		t->snap[1] = 1.0f;
		t->snap[2] = t->snap[1] * 0.1f;

		t->num.increment = t->snap[1];

		t->flag |= T_NO_CONSTRAINT;
	}
}



int ShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
	float vec[3];
	float distance;
	int i;
	char str[64];
	TransData *td = t->data;

	distance = -t->values[0];

	snapGrid(t, &distance);

	applyNumInput(&t->num, &distance);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);

		sprintf(str, "Shrink/Fatten: %s %s", c, t->proptext);
	}
	else {
		/* default header print */
		sprintf(str, "Shrink/Fatten: %.4f %s", distance, t->proptext);
	}


	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		copy_v3_v3(vec, td->axismtx[2]);
		mul_v3_fl(vec, distance);
		mul_v3_fl(vec, td->factor);

		add_v3_v3v3(td->loc, td->iloc, vec);
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** TILT *************************** */

void initTilt(TransInfo *t)
{
	t->mode = TFM_TILT;
	t->transform = Tilt;

	initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;

	t->num.increment = t->snap[1];

	t->flag |= T_NO_CONSTRAINT|T_NO_PROJECT;
}



int Tilt(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	int i;
	char str[50];

	float final;

	final = t->values[0];

	snapGrid(t, &final);

	if (hasNumInput(&t->num)) {
		char c[20];

		applyNumInput(&t->num, &final);

		outputNumInput(&(t->num), c);

		sprintf(str, "Tilt: %s %s", &c[0], t->proptext);

		final = DEG2RADF(final);
	}
	else {
		sprintf(str, "Tilt: %.2f %s", RAD2DEGF(final), t->proptext);
	}

	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (td->val) {
			*td->val = td->ival + final * td->factor;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}


/* ******************** Curve Shrink/Fatten *************** */

void initCurveShrinkFatten(TransInfo *t)
{
	t->mode = TFM_CURVE_SHRINKFATTEN;
	t->transform = CurveShrinkFatten;

	initMouseInputMode(t, &t->mouse, INPUT_SPRING);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	t->num.increment = t->snap[1];

	t->flag |= T_NO_ZERO;
	t->num.flag |= NUM_NO_ZERO;

	t->flag |= T_NO_CONSTRAINT;
}

int CurveShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float ratio;
	int i;
	char str[50];

	ratio = t->values[0];

	snapGrid(t, &ratio);

	applyNumInput(&t->num, &ratio);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);
		sprintf(str, "Shrink/Fatten: %s", c);
	}
	else {
		sprintf(str, "Shrink/Fatten: %3f", ratio);
	}

	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (td->val) {
			// *td->val= ratio;
			*td->val= td->ival*ratio;
			if (*td->val <= 0.0f) *td->val = 0.001f;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** PUSH/PULL *************************** */

void initPushPull(TransInfo *t)
{
	t->mode = TFM_PUSHPULL;
	t->transform = PushPull;

	initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 1.0f;
	t->snap[2] = t->snap[1] * 0.1f;

	t->num.increment = t->snap[1];
}


int PushPull(TransInfo *t, const int UNUSED(mval[2]))
{
	float vec[3], axis[3];
	float distance;
	int i;
	char str[128];
	TransData *td = t->data;

	distance = t->values[0];

	snapGrid(t, &distance);

	applyNumInput(&t->num, &distance);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);

		sprintf(str, "Push/Pull: %s%s %s", c, t->con.text, t->proptext);
	}
	else {
		/* default header print */
		sprintf(str, "Push/Pull: %.4f%s %s", distance, t->con.text, t->proptext);
	}

	if (t->con.applyRot && t->con.mode & CON_APPLY) {
		t->con.applyRot(t, NULL, axis, NULL);
	}

	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		sub_v3_v3v3(vec, t->center, td->center);
		if (t->con.applyRot && t->con.mode & CON_APPLY) {
			t->con.applyRot(t, td, axis, NULL);
			if (isLockConstraint(t)) {
				float dvec[3];
				project_v3_v3v3(dvec, vec, axis);
				sub_v3_v3(vec, dvec);
			}
			else {
				project_v3_v3v3(vec, vec, axis);
			}
		}
		normalize_v3(vec);
		mul_v3_fl(vec, distance);
		mul_v3_fl(vec, td->factor);

		add_v3_v3v3(td->loc, td->iloc, vec);
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** BEVEL **************************** */

void initBevel(TransInfo *t)
{
	t->transform = Bevel;
	t->handleEvent = handleEventBevel;

	initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_ABSOLUTE);

	t->mode = TFM_BEVEL;
	t->flag |= T_NO_CONSTRAINT;
	t->num.flag |= NUM_NO_NEGATIVE;

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	t->num.increment = t->snap[1];

	/* DON'T KNOW WHY THIS IS NEEDED */
	if (G.editBMesh->imval[0] == 0 && G.editBMesh->imval[1] == 0) {
		/* save the initial mouse co */
		G.editBMesh->imval[0] = t->imval[0];
		G.editBMesh->imval[1] = t->imval[1];
	}
	else {
		/* restore the mouse co from a previous call to initTransform() */
		t->imval[0] = G.editBMesh->imval[0];
		t->imval[1] = G.editBMesh->imval[1];
	}
}

int handleEventBevel(TransInfo *t, wmEvent *event)
{
	if (event->val==KM_PRESS) {
		if (!G.editBMesh) return 0;

		switch (event->type) {
		case MIDDLEMOUSE:
			G.editBMesh->options ^= BME_BEVEL_VERT;
			t->state = TRANS_CANCEL;
			return 1;
		//case PADPLUSKEY:
		//	G.editBMesh->options ^= BME_BEVEL_RES;
		//	G.editBMesh->res += 1;
		//	if (G.editBMesh->res > 4) {
		//		G.editBMesh->res = 4;
		//	}
		//	t->state = TRANS_CANCEL;
		//	return 1;
		//case PADMINUS:
		//	G.editBMesh->options ^= BME_BEVEL_RES;
		//	G.editBMesh->res -= 1;
		//	if (G.editBMesh->res < 0) {
		//		G.editBMesh->res = 0;
		//	}
		//	t->state = TRANS_CANCEL;
		//	return 1;
		default:
			return 0;
		}
	}
	return 0;
}

int Bevel(TransInfo *t, const int UNUSED(mval[2]))
{
	float distance,d;
	int i;
	char str[128];
	const char *mode;
	TransData *td = t->data;

	mode = (G.editBMesh->options & BME_BEVEL_VERT) ? "verts only" : "normal";
	distance = t->values[0] / 4; /* 4 just seemed a nice value to me, nothing special */

	distance = fabs(distance);

	snapGrid(t, &distance);

	applyNumInput(&t->num, &distance);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);

		sprintf(str, "Bevel - Dist: %s, Mode: %s (MMB to toggle))", c, mode);
	}
	else {
		/* default header print */
		sprintf(str, "Bevel - Dist: %.4f, Mode: %s (MMB to toggle))", distance, mode);
	}

	if (distance < 0) distance = -distance;
	for (i = 0 ; i < t->total; i++, td++) {
		if (td->axismtx[1][0] > 0 && distance > td->axismtx[1][0]) {
			d = td->axismtx[1][0];
		}
		else {
			d = distance;
		}
		madd_v3_v3v3fl(td->loc, td->center, td->axismtx[0], (*td->val) * d);
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** BEVEL WEIGHT *************************** */

void initBevelWeight(TransInfo *t)
{
	t->mode = TFM_BWEIGHT;
	t->transform = BevelWeight;

	initMouseInputMode(t, &t->mouse, INPUT_SPRING);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	t->num.increment = t->snap[1];

	t->flag |= T_NO_CONSTRAINT|T_NO_PROJECT;
}

int BevelWeight(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float weight;
	int i;
	char str[50];

	weight = t->values[0];

	weight -= 1.0f;
	if (weight > 1.0f) weight = 1.0f;

	snapGrid(t, &weight);

	applyNumInput(&t->num, &weight);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);

		if (weight >= 0.0f)
			sprintf(str, "Bevel Weight: +%s %s", c, t->proptext);
		else
			sprintf(str, "Bevel Weight: %s %s", c, t->proptext);
	}
	else {
		/* default header print */
		if (weight >= 0.0f)
			sprintf(str, "Bevel Weight: +%.3f %s", weight, t->proptext);
		else
			sprintf(str, "Bevel Weight: %.3f %s", weight, t->proptext);
	}

	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->val) {
			*td->val = td->ival + weight * td->factor;
			if (*td->val < 0.0f) *td->val = 0.0f;
			if (*td->val > 1.0f) *td->val = 1.0f;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** CREASE *************************** */

void initCrease(TransInfo *t)
{
	t->mode = TFM_CREASE;
	t->transform = Crease;

	initMouseInputMode(t, &t->mouse, INPUT_SPRING);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	t->num.increment = t->snap[1];

	t->flag |= T_NO_CONSTRAINT|T_NO_PROJECT;
}

int Crease(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float crease;
	int i;
	char str[50];

	crease = t->values[0];

	crease -= 1.0f;
	if (crease > 1.0f) crease = 1.0f;

	snapGrid(t, &crease);

	applyNumInput(&t->num, &crease);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);

		if (crease >= 0.0f)
			sprintf(str, "Crease: +%s %s", c, t->proptext);
		else
			sprintf(str, "Crease: %s %s", c, t->proptext);
	}
	else {
		/* default header print */
		if (crease >= 0.0f)
			sprintf(str, "Crease: +%.3f %s", crease, t->proptext);
		else
			sprintf(str, "Crease: %.3f %s", crease, t->proptext);
	}

	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (td->val) {
			*td->val = td->ival + crease * td->factor;
			if (*td->val < 0.0f) *td->val = 0.0f;
			if (*td->val > 1.0f) *td->val = 1.0f;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ******************** EditBone (B-bone) width scaling *************** */

void initBoneSize(TransInfo *t)
{
	t->mode = TFM_BONESIZE;
	t->transform = BoneSize;

	initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

	t->idx_max = 2;
	t->num.idx_max = 2;
	t->num.flag |= NUM_NULL_ONE;
	t->num.flag |= NUM_AFFECT_ALL;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	t->num.increment = t->snap[1];
}

static void headerBoneSize(TransInfo *t, float vec[3], char *str)
{
	char tvec[60];
	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec);
	}
	else {
		sprintf(&tvec[0], "%.4f", vec[0]);
		sprintf(&tvec[20], "%.4f", vec[1]);
		sprintf(&tvec[40], "%.4f", vec[2]);
	}

	/* hmm... perhaps the y-axis values don't need to be shown? */
	if (t->con.mode & CON_APPLY) {
		if (t->num.idx_max == 0)
			sprintf(str, "ScaleB: %s%s %s", &tvec[0], t->con.text, t->proptext);
		else
			sprintf(str, "ScaleB: %s : %s : %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
	}
	else {
		sprintf(str, "ScaleB X: %s  Y: %s  Z: %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
	}
}

static void ElementBoneSize(TransInfo *t, TransData *td, float mat[3][3])
{
	float tmat[3][3], smat[3][3], oldy;
	float sizemat[3][3];

	mul_m3_m3m3(smat, mat, td->mtx);
	mul_m3_m3m3(tmat, td->smtx, smat);

	if (t->con.applySize) {
		t->con.applySize(t, td, tmat);
	}

	/* we've tucked the scale in loc */
	oldy= td->iloc[1];
	size_to_mat3( sizemat,td->iloc);
	mul_m3_m3m3(tmat, tmat, sizemat);
	mat3_to_size( td->loc,tmat);
	td->loc[1]= oldy;
}

int BoneSize(TransInfo *t, const int mval[2])
{
	TransData *td = t->data;
	float size[3], mat[3][3];
	float ratio;
	int i;
	char str[60];
	
	// TRANSFORM_FIX_ME MOVE TO MOUSE INPUT
	/* for manipulator, center handle, the scaling can't be done relative to center */
	if ((t->flag & T_USES_MANIPULATOR) && t->con.mode==0) {
		ratio = 1.0f - ((t->imval[0] - mval[0]) + (t->imval[1] - mval[1]))/100.0f;
	}
	else {
		ratio = t->values[0];
	}
	
	size[0] = size[1] = size[2] = ratio;
	
	snapGrid(t, size);
	
	if (hasNumInput(&t->num)) {
		applyNumInput(&t->num, size);
		constraintNumInput(t, size);
	}
	
	size_to_mat3( mat,size);
	
	if (t->con.applySize) {
		t->con.applySize(t, NULL, mat);
	}
	
	copy_m3_m3(t->mat, mat);	// used in manipulator
	
	headerBoneSize(t, size, str);
	
	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		ElementBoneSize(t, td, mat);
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
	
	return 1;
}


/* ******************** EditBone envelope *************** */

void initBoneEnvelope(TransInfo *t)
{
	t->mode = TFM_BONE_ENVELOPE;
	t->transform = BoneEnvelope;
	
	initMouseInputMode(t, &t->mouse, INPUT_SPRING);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	
	t->num.increment = t->snap[1];

	t->flag |= T_NO_CONSTRAINT|T_NO_PROJECT;
}

int BoneEnvelope(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float ratio;
	int i;
	char str[50];
	
	ratio = t->values[0];
	
	snapGrid(t, &ratio);
	
	applyNumInput(&t->num, &ratio);
	
	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];
		
		outputNumInput(&(t->num), c);
		sprintf(str, "Envelope: %s", c);
	}
	else {
		sprintf(str, "Envelope: %3f", ratio);
	}
	
	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		if (td->val) {
			/* if the old/original value was 0.0f, then just use ratio */
			if (td->ival)
				*td->val= td->ival*ratio;
			else
				*td->val= ratio;
		}
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
	
	return 1;
}

/* ********************  Edge Slide   *************** */
static BMEdge *get_other_edge(BMesh *bm, BMVert *v, BMEdge *e)
{
	BMIter iter;
	BMEdge *e2;

	BM_ITER(e2, &iter, bm, BM_EDGES_OF_VERT, v) {
		if (BM_elem_flag_test(e2, BM_ELEM_SELECT) && e2 != e)
			return e2;
	}

	return NULL;
}

static BMLoop *get_next_loop(BMesh *UNUSED(bm), BMVert *v, BMLoop *l, 
                             BMEdge *olde, BMEdge *nexte, float vec[3])
{
	BMLoop *firstl;
	float a[3] = {0.0f, 0.0f, 0.0f}, n[3] = {0.0f, 0.0f, 0.0f};
	int i=0;

	firstl = l;
	do {
		l = BM_face_other_edge_loop(l->f, l->e, v);
		if (l->radial_next == l)
			return NULL;
		
		if (l->e == nexte) {
			if (i) {
				mul_v3_fl(a, 1.0f / (float)i);
			}
			else {
				float f1[3], f2[3], f3[3];

				sub_v3_v3v3(f1, BM_edge_other_vert(olde, v)->co, v->co);
				sub_v3_v3v3(f2, BM_edge_other_vert(nexte, v)->co, v->co);

				cross_v3_v3v3(f3, f1, l->f->no);
				cross_v3_v3v3(a, f2, l->f->no);
				mul_v3_fl(a, -1.0f);

				add_v3_v3(a, f3);
				mul_v3_fl(a, 0.5f);
			}
			
			copy_v3_v3(vec, a);
			return l;
		}
		else {
			sub_v3_v3v3(n, BM_edge_other_vert(l->e, v)->co, v->co);
			add_v3_v3v3(a, a, n);
			i += 1;
		}

		if (BM_face_other_edge_loop(l->f, l->e, v)->e == nexte) {
			if (i)
				mul_v3_fl(a, 1.0f / (float)i);
			
			copy_v3_v3(vec, a);
			return BM_face_other_edge_loop(l->f, l->e, v);
		}
		
		l = l->radial_next;
	} while (l != firstl); 

	if (i)
		mul_v3_fl(a, 1.0f / (float)i);
	
	copy_v3_v3(vec, a);
	
	return NULL;
}

static int createSlideVerts(TransInfo *t)
{
	Mesh *me = t->obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	BMIter iter, iter2;
	BMEdge *e, *e1 /*, *ee, *le */ /* UNUSED */;
	BMVert *v, *v2, *first;
	BMLoop *l, *l1, *l2;
	TransDataSlideVert *tempsv;
	BMBVHTree *btree = BMBVH_NewBVH(em, 0, NULL, NULL);
	SmallHash table;
	SlideData *sld = MEM_callocN(sizeof(*sld), "sld");
	View3D *v3d = t->sa ? t->sa->spacedata.first : NULL;
	RegionView3D *rv3d = t->ar ? t->ar->regiondata : NULL; /* background mode support */
	ARegion *ar = t->ar;
	float projectMat[4][4];
	float start[3] = {0.0f, 0.0f, 0.0f}, dir[3], end[3] = {0.0f, 0.0f, 0.0f};
	float vec[3], vec2[3], lastvec[3] /*, size, dis=0.0, z */ /* UNUSED */;
	int numsel, i, j;

	if (!v3d) {
		/* ok, let's try to survive this */
		unit_m4(projectMat);
	}
	else {
		ED_view3d_ob_project_mat_get(rv3d, t->obedit, projectMat);
	}
	
	BLI_smallhash_init(&sld->vhash);
	BLI_smallhash_init(&sld->origfaces);
	BLI_smallhash_init(&table);
	
	/*ensure valid selection*/
	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			numsel = 0;
			BM_ITER(e, &iter2, em->bm, BM_EDGES_OF_VERT, v) {
				if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
					/* BMESH_TODO: this is probably very evil,
					 * set v->e to a selected edge*/
					v->e = e;

					numsel++;
				}
			}

			if (numsel == 0 || numsel > 2) {
				MEM_freeN(sld);
				BMBVH_FreeBVH(btree);
				return 0; /* invalid edge selection */
			}
		}
	}

	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
			if (BM_edge_face_count(e) != 2) {
				MEM_freeN(sld);
				BMBVH_FreeBVH(btree);
				return 0; /* can only handle exactly 2 faces around each edge */
			}
		}
	}

	j = 0;
	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			BM_elem_flag_enable(v, BM_ELEM_TAG);
			BLI_smallhash_insert(&table, (uintptr_t)v, SET_INT_IN_POINTER(j));
			j += 1;
		}
		else {
			BM_elem_flag_disable(v, BM_ELEM_TAG);
		}
	}

	if (!j) {
		MEM_freeN(sld);
		BMBVH_FreeBVH(btree);
		return 0;
	}

	tempsv = MEM_callocN(sizeof(TransDataSlideVert)*j, "tempsv");

	j = 0;
	while (1) {
		v = NULL;
		BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG))
				break;

		}

		if (!v)
			break;

		if (!v->e)
			continue;
		
		first = v;

		/*walk along the edge loop*/
		e = v->e;

		/*first, rewind*/
		numsel = 0;
		do {
			e = get_other_edge(bm, v, e);
			if (!e) {
				e = v->e;
				break;
			}

			numsel += 1;

			if (!BM_elem_flag_test(BM_edge_other_vert(e, v), BM_ELEM_TAG))
				break;

			v = BM_edge_other_vert(e, v);
		} while (e != first->e);

		BM_elem_flag_disable(v, BM_ELEM_TAG);

		l1 = l2 = l = NULL;
		l1 = e->l;
		l2 = e->l->radial_next;

		l = BM_face_other_edge_loop(l1->f, l1->e, v);
		sub_v3_v3v3(vec, BM_edge_other_vert(l->e, v)->co, v->co);

		if (l2 != l1) {
			l = BM_face_other_edge_loop(l2->f, l2->e, v);
			sub_v3_v3v3(vec2, BM_edge_other_vert(l->e, v)->co, v->co);
		}
		else {
			l2 = NULL;
		}

		/*iterate over the loop*/
		first = v;
		do {
			TransDataSlideVert *sv = tempsv + j;

			sv->v = v;
			sv->origvert = *v;
			copy_v3_v3(sv->upvec, vec);
			if (l2)
				copy_v3_v3(sv->downvec, vec2);

			l = BM_face_other_edge_loop(l1->f, l1->e, v);
			sv->up = BM_edge_other_vert(l->e, v);

			if (l2) {
				l = BM_face_other_edge_loop(l2->f, l2->e, v);
				sv->down = BM_edge_other_vert(l->e, v);
			}

			v2=v, v = BM_edge_other_vert(e, v);

			e1 = e;
			e = get_other_edge(bm, v, e);
			if (!e) {
				//v2=v, v = BM_edge_other_vert(l1->e, v);

				sv = tempsv + j + 1;
				sv->v = v;
				sv->origvert = *v;
				
				l = BM_face_other_edge_loop(l1->f, l1->e, v);
				sv->up = BM_edge_other_vert(l->e, v);
				sub_v3_v3v3(sv->upvec, BM_edge_other_vert(l->e, v)->co, v->co);

				if (l2) {
					l = BM_face_other_edge_loop(l2->f, l2->e, v);
					sv->down = BM_edge_other_vert(l->e, v);
					sub_v3_v3v3(sv->downvec, BM_edge_other_vert(l->e, v)->co, v->co);
				}

				BM_elem_flag_disable(v, BM_ELEM_TAG);
				BM_elem_flag_disable(v2, BM_ELEM_TAG);
				
				j += 2;
				break;
			}

			l1 = get_next_loop(bm, v, l1, e1, e, vec);
			l2 = l2 ? get_next_loop(bm, v, l2, e1, e, vec2) : NULL;

			j += 1;

			BM_elem_flag_disable(v, BM_ELEM_TAG);
			BM_elem_flag_disable(v2, BM_ELEM_TAG);
		} while (e != first->e && l1);
	}

	//EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	sld->sv = tempsv;
	sld->totsv = j;
	
	/*find mouse vector*/
	/* dis = z = -1.0f; */ /* UNUSED */
	/* size = 50.0; */ /* UNUSED */
	zero_v3(lastvec); zero_v3(dir);
	/* ee = le = NULL; */ /* UNUSED */
	BM_ITER(e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
			BMIter iter2;
			BMEdge *e2;
			float vec1[3], dis2, mval[2] = {t->mval[0], t->mval[1]}, d;
						
			/* search cross edges for visible edge to the mouse cursor,
			 * then use the shared vertex to calculate screen vector*/
			dis2 = -1.0f;
			for (i=0; i<2; i++) {
				v = i?e->v1:e->v2;
				BM_ITER(e2, &iter2, em->bm, BM_EDGES_OF_VERT, v) {
					if (BM_elem_flag_test(e2, BM_ELEM_SELECT))
						continue;
					
					if (!BMBVH_EdgeVisible(btree, e2, ar, v3d, t->obedit))
						continue;
					
					j = GET_INT_FROM_POINTER(BLI_smallhash_lookup(&table, (uintptr_t)v));

					if (tempsv[j].down) {
						ED_view3d_project_float_v3(ar, tempsv[j].down->co, vec1, projectMat);
					}
					else {
						add_v3_v3v3(vec1, v->co, tempsv[j].downvec);
						ED_view3d_project_float_v3(ar, vec1, vec1, projectMat);
					}
					
					if (tempsv[j].up) {
						ED_view3d_project_float_v3(ar, tempsv[j].up->co, vec2, projectMat);
					}
					else {
						add_v3_v3v3(vec1, v->co, tempsv[j].upvec);
						ED_view3d_project_float_v3(ar, vec2, vec2, projectMat);
					}

					d = dist_to_line_segment_v2(mval, vec1, vec2);
					if (dis2 == -1.0f || d < dis2) {
						dis2 = d;
						/* ee = e2; */ /* UNUSED */
						/* size = len_v3v3(vec1, vec2); */ /* UNUSED */
						sub_v3_v3v3(dir, vec1, vec2);
					}
				}
			}
		}
	}

	bmesh_edit_begin(em->bm, BMO_OP_FLAG_UNTAN_MULTIRES);

	/*create copies of faces for customdata projection*/
	tempsv = sld->sv;
	for (i=0; i<sld->totsv; i++, tempsv++) {
		BMIter fiter, liter;
		BMFace *f;
		BMLoop *l;
		
		BM_ITER(f, &fiter, em->bm, BM_FACES_OF_VERT, tempsv->v) {
			
			if (!BLI_smallhash_haskey(&sld->origfaces, (uintptr_t)f)) {
				BMFace *copyf = BM_face_copy(em->bm, f, TRUE, TRUE);
				
				BM_elem_select_set(em->bm, copyf, FALSE);
				BM_elem_flag_enable(copyf, BM_ELEM_HIDDEN);
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, copyf) {
					BM_elem_select_set(em->bm, l->v, FALSE);
					BM_elem_flag_enable(l->v, BM_ELEM_HIDDEN);
					BM_elem_select_set(em->bm, l->e, FALSE);
					BM_elem_flag_enable(l->e, BM_ELEM_HIDDEN);
				}

				BLI_smallhash_insert(&sld->origfaces, (uintptr_t)f, copyf);
			}
		}

		BLI_smallhash_insert(&sld->vhash, (uintptr_t)tempsv->v, tempsv);
	}
	
	sld->origfaces_init = TRUE;
	sld->em = em;
	
	/*zero out start*/
	zero_v3(start);
	
	/*dir holds a vector along edge loop*/
	copy_v3_v3(end, dir);
	mul_v3_fl(end, 0.5);
	
	sld->start[0] = t->mval[0] + start[0];
	sld->start[1] = t->mval[1] + start[1];

	sld->end[0] = t->mval[0] + end[0];
	sld->end[1] = t->mval[1] + end[1];
	
	sld->perc = 0.0f;
	
	t->customData = sld;
	
	BLI_smallhash_release(&table);
	BMBVH_FreeBVH(btree);
	
	return 1;
}

void projectSVData(TransInfo *t, int final)
{
	SlideData *sld = t->customData;
	TransDataSlideVert *tempsv;
	BMEditMesh *em = sld->em;
	SmallHash visit;
	int i;

	if (!em)
		return;
	
	if(!(t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT))
		return;

	/* don't do this at all for non-basis shape keys, too easy to
	   accidentally break uv maps or vertex colors then */
	if(em->bm->shapenr > 1)
		return;

	BLI_smallhash_init(&visit);
	
		for (i=0, tempsv=sld->sv; i<sld->totsv; i++, tempsv++) {
		BMIter fiter;
		BMFace *f;
		
		BM_ITER(f, &fiter, em->bm, BM_FACES_OF_VERT, tempsv->v) {
			BMIter liter2;
			BMFace *copyf, *copyf2;
			BMLoop *l2;
			int sel, hide;
			
			if (BLI_smallhash_haskey(&visit, (uintptr_t)f))
				continue;
			
			BLI_smallhash_insert(&visit, (uintptr_t)f, NULL);
			
			/* the face attributes of the copied face will get
			 * copied over, so its necessary to save the selection
			 * and hidden state*/
			sel = BM_elem_flag_test(f, BM_ELEM_SELECT);
			hide = BM_elem_flag_test(f, BM_ELEM_HIDDEN);
			
			copyf2 = BLI_smallhash_lookup(&sld->origfaces, (uintptr_t)f);
			
			/* project onto copied projection face */
			BM_ITER(l2, &liter2, em->bm, BM_LOOPS_OF_FACE, f) {
				copyf = copyf2;
				
				if (BM_elem_flag_test(l2->e, BM_ELEM_SELECT) || BM_elem_flag_test(l2->prev->e, BM_ELEM_SELECT)) {
					BMLoop *l3 = l2;
					
					if (!BM_elem_flag_test(l2->e, BM_ELEM_SELECT))
						l3 = l3->prev;
					
					if (sld->perc < 0.0 && BM_vert_in_face(l3->radial_next->f, tempsv->down)) {
						copyf = BLI_smallhash_lookup(&sld->origfaces, (uintptr_t)l3->radial_next->f);
					}
					else if (sld->perc > 0.0 && BM_vert_in_face(l3->radial_next->f, tempsv->up)) {
						copyf = BLI_smallhash_lookup(&sld->origfaces, (uintptr_t)l3->radial_next->f);
					}
					if (!copyf)
						continue;  /* shouldn't happen, but protection */
				}
				
				/* only loop data, no vertex data since that contains shape keys,
				 * and we do not want to mess up other shape keys */
				BM_loop_interp_from_face(em->bm, l2, copyf, FALSE, FALSE);

				if (final) {
					BM_loop_interp_multires(em->bm, l2, copyf);	
					if (copyf2 != copyf) {
						BM_loop_interp_multires(em->bm, l2, copyf2);
					}
				}
			}
			
			/* make sure face-attributes are correct (e.g. MTexPoly) */
			BM_elem_attrs_copy(em->bm, em->bm, copyf2, f);
			
			/* restore selection and hidden flags */
			BM_elem_select_set(em->bm, f, sel);
			if (!hide) { /* this check is a workaround for bug, see note - [#30735], without this edge can be hidden and selected */
				BM_elem_hide_set(em->bm, f, hide);
			}
		}
	}
	
	BLI_smallhash_release(&visit);
}

void freeSlideTempFaces(SlideData *sld) {
	if (sld->origfaces_init) {
		SmallHashIter hiter;
		BMFace *copyf;

		copyf = BLI_smallhash_iternew(&sld->origfaces, &hiter, NULL);
		for (; copyf; copyf=BLI_smallhash_iternext(&hiter, NULL)) {
			BM_face_verts_kill(sld->em->bm, copyf);
		}

		BLI_smallhash_release(&sld->origfaces);

		sld->origfaces_init = FALSE;
	}
}


void freeSlideVerts(TransInfo *t)
{
	SlideData *sld = t->customData;
	
#if 0 /*BMESH_TODO*/
	if (me->drawflag & ME_DRAWEXTRA_EDGELEN) {
		TransDataSlideVert *tempsv;
		LinkNode *look = sld->vertlist;
		GHash *vertgh = sld->vhash;
		while (look) {
			tempsv  = BLI_ghash_lookup(vertgh,(EditVert*)look->link);
			if (tempsv != NULL) {
				tempsv->up->f &= !SELECT;
				tempsv->down->f &= !SELECT;
			}
			look = look->next;
		}
	}
#endif
	
	if (!sld)
		return;
	
	freeSlideTempFaces(sld);

	bmesh_edit_end(sld->em->bm, BMO_OP_FLAG_UNTAN_MULTIRES);

	BLI_smallhash_release(&sld->vhash);
	
	MEM_freeN(sld->sv);
	MEM_freeN(sld);
	
	t->customData = NULL;
	
	recalcData(t);
}

void initEdgeSlide(TransInfo *t)
{
	SlideData *sld;

	t->mode = TFM_EDGE_SLIDE;
	t->transform = EdgeSlide;
	
	if (!createSlideVerts(t)) {
		t->state= TRANS_CANCEL;
		return;
	}
	
	sld = t->customData;

	if (!sld)
		return;

	t->customFree = freeSlideVerts;

	/* set custom point first if you want value to be initialized by init */
	setCustomPoints(t, &t->mouse, sld->end, sld->start);
	initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	t->num.increment = t->snap[1];

	t->flag |= T_NO_CONSTRAINT|T_NO_PROJECT;
}

static int doEdgeSlide(TransInfo *t, float perc)
{
	SlideData *sld = t->customData;
	TransDataSlideVert *svlist = sld->sv, *sv;
	float vec[3];
	int i;

	sld->perc = perc;

	sv = svlist;
	for (i=0; i<sld->totsv; i++, sv++) {
		if (perc > 0.0f) {
			copy_v3_v3(vec, sv->upvec);
			mul_v3_fl(vec, perc);
			add_v3_v3v3(sv->v->co, sv->origvert.co, vec);
		}
		else {
			copy_v3_v3(vec, sv->downvec);
			mul_v3_fl(vec, -perc);
			add_v3_v3v3(sv->v->co, sv->origvert.co, vec);
		}
	}
	
	projectSVData(t, 0);
	
	return 1;
}

int EdgeSlide(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[50];
	float final;

	final = t->values[0];

	snapGrid(t, &final);

	/* only do this so out of range values are not displayed */
	CLAMP(final, -1.0f, 1.0f);

	if (hasNumInput(&t->num)) {
		char c[20];

		applyNumInput(&t->num, &final);

		outputNumInput(&(t->num), c);

		sprintf(str, "Edge Slide: %s", &c[0]);
	}
	else {
		sprintf(str, "Edge Slide: %.2f", final);
	}

	CLAMP(final, -1.0f, 1.0f);

	/*do stuff here*/
	if (t->customData)
		doEdgeSlide(t, final);
	else {
		strcpy(str, "Invalid Edge Selection");
		t->state = TRANS_CANCEL;
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ******************** EditBone roll *************** */

void initBoneRoll(TransInfo *t)
{
	t->mode = TFM_BONE_ROLL;
	t->transform = BoneRoll;

	initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;

	t->num.increment = 1.0f;

	t->flag |= T_NO_CONSTRAINT|T_NO_PROJECT;
}

int BoneRoll(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	int i;
	char str[50];

	float final;

	final = t->values[0];

	snapGrid(t, &final);

	if (hasNumInput(&t->num)) {
		char c[20];

		applyNumInput(&t->num, &final);

		outputNumInput(&(t->num), c);

		sprintf(str, "Roll: %s", &c[0]);

		final = DEG2RADF(final);
	}
	else {
		sprintf(str, "Roll: %.2f", RAD2DEGF(final));
	}

	/* set roll values */
	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		*(td->val) = td->ival - final;
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** BAKE TIME ******************* */

void initBakeTime(TransInfo *t)
{
	t->transform = BakeTime;
	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 1.0f;
	t->snap[2] = t->snap[1] * 0.1f;

	t->num.increment = t->snap[1];
}

int BakeTime(TransInfo *t, const int mval[2])
{
	TransData *td = t->data;
	float time;
	int i;
	char str[50];

	float fac = 0.1f;

	if (t->mouse.precision) {
		/* calculate ratio for shiftkey pos, and for total, and blend these for precision */
		time= (float)(t->center2d[0] - t->mouse.precision_mval[0]) * fac;
		time+= 0.1f*((float)(t->center2d[0]*fac - mval[0]) -time);
	}
	else {
		time = (float)(t->center2d[0] - mval[0])*fac;
	}

	snapGrid(t, &time);

	applyNumInput(&t->num, &time);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);

		if (time >= 0.0f)
			sprintf(str, "Time: +%s %s", c, t->proptext);
		else
			sprintf(str, "Time: %s %s", c, t->proptext);
	}
	else {
		/* default header print */
		if (time >= 0.0f)
			sprintf(str, "Time: +%.3f %s", time, t->proptext);
		else
			sprintf(str, "Time: %.3f %s", time, t->proptext);
	}

	for (i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (td->val) {
			*td->val = td->ival + time * td->factor;
			if (td->ext->size && *td->val < *td->ext->size) *td->val = *td->ext->size;
			if (td->ext->quat && *td->val > *td->ext->quat) *td->val = *td->ext->quat;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** MIRROR *************************** */

void initMirror(TransInfo *t)
{
	t->transform = Mirror;
	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	t->flag |= T_NULL_ONE;
	if (!t->obedit) {
		t->flag |= T_NO_ZERO;
	}
}

int Mirror(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td;
	float size[3], mat[3][3];
	int i;
	char str[200];

	/*
	 * OPTIMIZATION:
	 * This still recalcs transformation on mouse move
	 * while it should only recalc on constraint change
	 * */

	/* if an axis has been selected */
	if (t->con.mode & CON_APPLY) {
		size[0] = size[1] = size[2] = -1;

		size_to_mat3( mat,size);

		if (t->con.applySize) {
			t->con.applySize(t, NULL, mat);
		}

		sprintf(str, "Mirror%s", t->con.text);

		for (i = 0, td=t->data; i < t->total; i++, td++) {
			if (td->flag & TD_NOACTION)
				break;

			if (td->flag & TD_SKIP)
				continue;

			ElementResize(t, td, mat);
		}

		recalcData(t);

		ED_area_headerprint(t->sa, str);
	}
	else {
		size[0] = size[1] = size[2] = 1;

		size_to_mat3( mat,size);

		for (i = 0, td=t->data; i < t->total; i++, td++) {
			if (td->flag & TD_NOACTION)
				break;

			if (td->flag & TD_SKIP)
				continue;

			ElementResize(t, td, mat);
		}

		recalcData(t);

		if (t->flag & T_2D_EDIT)
			ED_area_headerprint(t->sa, "Select a mirror axis (X, Y)");
		else
			ED_area_headerprint(t->sa, "Select a mirror axis (X, Y, Z)");
	}

	return 1;
}

/* ************************** ALIGN *************************** */

void initAlign(TransInfo *t)
{
	t->flag |= T_NO_CONSTRAINT;

	t->transform = Align;

	initMouseInputMode(t, &t->mouse, INPUT_NONE);
}

int Align(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float center[3];
	int i;

	/* saving original center */
	copy_v3_v3(center, t->center);

	for (i = 0 ; i < t->total; i++, td++)
	{
		float mat[3][3], invmat[3][3];

		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		/* around local centers */
		if (t->flag & (T_OBJECT|T_POSE)) {
			copy_v3_v3(t->center, td->center);
		}
		else {
			if (t->settings->selectmode & SCE_SELECT_FACE) {
				copy_v3_v3(t->center, td->center);
			}
		}

		invert_m3_m3(invmat, td->axismtx);

		mul_m3_m3m3(mat, t->spacemtx, invmat);

		ElementRotation(t, td, mat, t->around);
	}

	/* restoring original center */
	copy_v3_v3(t->center, center);

	recalcData(t);

	ED_area_headerprint(t->sa, "Align");

	return 1;
}

/* ************************** SEQ SLIDE *************************** */

void initSeqSlide(TransInfo *t)
{
	t->transform = SeqSlide;

	initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

	t->idx_max = 1;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	t->snap[0] = 0.0f;
	t->snap[1] = floor(t->scene->r.frs_sec / t->scene->r.frs_sec_base);
	t->snap[2] = 10.0f;

	t->num.increment = t->snap[1];
}

static void headerSeqSlide(TransInfo *t, float val[2], char *str)
{
	char tvec[60];

	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec);
	}
	else {
		sprintf(&tvec[0], "%.0f, %.0f", val[0], val[1]);
	}

	sprintf(str, "Sequence Slide: %s%s", &tvec[0], t->con.text);
}

static void applySeqSlide(TransInfo *t, float val[2])
{
	TransData *td = t->data;
	int i;

	for (i = 0 ; i < t->total; i++, td++) {
		float tvec[2];

		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		copy_v2_v2(tvec, val);

		mul_v2_fl(tvec, td->factor);

		td->loc[0] = td->iloc[0] + tvec[0];
		td->loc[1] = td->iloc[1] + tvec[1];
	}
}

int SeqSlide(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[200];

	if (t->con.mode & CON_APPLY) {
		float pvec[3] = {0.0f, 0.0f, 0.0f};
		float tvec[3];
		t->con.applyVec(t, NULL, t->values, tvec, pvec);
		copy_v3_v3(t->values, tvec);
	}
	else {
		snapGrid(t, t->values);
		applyNumInput(&t->num, t->values);
	}

	t->values[0] = floor(t->values[0] + 0.5f);
	t->values[1] = floor(t->values[1] + 0.5f);

	headerSeqSlide(t, t->values, str);
	applySeqSlide(t, t->values);

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************** ANIM EDITORS - TRANSFORM TOOLS *************************** */

/* ---------------- Special Helpers for Various Settings ------------- */


/* This function returns the snapping 'mode' for Animation Editors only
 * We cannot use the standard snapping due to NLA-strip scaling complexities.
 */
// XXX these modifier checks should be keymappable
static short getAnimEdit_SnapMode(TransInfo *t)
{
	short autosnap= SACTSNAP_OFF;
	
	if (t->spacetype == SPACE_ACTION) {
		SpaceAction *saction= (SpaceAction *)t->sa->spacedata.first;
		
		if (saction)
			autosnap= saction->autosnap;
	}
	else if (t->spacetype == SPACE_IPO) {
		SpaceIpo *sipo= (SpaceIpo *)t->sa->spacedata.first;
		
		if (sipo)
			autosnap= sipo->autosnap;
	}
	else if (t->spacetype == SPACE_NLA) {
		SpaceNla *snla= (SpaceNla *)t->sa->spacedata.first;
		
		if (snla)
			autosnap= snla->autosnap;
	}
	else {
		autosnap= SACTSNAP_OFF;
	}
	
	/* toggle autosnap on/off 
	 * 	- when toggling on, prefer nearest frame over 1.0 frame increments
	 */
	if (t->modifiers & MOD_SNAP_INVERT) {
		if (autosnap)
			autosnap= SACTSNAP_OFF;
		else
			autosnap= SACTSNAP_FRAME;
	}

	return autosnap;
}

/* This function is used for testing if an Animation Editor is displaying
 * its data in frames or seconds (and the data needing to be edited as such).
 * Returns 1 if in seconds, 0 if in frames
 */
static short getAnimEdit_DrawTime(TransInfo *t)
{
	short drawtime;

	if (t->spacetype == SPACE_ACTION) {
		SpaceAction *saction= (SpaceAction *)t->sa->spacedata.first;
		
		drawtime = (saction->flag & SACTION_DRAWTIME)? 1 : 0;
	}
	else if (t->spacetype == SPACE_NLA) {
		SpaceNla *snla= (SpaceNla *)t->sa->spacedata.first;
		
		drawtime = (snla->flag & SNLA_DRAWTIME)? 1 : 0;
	}
	else if (t->spacetype == SPACE_IPO) {
		SpaceIpo *sipo= (SpaceIpo *)t->sa->spacedata.first;
		
		drawtime = (sipo->flag & SIPO_DRAWTIME)? 1 : 0;
	}	
	else {
		drawtime = 0;
	}

	return drawtime;
}


/* This function is used by Animation Editor specific transform functions to do
 * the Snap Keyframe to Nearest Frame/Marker
 */
static void doAnimEdit_SnapFrame(TransInfo *t, TransData *td, TransData2D *td2d, AnimData *adt, short autosnap)
{
	/* snap key to nearest frame? */
	if (autosnap == SACTSNAP_FRAME) {

#if 0   /* 'doTime' disabled for now */

		const Scene *scene= t->scene;
		const short doTime= 0; //getAnimEdit_DrawTime(t); // NOTE: this works, but may be confusing behavior given the option's label, hence disabled
		const double secf= FPS;
#endif
		double val;
		
		/* convert frame to nla-action time (if needed) */
		if (adt)
			val= BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_MAP);
		else
			val= *(td->val);
		
#if 0	/* 'doTime' disabled for now */

		/* do the snapping to nearest frame/second */
		if (doTime) {
			val= (float)( floor((val/secf) + 0.5f) * secf );
		}
		else
#endif
		{
			val= floorf(val+0.5f);
		}
		
		/* convert frame out of nla-action time */
		if (adt)
			*(td->val)= BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
		else
			*(td->val)= val;
	}
	/* snap key to nearest marker? */
	else if (autosnap == SACTSNAP_MARKER) {
		float val;
		
		/* convert frame to nla-action time (if needed) */
		if (adt)
			val= BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_MAP);
		else
			val= *(td->val);
		
		/* snap to nearest marker */
		// TODO: need some more careful checks for where data comes from
		val= (float)ED_markers_find_nearest_marker_time(&t->scene->markers, val);
		
		/* convert frame out of nla-action time */
		if (adt)
			*(td->val)= BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
		else
			*(td->val)= val;
	}
	
	/* if the handles are to be moved too (as side-effect of keyframes moving, to keep the general effect) 
	 * offset them by the same amount so that the general angles are maintained (i.e. won't change while 
	 * handles are free-to-roam and keyframes are snap-locked)
	 */
	if ((td->flag & TD_MOVEHANDLE1) && td2d->h1) {
		td2d->h1[0] = td2d->ih1[0] + *td->val - td->ival;
	}
	if ((td->flag & TD_MOVEHANDLE2) && td2d->h2) {
		td2d->h2[0] = td2d->ih2[0] + *td->val - td->ival;
	}
}

/* ----------------- Translation ----------------------- */

void initTimeTranslate(TransInfo *t)
{
	/* this tool is only really available in the Action Editor... */
	if (!ELEM(t->spacetype, SPACE_ACTION, SPACE_SEQ)) {
		t->state = TRANS_CANCEL;
	}

	t->mode = TFM_TIME_TRANSLATE;
	t->transform = TimeTranslate;

	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	/* num-input has max of (n-1) */
	t->idx_max = 0;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	/* initialize snap like for everything else */
	t->snap[0] = 0.0f;
	t->snap[1] = t->snap[2] = 1.0f;

	t->num.increment = t->snap[1];
}

static void headerTimeTranslate(TransInfo *t, char *str)
{
	char tvec[60];

	/* if numeric input is active, use results from that, otherwise apply snapping to result */
	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec);
	}
	else {
		const Scene *scene = t->scene;
		const short autosnap= getAnimEdit_SnapMode(t);
		const short doTime = getAnimEdit_DrawTime(t);
		const double secf= FPS;
		float val = t->values[0];
		
		/* apply snapping + frame->seconds conversions */
		if (autosnap == SACTSNAP_STEP) {
			if (doTime)
				val= floorf((double)val/secf + 0.5f);
			else
				val= floorf(val + 0.5f);
		}
		else {
			if (doTime)
				val= (float)((double)val / secf);
		}
		
		if (autosnap == SACTSNAP_FRAME)
			sprintf(&tvec[0], "%d.00 (%.4f)", (int)val, val);
		else
			sprintf(&tvec[0], "%.4f", val);
	}

	sprintf(str, "DeltaX: %s", &tvec[0]);
}

static void applyTimeTranslate(TransInfo *t, float UNUSED(sval))
{
	TransData *td = t->data;
	TransData2D *td2d = t->data2d;
	Scene *scene = t->scene;
	int i;

	const short doTime= getAnimEdit_DrawTime(t);
	const double secf= FPS;

	const short autosnap= getAnimEdit_SnapMode(t);

	float deltax, val /* , valprev */;

	/* it doesn't matter whether we apply to t->data or t->data2d, but t->data2d is more convenient */
	for (i = 0 ; i < t->total; i++, td++, td2d++) {
		/* it is assumed that td->extra is a pointer to the AnimData,
		 * whose active action is where this keyframe comes from
		 * (this is only valid when not in NLA)
		 */
		AnimData *adt= (t->spacetype != SPACE_NLA) ? td->extra : NULL;

		/* valprev = *td->val; */ /* UNUSED */

		/* check if any need to apply nla-mapping */
		if (adt && t->spacetype != SPACE_SEQ) {
			deltax = t->values[0];

			if (autosnap == SACTSNAP_STEP) {
				if (doTime)
					deltax= (float)( floor((deltax/secf) + 0.5f) * secf );
				else
					deltax= (float)( floor(deltax + 0.5f) );
			}

			val = BKE_nla_tweakedit_remap(adt, td->ival, NLATIME_CONVERT_MAP);
			val += deltax;
			*(td->val) = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
		}
		else {
			deltax = val = t->values[0];

			if (autosnap == SACTSNAP_STEP) {
				if (doTime)
					val= (float)( floor((deltax/secf) + 0.5f) * secf );
				else
					val= (float)( floor(val + 0.5f) );
			}

			*(td->val) = td->ival + val;
		}

		/* apply nearest snapping */
		doAnimEdit_SnapFrame(t, td, td2d, adt, autosnap);
	}
}

int TimeTranslate(TransInfo *t, const int mval[2])
{
	View2D *v2d = (View2D *)t->view;
	float cval[2], sval[2];
	char str[200];

	/* calculate translation amount from mouse movement - in 'time-grid space' */
	UI_view2d_region_to_view(v2d, mval[0], mval[0], &cval[0], &cval[1]);
	UI_view2d_region_to_view(v2d, t->imval[0], t->imval[0], &sval[0], &sval[1]);

	/* we only need to calculate effect for time (applyTimeTranslate only needs that) */
	t->values[0] = cval[0] - sval[0];

	/* handle numeric-input stuff */
	t->vec[0] = t->values[0];
	applyNumInput(&t->num, &t->vec[0]);
	t->values[0] = t->vec[0];
	headerTimeTranslate(t, str);

	applyTimeTranslate(t, sval[0]);

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ----------------- Time Slide ----------------------- */

void initTimeSlide(TransInfo *t)
{
	/* this tool is only really available in the Action Editor... */
	if (t->spacetype == SPACE_ACTION) {
		SpaceAction *saction= (SpaceAction *)t->sa->spacedata.first;

		/* set flag for drawing stuff */
		saction->flag |= SACTION_MOVING;
	}
	else {
		t->state = TRANS_CANCEL;
	}


	t->mode = TFM_TIME_SLIDE;
	t->transform = TimeSlide;
	t->flag |= T_FREE_CUSTOMDATA;

	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	/* num-input has max of (n-1) */
	t->idx_max = 0;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	/* initialize snap like for everything else */
	t->snap[0] = 0.0f;
	t->snap[1] = t->snap[2] = 1.0f;

	t->num.increment = t->snap[1];
}

static void headerTimeSlide(TransInfo *t, float sval, char *str)
{
	char tvec[60];

	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec);
	}
	else {
		float minx= *((float *)(t->customData));
		float maxx= *((float *)(t->customData) + 1);
		float cval= t->values[0];
		float val;

		val= 2.0f*(cval-sval) / (maxx-minx);
		CLAMP(val, -1.0f, 1.0f);

		sprintf(&tvec[0], "%.4f", val);
	}

	sprintf(str, "TimeSlide: %s", &tvec[0]);
}

static void applyTimeSlide(TransInfo *t, float sval)
{
	TransData *td = t->data;
	int i;

	float minx= *((float *)(t->customData));
	float maxx= *((float *)(t->customData) + 1);

	/* set value for drawing black line */
	if (t->spacetype == SPACE_ACTION) {
		SpaceAction *saction= (SpaceAction *)t->sa->spacedata.first;
		float cvalf = t->values[0];

		saction->timeslide= cvalf;
	}

	/* it doesn't matter whether we apply to t->data or t->data2d, but t->data2d is more convenient */
	for (i = 0 ; i < t->total; i++, td++) {
		/* it is assumed that td->extra is a pointer to the AnimData,
		 * whose active action is where this keyframe comes from
		 * (this is only valid when not in NLA)
		 */
		AnimData *adt= (t->spacetype != SPACE_NLA) ? td->extra : NULL;
		float cval = t->values[0];

		/* apply NLA-mapping to necessary values */
		if (adt)
			cval= BKE_nla_tweakedit_remap(adt, cval, NLATIME_CONVERT_UNMAP);

		/* only apply to data if in range */
		if ((sval > minx) && (sval < maxx)) {
			float cvalc= CLAMPIS(cval, minx, maxx);
			float timefac;

			/* left half? */
			if (td->ival < sval) {
				timefac= (sval - td->ival) / (sval - minx);
				*(td->val)= cvalc - timefac * (cvalc - minx);
			}
			else {
				timefac= (td->ival - sval) / (maxx - sval);
				*(td->val)= cvalc + timefac * (maxx - cvalc);
			}
		}
	}
}

int TimeSlide(TransInfo *t, const int mval[2])
{
	View2D *v2d = (View2D *)t->view;
	float cval[2], sval[2];
	float minx= *((float *)(t->customData));
	float maxx= *((float *)(t->customData) + 1);
	char str[200];

	/* calculate mouse co-ordinates */
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &cval[0], &cval[1]);
	UI_view2d_region_to_view(v2d, t->imval[0], t->imval[1], &sval[0], &sval[1]);

	/* t->values[0] stores cval[0], which is the current mouse-pointer location (in frames) */
	// XXX Need to be able to repeat this
	t->values[0] = cval[0];

	/* handle numeric-input stuff */
	t->vec[0] = 2.0f*(cval[0]-sval[0]) / (maxx-minx);
	applyNumInput(&t->num, &t->vec[0]);
	t->values[0] = (maxx-minx) * t->vec[0] / 2.0f + sval[0];

	headerTimeSlide(t, sval[0], str);
	applyTimeSlide(t, sval[0]);

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ----------------- Scaling ----------------------- */

void initTimeScale(TransInfo *t)
{
	int center[2];

	/* this tool is only really available in the Action Editor
	 * AND NLA Editor (for strip scaling)
	 */
	if (ELEM(t->spacetype, SPACE_ACTION, SPACE_NLA) == 0) {
		t->state = TRANS_CANCEL;
	}

	t->mode = TFM_TIME_SCALE;
	t->transform = TimeScale;

	/* recalculate center2d to use CFRA and mouse Y, since that's
	 * what is used in time scale */
	t->center[0] = t->scene->r.cfra;
	projectIntView(t, t->center, center);
	center[1] = t->imval[1];

	/* force a reinit with the center2d used here */
	initMouseInput(t, &t->mouse, center, t->imval);

	initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

	t->flag |= T_NULL_ONE;
	t->num.flag |= NUM_NULL_ONE;

	/* num-input has max of (n-1) */
	t->idx_max = 0;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	/* initialize snap like for everything else */
	t->snap[0] = 0.0f;
	t->snap[1] = t->snap[2] = 1.0f;

	t->num.increment = t->snap[1];
}

static void headerTimeScale(TransInfo *t, char *str)
{
	char tvec[60];

	if (hasNumInput(&t->num))
		outputNumInput(&(t->num), tvec);
	else
		sprintf(&tvec[0], "%.4f", t->values[0]);

	sprintf(str, "ScaleX: %s", &tvec[0]);
}

static void applyTimeScale(TransInfo *t)
{
	Scene *scene = t->scene;
	TransData *td = t->data;
	TransData2D *td2d = t->data2d;
	int i;

	const short autosnap= getAnimEdit_SnapMode(t);
	const short doTime= getAnimEdit_DrawTime(t);
	const double secf= FPS;


	for (i = 0 ; i < t->total; i++, td++, td2d++) {
		/* it is assumed that td->extra is a pointer to the AnimData,
		 * whose active action is where this keyframe comes from
		 * (this is only valid when not in NLA)
		 */
		AnimData *adt= (t->spacetype != SPACE_NLA) ? td->extra : NULL;
		float startx= CFRA;
		float fac= t->values[0];

		if (autosnap == SACTSNAP_STEP) {
			if (doTime)
				fac= (float)( floor(fac/secf + 0.5f) * secf );
			else
				fac= (float)( floor(fac + 0.5f) );
		}

		/* check if any need to apply nla-mapping */
		if (adt)
			startx= BKE_nla_tweakedit_remap(adt, startx, NLATIME_CONVERT_UNMAP);

		/* now, calculate the new value */
		*(td->val) = td->ival - startx;
		*(td->val) *= fac;
		*(td->val) += startx;

		/* apply nearest snapping */
		doAnimEdit_SnapFrame(t, td, td2d, adt, autosnap);
	}
}

int TimeScale(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[200];
	
	/* handle numeric-input stuff */
	t->vec[0] = t->values[0];
	applyNumInput(&t->num, &t->vec[0]);
	t->values[0] = t->vec[0];
	headerTimeScale(t, str);

	applyTimeScale(t);

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ************************************ */

void BIF_TransformSetUndo(const char *UNUSED(str))
{
	// TRANSFORM_FIX_ME
	//Trans.undostr= str;
}
