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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_action_types.h"  /* for some special action-editor settings */
#include "DNA_constraint_types.h"
#include "DNA_ipo_types.h"		/* some silly ipo flag	*/
#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"		/* PET modes			*/
#include "DNA_screen_types.h"	/* area dimensions		*/
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"

//#include "BIF_editview.h"		/* arrows_move_cursor	*/
#include "BIF_gl.h"
#include "BIF_glutil.h"
//#include "BIF_mywindow.h"
//#include "BIF_resources.h"
//#include "BIF_screen.h"
//#include "BIF_space.h"			/* undo					*/
//#include "BIF_toets.h"			/* persptoetsen			*/
//#include "BIF_mywindow.h"		/* warp_pointer			*/
//#include "BIF_toolbox.h"			/* notice				*/
//#include "BIF_editmesh.h"
//#include "BIF_editsima.h"
//#include "BIF_editparticle.h"

#include "BKE_action.h"
#include "BKE_nla.h"
//#include "BKE_bad_level_calls.h"/* popmenu and error	*/
#include "BKE_bmesh.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_global.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_utildefines.h"
#include "BKE_context.h"
#include "BKE_unit.h"

//#include "BSE_view.h"

#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_markers.h"
#include "ED_util.h"
#include "ED_view3d.h"
#include "ED_mesh.h"

#include "UI_view2d.h"
#include "WM_types.h"
#include "WM_api.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"

#include "PIL_time.h"			/* sleep				*/

#include "UI_resources.h"

//#include "blendef.h"
//
//#include "mydevice.h"

#include "transform.h"

/* ************************** SPACE DEPENDANT CODE **************************** */

void setTransformViewMatrices(TransInfo *t)
{
	if(t->spacetype==SPACE_VIEW3D && t->ar->regiontype == RGN_TYPE_WINDOW) {
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

void convertViewVec(TransInfo *t, float *vec, short dx, short dy)
{
	if (t->spacetype==SPACE_VIEW3D) {
		if (t->ar->regiontype == RGN_TYPE_WINDOW)
		{
			window_to_3d_delta(t->ar, vec, dx, dy);
		}
	}
	else if(t->spacetype==SPACE_IMAGE) {
		View2D *v2d = t->view;
		float divx, divy, aspx, aspy;

		ED_space_image_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);

		divx= v2d->mask.xmax-v2d->mask.xmin;
		divy= v2d->mask.ymax-v2d->mask.ymin;

		vec[0]= aspx*(v2d->cur.xmax-v2d->cur.xmin)*(dx)/divx;
		vec[1]= aspy*(v2d->cur.ymax-v2d->cur.ymin)*(dy)/divy;
		vec[2]= 0.0f;
	}
	else if(ELEM(t->spacetype, SPACE_IPO, SPACE_NLA)) {
		View2D *v2d = t->view;
		float divx, divy;

		divx= v2d->mask.xmax-v2d->mask.xmin;
		divy= v2d->mask.ymax-v2d->mask.ymin;

		vec[0]= (v2d->cur.xmax-v2d->cur.xmin)*(dx) / (divx);
		vec[1]= (v2d->cur.ymax-v2d->cur.ymin)*(dy) / (divy);
		vec[2]= 0.0f;
	}
	else if(t->spacetype==SPACE_NODE) {
		View2D *v2d = &t->ar->v2d;
		float divx, divy;

		divx= v2d->mask.xmax-v2d->mask.xmin;
		divy= v2d->mask.ymax-v2d->mask.ymin;

		vec[0]= (v2d->cur.xmax-v2d->cur.xmin)*(dx)/divx;
		vec[1]= (v2d->cur.ymax-v2d->cur.ymin)*(dy)/divy;
		vec[2]= 0.0f;
	}
	else if(t->spacetype==SPACE_SEQ) {
		View2D *v2d = &t->ar->v2d;
		float divx, divy;

		divx= v2d->mask.xmax-v2d->mask.xmin;
		divy= v2d->mask.ymax-v2d->mask.ymin;

		vec[0]= (v2d->cur.xmax-v2d->cur.xmin)*(dx)/divx;
		vec[1]= (v2d->cur.ymax-v2d->cur.ymin)*(dy)/divy;
		vec[2]= 0.0f;
	}
}

void projectIntView(TransInfo *t, float *vec, int *adr)
{
	if (t->spacetype==SPACE_VIEW3D) {
		if(t->ar->regiontype == RGN_TYPE_WINDOW)
			project_int_noclip(t->ar, vec, adr);
	}
	else if(t->spacetype==SPACE_IMAGE) {
		float aspx, aspy, v[2];

		ED_space_image_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);
		v[0]= vec[0]/aspx;
		v[1]= vec[1]/aspy;

		UI_view2d_to_region_no_clip(t->view, v[0], v[1], adr, adr+1);
	}
	else if(ELEM(t->spacetype, SPACE_IPO, SPACE_NLA)) {
		int out[2] = {0, 0};

		UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], out, out+1);
		adr[0]= out[0];
		adr[1]= out[1];
	}
	else if(t->spacetype==SPACE_SEQ) { /* XXX not tested yet, but should work */
		int out[2] = {0, 0};

		UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], out, out+1);
		adr[0]= out[0];
		adr[1]= out[1];
	}
}

void projectFloatView(TransInfo *t, float *vec, float *adr)
{
	if (t->spacetype==SPACE_VIEW3D) {
		if(t->ar->regiontype == RGN_TYPE_WINDOW)
			project_float_noclip(t->ar, vec, adr);
	}
	else if(t->spacetype==SPACE_IMAGE) {
		int a[2];

		projectIntView(t, vec, a);
		adr[0]= a[0];
		adr[1]= a[1];
	}
	else if(ELEM(t->spacetype, SPACE_IPO, SPACE_NLA)) {
		int a[2];

		projectIntView(t, vec, a);
		adr[0]= a[0];
		adr[1]= a[1];
	}
}

void applyAspectRatio(TransInfo *t, float *vec)
{
	SpaceImage *sima= t->sa->spacedata.first;

	if ((t->spacetype==SPACE_IMAGE) && (t->mode==TFM_TRANSLATION)) {
		float aspx, aspy;

		if((sima->flag & SI_COORDFLOATS)==0) {
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
	SpaceImage *sima= t->sa->spacedata.first;

	if ((t->spacetype==SPACE_IMAGE) && (t->mode==TFM_TRANSLATION)) {
		float aspx, aspy;

		if((sima->flag & SI_COORDFLOATS)==0) {
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

static void viewRedrawForce(bContext *C, TransInfo *t)
{
	if (t->spacetype == SPACE_VIEW3D)
	{
		/* Do we need more refined tags? */
		WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
		
		/* for realtime animation record - send notifiers recognised by animation editors */
		if ((t->animtimer) && IS_AUTOKEY_ON(t->scene))
			WM_event_add_notifier(C, NC_OBJECT|ND_KEYS, NULL);
	}
	else if (t->spacetype == SPACE_ACTION) {
		//SpaceAction *saction= (SpaceAction *)t->sa->spacedata.first;
		WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	}
	else if (t->spacetype == SPACE_IPO) {
		//SpaceIpo *sipo= (SpaceIpo *)t->sa->spacedata.first;
		WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	}
	else if (t->spacetype == SPACE_NLA) {
		WM_event_add_notifier(C, NC_ANIMATION|ND_NLA_EDIT, NULL);
	}
	else if(t->spacetype == SPACE_NODE)
	{
		//ED_area_tag_redraw(t->sa);
		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_NODE_VIEW, NULL);
	}
	else if(t->spacetype == SPACE_SEQ)
	{
		WM_event_add_notifier(C, NC_SCENE|ND_SEQUENCER, NULL);
	}
	else if (t->spacetype==SPACE_IMAGE) {
		// XXX how to deal with lock?
		SpaceImage *sima= (SpaceImage*)t->sa->spacedata.first;
		if(sima->lock) WM_event_add_notifier(C, NC_GEOM|ND_DATA, t->obedit->data);
		else ED_area_tag_redraw(t->sa);
	}
}

static void viewRedrawPost(TransInfo *t)
{
	ED_area_headerprint(t->sa, NULL);

#if 0 // TRANSFORM_FIX_ME
	if(t->spacetype==SPACE_VIEW3D) {
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
	else if(t->spacetype==SPACE_IMAGE) {
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
	else if(ELEM3(t->spacetype, SPACE_ACTION, SPACE_NLA, SPACE_IPO)) {
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

void BIF_selectOrientation() {
#if 0 // TRANSFORM_FIX_ME
	short val;
	char *str_menu = BIF_menustringTransformOrientation("Orientation");
	val= pupmenu(str_menu);
	MEM_freeN(str_menu);

	if(val >= 0) {
		G.vd->twmode = val;
	}
#endif
}

static void view_editmove(unsigned short event)
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

			if( G.qual & LR_SHIFTKEY ) {
				if( G.qual & LR_ALTKEY ) {
					G.qual &= ~LR_SHIFTKEY;
					persptoetsen(PAD2);
					G.qual |= LR_SHIFTKEY;
				} else {
					persptoetsen(PAD2);
				}
			} else if( G.qual & LR_CTRLKEY ) {
				if( G.qual & LR_ALTKEY ) {
					G.qual &= ~LR_CTRLKEY;
					persptoetsen(PAD4);
					G.qual |= LR_CTRLKEY;
				} else {
					persptoetsen(PAD4);
				}
			} else if(U.uiflag & USER_WHEELZOOMDIR)
				persptoetsen(PADMINUS);
			else
				persptoetsen(PADPLUSKEY);

			refresh = 1;
			break;
		case WHEELDOWNMOUSE:
			if( G.qual & LR_SHIFTKEY ) {
				if( G.qual & LR_ALTKEY ) {
					G.qual &= ~LR_SHIFTKEY;
					persptoetsen(PAD8);
					G.qual |= LR_SHIFTKEY;
				} else {
					persptoetsen(PAD8);
				}
			} else if( G.qual & LR_CTRLKEY ) {
				if( G.qual & LR_ALTKEY ) {
					G.qual &= ~LR_CTRLKEY;
					persptoetsen(PAD6);
					G.qual |= LR_CTRLKEY;
				} else {
					persptoetsen(PAD6);
				}
			} else if(U.uiflag & USER_WHEELZOOMDIR)
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

#if 0
static char *transform_to_undostr(TransInfo *t)
{
	switch (t->mode) {
		case TFM_TRANSLATION:
			return "Translate";
		case TFM_ROTATION:
			return "Rotate";
		case TFM_RESIZE:
			return "Scale";
		case TFM_TOSPHERE:
			return "To Sphere";
		case TFM_SHEAR:
			return "Shear";
		case TFM_WARP:
			return "Warp";
		case TFM_SHRINKFATTEN:
			return "Shrink/Fatten";
		case TFM_TILT:
			return "Tilt";
		case TFM_TRACKBALL:
			return "Trackball";
		case TFM_PUSHPULL:
			return "Push/Pull";
		case TFM_BEVEL:
			return "Bevel";
		case TFM_BWEIGHT:
			return "Bevel Weight";
		case TFM_CREASE:
			return "Crease";
		case TFM_BONESIZE:
			return "Bone Width";
		case TFM_BONE_ENVELOPE:
			return "Bone Envelope";
		case TFM_TIME_TRANSLATE:
			return "Translate Anim. Data";
		case TFM_TIME_SCALE:
			return "Scale Anim. Data";
		case TFM_TIME_SLIDE:
			return "Time Slide";
		case TFM_BAKE_TIME:
			return "Key Time";
		case TFM_MIRROR:
			return "Mirror";
	}
	return "Transform";
}
#endif

/* ************************************************* */

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
#define TFM_MODAL_CANCEL		1
#define TFM_MODAL_CONFIRM		2
#define TFM_MODAL_TRANSLATE		3
#define TFM_MODAL_ROTATE		4
#define TFM_MODAL_RESIZE		5
#define TFM_MODAL_SNAP_ON		6
#define TFM_MODAL_SNAP_OFF		7
#define TFM_MODAL_SNAP_TOGGLE	8
#define TFM_MODAL_AXIS_X		9
#define TFM_MODAL_AXIS_Y		10
#define TFM_MODAL_AXIS_Z		11
#define TFM_MODAL_PLANE_X		12
#define TFM_MODAL_PLANE_Y		13
#define TFM_MODAL_PLANE_Z		14
#define TFM_MODAL_CONS_OFF		15

/* called in transform_ops.c, on each regeneration of keymaps */
void transform_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
	{TFM_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
	{TFM_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
	{TFM_MODAL_TRANSLATE, "TRANSLATE", 0, "Translate", ""},
	{TFM_MODAL_ROTATE, "ROTATE", 0, "Rotate", ""},
	{TFM_MODAL_RESIZE, "RESIZE", 0, "Resize", ""},
	{TFM_MODAL_SNAP_ON, "SNAP_ON", 0, "Snap On", ""},
	{TFM_MODAL_SNAP_OFF, "SNAP_OFF", 0, "Snap Off", ""},
	{TFM_MODAL_SNAP_TOGGLE, "SNAP_TOGGLE", 0, "Snap Toggle", ""},
	{TFM_MODAL_AXIS_X, "AXIS_X", 0, "Orientation X axis", ""},
	{TFM_MODAL_AXIS_Y, "AXIS_Y", 0, "Orientation Y axis", ""},
	{TFM_MODAL_AXIS_Z, "AXIS_Z", 0, "Orientation Z axis", ""},
	{TFM_MODAL_PLANE_X, "PLANE_X", 0, "Orientation X plane", ""},
	{TFM_MODAL_PLANE_Y, "PLANE_Y", 0, "Orientation Y plane", ""},
	{TFM_MODAL_PLANE_Z, "PLANE_Z", 0, "Orientation Z plane", ""},
	{TFM_MODAL_CONS_OFF, "CONS_OFF", 0, "Remove Constraints", ""},
	{0, NULL, 0, NULL, NULL}};
	
	wmKeyMap *keymap= WM_modalkeymap_get(keyconf, "Transform Modal Map");
	
	/* this function is called for each spacetype, only needs to add map once */
	if(keymap) return;
	
	keymap= WM_modalkeymap_add(keyconf, "Transform Modal Map", modal_items);
	
	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, TFM_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, TFM_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, TFM_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, TFM_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, GKEY, KM_PRESS, 0, 0, TFM_MODAL_TRANSLATE);
	WM_modalkeymap_add_item(keymap, RKEY, KM_PRESS, 0, 0, TFM_MODAL_ROTATE);
	WM_modalkeymap_add_item(keymap, SKEY, KM_PRESS, 0, 0, TFM_MODAL_RESIZE);
	
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_CLICK, KM_ANY, 0, TFM_MODAL_SNAP_TOGGLE);
	
	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "TFM_OT_transform");
	WM_modalkeymap_assign(keymap, "TFM_OT_translate");
	WM_modalkeymap_assign(keymap, "TFM_OT_rotate");
	WM_modalkeymap_assign(keymap, "TFM_OT_tosphere");
	WM_modalkeymap_assign(keymap, "TFM_OT_resize");
	WM_modalkeymap_assign(keymap, "TFM_OT_shear");
	WM_modalkeymap_assign(keymap, "TFM_OT_warp");
	WM_modalkeymap_assign(keymap, "TFM_OT_shrink_fatten");
	WM_modalkeymap_assign(keymap, "TFM_OT_tilt");
	WM_modalkeymap_assign(keymap, "TFM_OT_trackball");
	WM_modalkeymap_assign(keymap, "TFM_OT_mirror");
	WM_modalkeymap_assign(keymap, "TFM_OT_edge_slide");
}


int transformEvent(TransInfo *t, wmEvent *event)
{
	float mati[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
	char cmode = constraintModeToChar(t);
	int handled = 1;

	t->redraw |= handleMouseInput(t, &t->mouse, event);

	if (event->type == MOUSEMOVE)
	{
		t->mval[0] = event->x - t->ar->winrct.xmin;
		t->mval[1] = event->y - t->ar->winrct.ymin;

		t->redraw = 1;

		if (t->state == TRANS_STARTING) {
		    t->state = TRANS_RUNNING;
		}

		applyMouseInput(t, &t->mouse, t->mval, t->values);
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
				if( ELEM3(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL) ) {
					resetTransRestrictions(t);
					restoreTransObjects(t);
					initTranslation(t);
					initSnapping(t, NULL); // need to reinit after mode change
					t->redraw = 1;
				}
				break;
			case TFM_MODAL_ROTATE:
				/* only switch when... */
				if( ELEM4(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL, TFM_TRANSLATION) ) {
					
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
					t->redraw = 1;
				}
				break;
			case TFM_MODAL_RESIZE:
				/* only switch when... */
				if( ELEM3(t->mode, TFM_ROTATION, TFM_TRANSLATION, TFM_TRACKBALL) ) {
					resetTransRestrictions(t);
					restoreTransObjects(t);
					initResize(t);
					initSnapping(t, NULL); // need to reinit after mode change
					t->redraw = 1;
				}
				break;
				
			case TFM_MODAL_SNAP_ON:
				t->modifiers |= MOD_SNAP;
				t->redraw = 1;
				break;
			case TFM_MODAL_SNAP_OFF:
				t->modifiers &= ~MOD_SNAP;
				t->redraw = 1;
				break;
			case TFM_MODAL_SNAP_TOGGLE:
				t->modifiers ^= MOD_SNAP;
				t->redraw = 1;
				break;
			case TFM_MODAL_AXIS_X:
				if ((t->flag & T_NO_CONSTRAINT)==0) {
					if (cmode == 'X') {
						stopConstraint(t);
					}
					else {
						if (t->flag & T_2D_EDIT) {
							setConstraint(t, mati, (CON_AXIS0), "along X axis");
						}
						else {
							setUserConstraint(t, t->current_orientation, (CON_AXIS0), "along %s X");
						}
					}
					t->redraw = 1;
				}
				break;
			case TFM_MODAL_AXIS_Y:
				if ((t->flag & T_NO_CONSTRAINT)==0) {
					if (cmode == 'Y') {
						stopConstraint(t);
					}
					else {
						if (t->flag & T_2D_EDIT) {
							setConstraint(t, mati, (CON_AXIS1), "along Y axis");
						}
						else {
							setUserConstraint(t, t->current_orientation, (CON_AXIS1), "along %s Y");
						}
					}
					t->redraw = 1;
				}
				break;
			case TFM_MODAL_AXIS_Z:
				if ((t->flag & T_NO_CONSTRAINT)==0) {
					if (cmode == 'Z') {
						stopConstraint(t);
					}
					else {
						if (t->flag & T_2D_EDIT) {
							setConstraint(t, mati, (CON_AXIS0), "along Z axis");
						}
						else {
							setUserConstraint(t, t->current_orientation, (CON_AXIS2), "along %s Z");
						}
					}
					t->redraw = 1;
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
					t->redraw = 1;
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
					t->redraw = 1;
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
					t->redraw = 1;
				}
				break;
			case TFM_MODAL_CONS_OFF:
				if ((t->flag & T_NO_CONSTRAINT)==0) {
					stopConstraint(t);
					t->redraw = 1;
				}
				break;
			default:
				handled = 0;
				break;
		}
	}
	/* else do non-mapped events */
	else if (event->val==KM_PRESS) {
		switch (event->type){
		case RIGHTMOUSE:
			t->state = TRANS_CANCEL;
			break;
		/* enforce redraw of transform when modifiers are used */
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			t->modifiers |= MOD_CONSTRAINT_PLANE;
			t->redraw = 1;
			break;

		case SPACEKEY:
			if ((t->spacetype==SPACE_VIEW3D) && event->alt) {
#if 0 // TRANSFORM_FIX_ME
				short mval[2];

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
				t->redraw = 1;
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
			if( ELEM3(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL) ) {
				resetTransRestrictions(t);
				restoreTransObjects(t);
				initTranslation(t);
				initSnapping(t, NULL); // need to reinit after mode change
				t->redraw = 1;
			}
			break;
		case SKEY:
			/* only switch when... */
			if( ELEM3(t->mode, TFM_ROTATION, TFM_TRANSLATION, TFM_TRACKBALL) ) {
				resetTransRestrictions(t);
				restoreTransObjects(t);
				initResize(t);
				initSnapping(t, NULL); // need to reinit after mode change
				t->redraw = 1;
			}
			break;
		case RKEY:
			/* only switch when... */
			if( ELEM4(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL, TFM_TRANSLATION) ) {

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
				t->redraw = 1;
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
				t->redraw = 1;
			}
			break;
		case XKEY:
			if ((t->flag & T_NO_CONSTRAINT)==0) {
				if (cmode == 'X') {
					if (t->flag & T_2D_EDIT) {
						stopConstraint(t);
					}
					else {
						if (t->con.mode & CON_USER) {
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
				}
				else {
					if (t->flag & T_2D_EDIT) {
						setConstraint(t, mati, (CON_AXIS0), "along X axis");
					}
					else {
						if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
							setConstraint(t, mati, (CON_AXIS0), "along global X");
						else if (t->modifiers & MOD_CONSTRAINT_PLANE)
							setConstraint(t, mati, (CON_AXIS1|CON_AXIS2), "locking global X");
					}
				}
				t->redraw = 1;
			}
			break;
		case YKEY:
			if ((t->flag & T_NO_CONSTRAINT)==0) {
				if (cmode == 'Y') {
					if (t->flag & T_2D_EDIT) {
						stopConstraint(t);
					}
					else {
						if (t->con.mode & CON_USER) {
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
				}
				else {
					if (t->flag & T_2D_EDIT) {
						setConstraint(t, mati, (CON_AXIS1), "along Y axis");
					}
					else {
						if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
							setConstraint(t, mati, (CON_AXIS1), "along global Y");
						else if (t->modifiers & MOD_CONSTRAINT_PLANE)
							setConstraint(t, mati, (CON_AXIS0|CON_AXIS2), "locking global Y");
					}
				}
				t->redraw = 1;
			}
			break;
		case ZKEY:
			if ((t->flag & T_NO_CONSTRAINT)==0) {
				if (cmode == 'Z') {
					if (t->con.mode & CON_USER) {
						stopConstraint(t);
					}
					else {
						short orientation = t->current_orientation != V3D_MANIP_GLOBAL ? t->current_orientation : V3D_MANIP_LOCAL;
						if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
							setUserConstraint(t, orientation, (CON_AXIS2), "along %s Z");
						else if ((t->modifiers & MOD_CONSTRAINT_PLANE) && ((t->flag & T_2D_EDIT)==0))
							setUserConstraint(t, orientation, (CON_AXIS0|CON_AXIS1), "locking %s Z");
					}
				}
				else if ((t->flag & T_2D_EDIT)==0) {
					if ((t->modifiers & MOD_CONSTRAINT_PLANE) == 0)
						setConstraint(t, mati, (CON_AXIS2), "along global Z");
					else if (t->modifiers & MOD_CONSTRAINT_PLANE)
						setConstraint(t, mati, (CON_AXIS0|CON_AXIS1), "locking global Z");
				}
				t->redraw = 1;
			}
			break;
		case OKEY:
			if (t->flag & T_PROP_EDIT && event->shift) {
				t->prop_mode = (t->prop_mode + 1) % 6;
				calculatePropRatio(t);
				t->redraw = 1;
			}
			break;
		case PADPLUSKEY:
			if(event->alt && t->flag & T_PROP_EDIT) {
				t->prop_size *= 1.1f;
				calculatePropRatio(t);
			}
			t->redraw= 1;
			break;
		case PAGEUPKEY:
		case WHEELDOWNMOUSE:
			if (t->flag & T_AUTOIK) {
				transform_autoik_update(t, 1);
			}
			else if(t->flag & T_PROP_EDIT) {
				t->prop_size*= 1.1f;
				calculatePropRatio(t);
			}
			else view_editmove(event->type);
			t->redraw= 1;
			break;
		case PADMINUS:
			if(event->alt && t->flag & T_PROP_EDIT) {
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
			else if (t->flag & T_PROP_EDIT) {
				t->prop_size*= 0.90909090f;
				calculatePropRatio(t);
			}
			else view_editmove(event->type);
			t->redraw= 1;
			break;
//		case NDOFMOTION:
//            viewmoveNDOF(1);
  //         break;
		default:
			handled = 0;
			break;
		}

		// Numerical input events
		t->redraw |= handleNumInput(&(t->num), event);

		// NDof input events
		switch(handleNDofInput(&(t->ndof), event))
		{
			case NDOF_CONFIRM:
				if ((t->options & CTX_NDOF) == 0)
				{
					/* Confirm on normal transform only */
					t->state = TRANS_CONFIRM;
				}
				break;
			case NDOF_CANCEL:
				if (t->options & CTX_NDOF)
				{
					/* Cancel on pure NDOF transform */
					t->state = TRANS_CANCEL;
				}
				else
				{
					/* Otherwise, just redraw, NDof input was cancelled */
					t->redraw = 1;
				}
				break;
			case NDOF_NOMOVE:
				if (t->options & CTX_NDOF)
				{
					/* Confirm on pure NDOF transform */
					t->state = TRANS_CONFIRM;
				}
				break;
			case NDOF_REFRESH:
				t->redraw = 1;
				break;
			default:
				handled = 0;
				break;
		}

		// Snapping events
		t->redraw |= handleSnapping(t, event);

		//arrows_move_cursor(event->type);
	}
	else if (event->val==KM_RELEASE) {
		switch (event->type){
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			t->modifiers &= ~MOD_CONSTRAINT_PLANE;
			t->redraw = 1;
			break;

		case MIDDLEMOUSE:
			if ((t->flag & T_NO_CONSTRAINT)==0) {
				t->modifiers &= ~MOD_CONSTRAINT_SELECT;
				postSelectConstraint(t);
				t->redraw = 1;
			}
			break;
//		case LEFTMOUSE:
//		case RIGHTMOUSE:
//			if(WM_modal_tweak_exit(event, t->event_type))
////			if (t->options & CTX_TWEAK)
//				t->state = TRANS_CONFIRM;
//			break;
		default:
			handled = 0;
			break;
		}

		/* confirm transform if launch key is released after mouse move */
		/* XXX Keyrepeat bug in Xorg fucks this up, will test when fixed */
		if (event->type == LEFTMOUSE /*t->launch_event*/ && t->state != TRANS_STARTING)
		{
			t->state = TRANS_CONFIRM;
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

int calculateTransformCenter(bContext *C, wmEvent *event, int centerMode, float *vec)
{
	TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data");
	int success = 1;

	t->state = TRANS_RUNNING;

	t->options = CTX_NONE;

	t->mode = TFM_DUMMY;

	initTransInfo(C, t, NULL, event);					// internal data, mouse, vectors

	createTransData(C, t);			// make TransData structs from selection

	t->around = centerMode; 			// override userdefined mode

	if (t->total == 0) {
		success = 0;
	}
	else {
		success = 1;

		calculateCenter(t);

		// Copy center from constraint center. Transform center can be local
		VECCOPY(vec, t->con.center);
	}


	/* aftertrans does insert ipos and action channels, and clears base flags, doesnt read transdata */
	special_aftertrans_update(t);

	postTrans(t);

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

	for( angle = angle_start; angle < angle_end; angle += delta)
	{
		glVertex2f( cosf(angle) * size, sinf(angle) * size);
	}
	glVertex2f( cosf(angle_end) * size, sinf(angle_end) * size);

	glEnd();
}

void drawHelpline(const struct bContext *C, TransInfo *t)
{
	if (t->helpline != HLP_NONE && !(t->flag & T_USES_MANIPULATOR))
	{
		float vecrot[3], cent[2];

		VECCOPY(vecrot, t->center);
		if(t->flag & T_EDIT) {
			Object *ob= t->obedit;
			if(ob) mul_m4_v3(ob->obmat, vecrot);
		}
		else if(t->flag & T_POSE) {
			Object *ob=t->poseobj;
			if(ob) mul_m4_v3(ob->obmat, vecrot);
		}

		projectFloatView(t, vecrot, cent);	// no overflow in extreme cases

		glPushMatrix();

		switch(t->helpline)
		{
			case HLP_SPRING:
				UI_ThemeColor(TH_WIRE);

				setlinestyle(3);
				glBegin(GL_LINE_STRIP);
				glVertex2sv(t->mval);
				glVertex2fv(cent);
				glEnd();

				glTranslatef(t->mval[0], t->mval[1], 0);
				glRotatef(-180 / M_PI * atan2f(cent[0] - t->mval[0], cent[1] - t->mval[1]), 0, 0, 1);

				setlinestyle(0);
				glLineWidth(3.0);
				drawArrow(UP, 5, 10, 5);
				drawArrow(DOWN, 5, 10, 5);
				glLineWidth(1.0);
				break;
			case HLP_HARROW:
				UI_ThemeColor(TH_WIRE);

				glTranslatef(t->mval[0], t->mval[1], 0);

				glLineWidth(3.0);
				drawArrow(RIGHT, 5, 10, 5);
				drawArrow(LEFT, 5, 10, 5);
				glLineWidth(1.0);
				break;
			case HLP_VARROW:
				UI_ThemeColor(TH_WIRE);

				glTranslatef(t->mval[0], t->mval[1], 0);

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
					float delta_angle = MIN2(15 / dist, M_PI/4);
					float spacing_angle = MIN2(5 / dist, M_PI/12);
					UI_ThemeColor(TH_WIRE);

					setlinestyle(3);
					glBegin(GL_LINE_STRIP);
					glVertex2sv(t->mval);
					glVertex2fv(cent);
					glEnd();

					glTranslatef(cent[0], cent[1], 0);

					setlinestyle(0);
					glLineWidth(3.0);
					drawArc(dist, angle - delta_angle, angle - spacing_angle, 10);
					drawArc(dist, angle + spacing_angle, angle + delta_angle, 10);

					glPushMatrix();

					glTranslatef(cosf(angle - delta_angle) * dist, sinf(angle - delta_angle) * dist, 0);
					glRotatef(180 / M_PI * (angle - delta_angle), 0, 0, 1);

					drawArrowHead(DOWN, 5);

					glPopMatrix();

					glTranslatef(cosf(angle + delta_angle) * dist, sinf(angle + delta_angle) * dist, 0);
					glRotatef(180 / M_PI * (angle + delta_angle), 0, 0, 1);

					drawArrowHead(UP, 5);

					glLineWidth(1.0);
					break;
				}
				case HLP_TRACKBALL:
				{
					char col[3], col2[3];
					UI_GetThemeColor3ubv(TH_GRID, col);

					glTranslatef(t->mval[0], t->mval[1], 0);

					glLineWidth(3.0);

					UI_make_axis_color(col, col2, 'x');
					glColor3ubv((GLubyte *)col2);

					drawArrow(RIGHT, 5, 10, 5);
					drawArrow(LEFT, 5, 10, 5);

					UI_make_axis_color(col, col2, 'y');
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

void drawTransformView(const struct bContext *C, struct ARegion *ar, void *arg)
{
	TransInfo *t = arg;

	drawConstraint(C, t);
	drawPropCircle(C, t);
	drawSnapping(C, t);
}

void drawTransformPixel(const struct bContext *C, struct ARegion *ar, void *arg)
{
	TransInfo *t = arg;

	drawHelpline(C, t);
}

void saveTransform(bContext *C, TransInfo *t, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	int constraint_axis[3] = {0, 0, 0};
	int proportional = 0;

	if (RNA_struct_find_property(op->ptr, "value"))
	{
		if (t->flag & T_AUTOVALUES)
		{
			RNA_float_set_array(op->ptr, "value", t->auto_values);
		}
		else
		{
			RNA_float_set_array(op->ptr, "value", t->values);
		}
	}

	/* XXX convert stupid flag to enum */
	switch(t->flag & (T_PROP_EDIT|T_PROP_CONNECTED))
	{
	case (T_PROP_EDIT|T_PROP_CONNECTED):
		proportional = 2;
		break;
	case T_PROP_EDIT:
		proportional = 1;
		break;
	default:
		proportional = 0;
	}

	// If modal, save settings back in scene if not set as operator argument
	if (t->flag & T_MODAL) {

		/* save settings if not set in operator */
		if (RNA_struct_find_property(op->ptr, "proportional") && !RNA_property_is_set(op->ptr, "proportional")) {
			ts->proportional = proportional;
		}

		if (RNA_struct_find_property(op->ptr, "proportional_size") && !RNA_property_is_set(op->ptr, "proportional_size")) {
			ts->proportional_size = t->prop_size;
		}
			
		if (RNA_struct_find_property(op->ptr, "proportional_editing_falloff") && !RNA_property_is_set(op->ptr, "proportional_editing_falloff")) {
			ts->prop_mode = t->prop_mode;
		}
		
		/* do we check for parameter? */
		if (t->modifiers & MOD_SNAP) {
			ts->snap_flag |= SCE_SNAP;
		} else {
			ts->snap_flag &= ~SCE_SNAP;
		}

		if(t->spacetype == SPACE_VIEW3D) {
			if (RNA_struct_find_property(op->ptr, "constraint_orientation") && !RNA_property_is_set(op->ptr, "constraint_orientation")) {
				View3D *v3d = t->view;
	
				v3d->twmode = t->current_orientation;
			}
		}
	}
	
	if (RNA_struct_find_property(op->ptr, "proportional"))
	{
		RNA_enum_set(op->ptr, "proportional", proportional);
		RNA_enum_set(op->ptr, "proportional_editing_falloff", t->prop_mode);
		RNA_float_set(op->ptr, "proportional_size", t->prop_size);
	}

	if (RNA_struct_find_property(op->ptr, "mirror"))
	{
		RNA_boolean_set(op->ptr, "mirror", t->flag & T_MIRROR);
	}

	if (RNA_struct_find_property(op->ptr, "constraint_axis"))
	{
		RNA_enum_set(op->ptr, "constraint_orientation", t->current_orientation);

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

		RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
	}
}

int initTransform(bContext *C, TransInfo *t, wmOperator *op, wmEvent *event, int mode)
{
	int options = 0;

	/* added initialize, for external calls to set stuff in TransInfo, like undo string */

	t->state = TRANS_STARTING;

	t->options = options;

	t->mode = mode;

	t->launch_event = event ? event->type : -1;

	if (!initTransInfo(C, t, op, event))					// internal data, mouse, vectors
	{
		return 0;
	}

	if(t->spacetype == SPACE_VIEW3D)
	{
		//calc_manipulator_stats(curarea);
		initTransformOrientation(C, t);

		t->draw_handle_view = ED_region_draw_cb_activate(t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
		t->draw_handle_pixel = ED_region_draw_cb_activate(t->ar->type, drawTransformPixel, t, REGION_DRAW_POST_PIXEL);
	}
	else if(t->spacetype == SPACE_IMAGE) {
		unit_m3(t->spacemtx);
		t->draw_handle_view = ED_region_draw_cb_activate(t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
		t->draw_handle_pixel = ED_region_draw_cb_activate(t->ar->type, drawTransformPixel, t, REGION_DRAW_POST_PIXEL);
	}
	else
		unit_m3(t->spacemtx);

	createTransData(C, t);			// make TransData structs from selection

	if (t->total == 0) {
		postTrans(t);
		return 0;
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
			if(arm->drawtype==ARM_ENVELOPE)
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
	case TFM_TIME_EXTEND:
		/* now that transdata has been made, do like for TFM_TIME_TRANSLATE (for most Animation
		 * Editors because they have only 1D transforms for time values) or TFM_TRANSLATION
		 * (for Graph/NLA Editors only since they uses 'standard' transforms to get 2D movement)
		 * depending on which editor this was called from
		 */
		if ELEM(t->spacetype, SPACE_IPO, SPACE_NLA)
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
	}

	/* overwrite initial values if operator supplied a non-null vector */
	if (RNA_property_is_set(op->ptr, "value"))
	{
		float values[4];
		RNA_float_get_array(op->ptr, "value", values);
		QUATCOPY(t->values, values);
		QUATCOPY(t->auto_values, values);
		t->flag |= T_AUTOVALUES;
	}

	/* Constraint init from operator */
	if (RNA_struct_find_property(op->ptr, "constraint_axis") && RNA_property_is_set(op->ptr, "constraint_axis"))
	{
		int constraint_axis[3];

		RNA_boolean_get_array(op->ptr, "constraint_axis", constraint_axis);

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

	return 1;
}

void transformApply(bContext *C, TransInfo *t)
{
	if (t->redraw)
	{
		if (t->modifiers & MOD_CONSTRAINT_SELECT)
			t->con.mode |= CON_SELECT;

		selectConstraint(t);
		if (t->transform) {
			t->transform(t, t->mval);  // calls recalcData()
			viewRedrawForce(C, t);
		}
		t->redraw = 0;
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
		t->redraw = 1;
	}
}

int transformEnd(bContext *C, TransInfo *t)
{
	int exit_code = OPERATOR_RUNNING_MODAL;

	if (t->state != TRANS_STARTING && t->state != TRANS_RUNNING)
	{
		/* handle restoring objects */
		if(t->state == TRANS_CANCEL)
		{
			exit_code = OPERATOR_CANCELLED;
			restoreTransObjects(t);	// calls recalcData()
		}
		else
		{
			exit_code = OPERATOR_FINISHED;
		}

		/* aftertrans does insert keyframes, and clears base flags, doesnt read transdata */
		special_aftertrans_update(t);

		/* free data */
		postTrans(t);

		/* send events out for redraws */
		viewRedrawPost(t);

		/*  Undo as last, certainly after special_trans_update! */

		if(t->state == TRANS_CANCEL) {
//			if(t->undostr) ED_undo_push(C, t->undostr);
		}
		else {
//			if(t->undostr) ED_undo_push(C, t->undostr);
//			else ED_undo_push(C, transform_to_undostr(t));
		}
		t->undostr= NULL;

		viewRedrawForce(C, t);
	}

	return exit_code;
}

/* ************************** TRANSFORM LOCKS **************************** */

static void protectedTransBits(short protectflag, float *vec)
{
	if(protectflag & OB_LOCK_LOCX)
		vec[0]= 0.0f;
	if(protectflag & OB_LOCK_LOCY)
		vec[1]= 0.0f;
	if(protectflag & OB_LOCK_LOCZ)
		vec[2]= 0.0f;
}

static void protectedSizeBits(short protectflag, float *size)
{
	if(protectflag & OB_LOCK_SCALEX)
		size[0]= 1.0f;
	if(protectflag & OB_LOCK_SCALEY)
		size[1]= 1.0f;
	if(protectflag & OB_LOCK_SCALEZ)
		size[2]= 1.0f;
}

static void protectedRotateBits(short protectflag, float *eul, float *oldeul)
{
	if(protectflag & OB_LOCK_ROTX)
		eul[0]= oldeul[0];
	if(protectflag & OB_LOCK_ROTY)
		eul[1]= oldeul[1];
	if(protectflag & OB_LOCK_ROTZ)
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
		if (IS_EQ(axis[0], axis[1]) && IS_EQ(axis[1], axis[2])) {
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
		/* quaternions get limited with euler... (compatability mode) */
		float eul[3], oldeul[3], quat1[4];
		
		QUATCOPY(quat1, quat);
		quat_to_eul( eul,quat);
		quat_to_eul( oldeul,oldquat);
		
		if (protectflag & OB_LOCK_ROTX)
			eul[0]= oldeul[0];
		if (protectflag & OB_LOCK_ROTY)
			eul[1]= oldeul[1];
		if (protectflag & OB_LOCK_ROTZ)
			eul[2]= oldeul[2];
		
		eul_to_quat( quat,eul);
		
		/* quaternions flip w sign to accumulate rotations correctly */
		if ( (quat1[0]<0.0f && quat[0]>0.0f) || (quat1[0]>0.0f && quat[0]<0.0f) ) {
			mul_qt_fl(quat, -1.0f);
		}
	}
}

/* ******************* TRANSFORM LIMITS ********************** */

static void constraintTransLim(TransInfo *t, TransData *td)
{
	if (td->con) {
		bConstraintTypeInfo *cti= get_constraint_typeinfo(CONSTRAINT_TYPE_LOCLIMIT);
		bConstraintOb cob;
		bConstraint *con;
		
		/* Make a temporary bConstraintOb for using these limit constraints
		 * 	- they only care that cob->matrix is correctly set ;-)
		 *	- current space should be local
		 */
		memset(&cob, 0, sizeof(bConstraintOb));
		unit_m4(cob.matrix);
		VECCOPY(cob.matrix[3], td->loc);
		
		/* Evaluate valid constraints */
		for (con= td->con; con; con= con->next) {
			float tmat[4][4];
			
			/* only consider constraint if enabled */
			if (con->flag & CONSTRAINT_DISABLE) continue;
			if (con->enforce == 0.0f) continue;
			
			/* only use it if it's tagged for this purpose (and the right type) */
			if (con->type == CONSTRAINT_TYPE_LOCLIMIT) {
				bLocLimitConstraint *data= con->data;
				
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
		VECCOPY(td->loc, cob.matrix[3]);
	}
}

static void constraintRotLim(TransInfo *t, TransData *td)
{
	if (td->con) {
		bConstraintTypeInfo *cti= get_constraint_typeinfo(CONSTRAINT_TYPE_ROTLIMIT);
		bConstraintOb cob;
		bConstraint *con;

		/* Make a temporary bConstraintOb for using these limit constraints
		 * 	- they only care that cob->matrix is correctly set ;-)
		 *	- current space should be local
		 */
		memset(&cob, 0, sizeof(bConstraintOb));
		if (td->rotOrder == ROT_MODE_QUAT) {
			/* quats */
			if (td->ext)
				quat_to_mat4( cob.matrix,td->ext->quat);
			else
				return;
		}
		else if (td->rotOrder == ROT_MODE_AXISANGLE) {
			/* axis angle */
			if (td->ext)
				axis_angle_to_mat4( cob.matrix,&td->ext->quat[1], td->ext->quat[0]);
			else
				return;
		}
		else {
			/* eulers */
			if (td->ext)
				eulO_to_mat4( cob.matrix,td->ext->rot, td->rotOrder);
			else
				return;
		}
		
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
		if (td->rotOrder == ROT_MODE_QUAT) {
			/* quats */
			mat4_to_quat( td->ext->quat,cob.matrix);
		}
		else if (td->rotOrder == ROT_MODE_AXISANGLE) {
			/* axis angle */
			mat4_to_axis_angle( &td->ext->quat[1], &td->ext->quat[0],cob.matrix);
		}
		else {
			/* eulers */
			mat4_to_eulO( td->ext->rot, td->rotOrder,cob.matrix);
		}
	}
}

static void constraintSizeLim(TransInfo *t, TransData *td)
{
	if (td->con && td->ext) {
		bConstraintTypeInfo *cti= get_constraint_typeinfo(CONSTRAINT_TYPE_SIZELIMIT);
		bConstraintOb cob;
		bConstraint *con;
		
		/* Make a temporary bConstraintOb for using these limit constraints
		 * 	- they only care that cob->matrix is correctly set ;-)
		 *	- current space should be local
		 */
		memset(&cob, 0, sizeof(bConstraintOb));
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

void initWarp(TransInfo *t)
{
	float max[3], min[3];
	int i;
	
	t->mode = TFM_WARP;
	t->transform = Warp;
	t->handleEvent = handleEventWarp;
	
	initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_RATIO);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 5.0f;
	t->snap[2] = 1.0f;
	
	t->flag |= T_NO_CONSTRAINT;
	
	/* we need min/max in view space */
	for(i = 0; i < t->total; i++) {
		float center[3];
		VECCOPY(center, t->data[i].center);
		mul_m3_v3(t->data[i].mtx, center);
		mul_m4_v3(t->viewmat, center);
		sub_v3_v3v3(center, center, t->viewmat[3]);
		if (i)
			minmax_v3_v3v3(min, max, center);
		else {
			VECCOPY(max, center);
			VECCOPY(min, center);
		}
	}
	
	t->center[0]= (min[0]+max[0])/2.0f;
	t->center[1]= (min[1]+max[1])/2.0f;
	t->center[2]= (min[2]+max[2])/2.0f;

	if (max[0] == min[0]) max[0] += 0.1; /* not optimal, but flipping is better than invalid garbage (i.e. division by zero!) */
	t->val= (max[0]-min[0])/2.0f; /* t->val is X dimension projected boundbox */
}

int handleEventWarp(TransInfo *t, wmEvent *event)
{
	int status = 0;
	
	if (event->type == MIDDLEMOUSE && event->val==KM_PRESS)
	{
		// Use customData pointer to signal warp direction
		if	(t->customData == 0)
			t->customData = (void*)1;
		else
			t->customData = 0;
		
		status = 1;
	}
	
	return status;
}

int Warp(TransInfo *t, short mval[2])
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
	VECCOPY(cursor, curs);
	VECCOPY(gcursor, cursor);
	if (t->flag & T_EDIT) {
		sub_v3_v3v3(cursor, cursor, t->obedit->obmat[3]);
		sub_v3_v3v3(gcursor, gcursor, t->obedit->obmat[3]);
		mul_m3_v3(t->data->smtx, gcursor);
	}
	mul_m4_v3(t->viewmat, cursor);
	sub_v3_v3v3(cursor, cursor, t->viewmat[3]);
	
	/* amount of degrees for warp */
	circumfac = 360.0f * t->values[0];
	
	if (t->customData) /* non-null value indicates reversed input */
	{
		circumfac *= -1;
	}
	
	snapGrid(t, &circumfac);
	applyNumInput(&t->num, &circumfac);
	
	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];
		
		outputNumInput(&(t->num), c);
		
		sprintf(str, "Warp: %s", c);
	}
	else {
		/* default header print */
		sprintf(str, "Warp: %.3f", circumfac);
	}
	
	circumfac*= (float)(-M_PI/360.0);
	
	for(i = 0; i < t->total; i++, td++) {
		float loc[3];
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		/* translate point to center, rotate in such a way that outline==distance */
		VECCOPY(vec, td->iloc);
		mul_m3_v3(td->mtx, vec);
		mul_m4_v3(t->viewmat, vec);
		sub_v3_v3v3(vec, vec, t->viewmat[3]);
		
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
		sub_v3_v3v3(loc, loc, t->viewinv[3]);
		mul_m3_v3(td->smtx, loc);
		
		sub_v3_v3v3(loc, loc, td->iloc);
		mul_v3_fl(loc, td->factor);
		add_v3_v3v3(td->loc, td->iloc, loc);
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
	
	return 1;
}

/* ************************** SHEAR *************************** */

void initShear(TransInfo *t)
{
	t->mode = TFM_SHEAR;
	t->transform = Shear;
	t->handleEvent = handleEventShear;
	
	initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_ABSOLUTE);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	
	t->flag |= T_NO_CONSTRAINT;
}

int handleEventShear(TransInfo *t, wmEvent *event)
{
	int status = 0;
	
	if (event->type == MIDDLEMOUSE && event->val==KM_PRESS)
	{
		// Use customData pointer to signal Shear direction
		if	(t->customData == 0)
		{
			initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);
			t->customData = (void*)1;
		}
		else
		{
			initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_ABSOLUTE);
			t->customData = 0;
		}
		
		status = 1;
	}
	
	return status;
}


int Shear(TransInfo *t, short mval[2])
{
	TransData *td = t->data;
	float vec[3];
	float smat[3][3], tmat[3][3], totmat[3][3], persmat[3][3], persinv[3][3];
	float value;
	int i;
	char str[50];
	
	copy_m3_m4(persmat, t->viewmat);
	invert_m3_m3(persinv, persmat);
	
	value = 0.05f * t->values[0];
	
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
	if (t->customData == 0)
		smat[1][0] = value;
	else
		smat[0][1] = value;
	
	mul_m3_m3m3(tmat, smat, persmat);
	mul_m3_m3m3(totmat, persinv, tmat);
	
	for(i = 0 ; i < t->total; i++, td++) {
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
		
		add_v3_v3v3(vec, vec, t->center);
		sub_v3_v3v3(vec, vec, td->center);
		
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
}

static void headerResize(TransInfo *t, float vec[3], char *str) {
	char tvec[60];
	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec);
	}
	else {
		sprintf(&tvec[0], "%.4f", vec[0]);
		sprintf(&tvec[20], "%.4f", vec[1]);
		sprintf(&tvec[40], "%.4f", vec[2]);
	}
	
	if (t->con.mode & CON_APPLY) {
		switch(t->num.idx_max) {
		case 0:
			sprintf(str, "Scale: %s%s %s", &tvec[0], t->con.text, t->proptext);
			break;
		case 1:
			sprintf(str, "Scale: %s : %s%s %s", &tvec[0], &tvec[20], t->con.text, t->proptext);
			break;
		case 2:
			sprintf(str, "Scale: %s : %s : %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
		}
	}
	else {
		if (t->flag & T_2D_EDIT)
			sprintf(str, "Scale X: %s   Y: %s%s %s", &tvec[0], &tvec[20], t->con.text, t->proptext);
		else
			sprintf(str, "Scale X: %s   Y: %s  Z: %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
	}
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
	if( VECSIGNFLIP(mat[0], smat[0]) ) size[0]= -size[0];
	if( VECSIGNFLIP(mat[1], smat[1]) ) size[1]= -size[1];
	if( VECSIGNFLIP(mat[2], smat[2]) ) size[2]= -size[2];
}


static void ElementResize(TransInfo *t, TransData *td, float mat[3][3]) {
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
	if (t->around == V3D_LOCAL) {
		if (t->flag & T_OBJECT) {
			VECCOPY(center, td->center);
		}
		else if (t->flag & T_EDIT) {
			
			if(t->around==V3D_LOCAL && (t->settings->selectmode & SCE_SELECT_FACE)) {
				VECCOPY(center, td->center);
			}
			else {
				VECCOPY(center, t->center);
			}
		}
		else {
			VECCOPY(center, t->center);
		}
	}
	else {
		VECCOPY(center, t->center);
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
			if((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)){
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
	
	add_v3_v3v3(vec, vec, center);
	if (t->flag & T_POINTS)
		sub_v3_v3v3(vec, vec, td->iloc);
	else
		sub_v3_v3v3(vec, vec, td->center);
	
	mul_v3_fl(vec, td->factor);
	
	if (t->flag & (T_OBJECT|T_POSE)) {
		mul_m3_v3(td->smtx, vec);
	}
	
	protectedTransBits(td->protectflag, vec);
	add_v3_v3v3(td->loc, td->iloc, vec);
	
	constraintTransLim(t, td);
}

int Resize(TransInfo *t, short mval[2])
{
	TransData *td;
	float size[3], mat[3][3];
	float ratio;
	int i;
	char str[200];
	
	/* for manipulator, center handle, the scaling can't be done relative to center */
	if( (t->flag & T_USES_MANIPULATOR) && t->con.mode==0)
	{
		ratio = 1.0f - ((t->imval[0] - mval[0]) + (t->imval[1] - mval[1]))/100.0f;
	}
	else
	{
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
		VECCOPY(size, t->auto_values);
	}
	
	VECCOPY(t->values, size);
	
	size_to_mat3( mat,size);
	
	if (t->con.applySize) {
		t->con.applySize(t, NULL, mat);
	}
	
	copy_m3_m3(t->mat, mat);	// used in manipulator
	
	headerResize(t, size, str);
	
	for(i = 0, td=t->data; i < t->total; i++, td++) {
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
		
		for(i = 0, td=t->data; i < t->total; i++, td++)
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
	
	t->num.flag |= NUM_NULL_ONE | NUM_NO_NEGATIVE;
	t->flag |= T_NO_CONSTRAINT;
	
	// Calculate average radius
	for(i = 0 ; i < t->total; i++, td++) {
		t->val += len_v3v3(t->center, td->iloc);
	}
	
	t->val /= (float)t->total;
}

int ToSphere(TransInfo *t, short mval[2])
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
	
	
	for(i = 0 ; i < t->total; i++, td++) {
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


void initRotation(TransInfo *t)
{
	t->mode = TFM_ROTATION;
	t->transform = Rotation;
	
	initMouseInputMode(t, &t->mouse, INPUT_ANGLE);
	
	t->ndof.axis = 16;
	/* Scale down and flip input for rotation */
	t->ndof.factor[0] = -0.2f;
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;
	
	if (t->flag & T_2D_EDIT)
		t->flag |= T_NO_CONSTRAINT;
}

static void ElementRotation(TransInfo *t, TransData *td, float mat[3][3], short around) {
	float vec[3], totmat[3][3], smat[3][3];
	float eul[3], fmat[3][3], quat[4];
	float *center = t->center;
	
	/* local constraint shouldn't alter center */
	if (around == V3D_LOCAL) {
		if (t->flag & (T_OBJECT|T_POSE)) {
			center = td->center;
		}
		else {
			if(around==V3D_LOCAL && (t->settings->selectmode & SCE_SELECT_FACE)) {
				center = td->center;
			}
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
		
		
		if(td->flag & TD_USEQUAT) {
			mul_serie_m3(fmat, td->mtx, mat, td->smtx, 0, 0, 0, 0, 0);
			mat3_to_quat( quat,fmat);	// Actual transform
			
			if(td->ext->quat){
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
			
			add_v3_v3v3(vec, vec, center);
			/* vec now is the location where the object has to be */
			
			sub_v3_v3v3(vec, vec, td->center); // Translation needed from the initial location
			
			mul_m3_v3(pmtx, vec);	// To Global space
			mul_m3_v3(td->smtx, vec);// To Pose space
			
			protectedTransBits(td->protectflag, vec);
			
			add_v3_v3v3(td->loc, td->iloc, vec);
			
			constraintTransLim(t, td);
		}
		
		/* rotation */
		if ((t->flag & T_V3D_ALIGN)==0) { // align mode doesn't rotate objects itself
			/* euler or quaternion/axis-angle? */
			if (td->rotOrder == ROT_MODE_QUAT) {
				mul_serie_m3(fmat, td->mtx, mat, td->smtx, 0, 0, 0, 0, 0);
				
				mat3_to_quat( quat,fmat);	// Actual transform
				
				mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
				/* this function works on end result */
				protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
				
			}
			else if (td->rotOrder == ROT_MODE_AXISANGLE) {
				/* calculate effect based on quats */
				float iquat[4], tquat[4];
				
				axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);
				
				mul_serie_m3(fmat, td->mtx, mat, td->smtx, 0, 0, 0, 0, 0);
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
				VECCOPY(eul, td->ext->irot);
				eulO_to_mat3( eulmat,eul, td->rotOrder);
				
				/* mat = transform, obmat = bone rotation */
				mul_m3_m3m3(fmat, smat, eulmat);
				
				mat3_to_compatible_eulO( eul, td->ext->rot, td->rotOrder,fmat);
				
				/* and apply (to end result only) */
				protectedRotateBits(td->protectflag, eul, td->ext->irot);
				VECCOPY(td->ext->rot, eul);
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
			add_v3_v3v3(vec, vec, center);
			/* vec now is the location where the object has to be */
			sub_v3_v3v3(vec, vec, td->center);
			mul_m3_v3(td->smtx, vec);
			
			protectedTransBits(td->protectflag, vec);
			
			add_v3_v3v3(td->loc, td->iloc, vec);
		}
		
		
		constraintTransLim(t, td);
		
		/* rotation */
		if ((t->flag & T_V3D_ALIGN)==0) { // align mode doesn't rotate objects itself
			/* euler or quaternion? */
 	  	    if ((td->rotOrder == ROT_MODE_QUAT) || (td->flag & TD_USEQUAT)) {
				mul_serie_m3(fmat, td->mtx, mat, td->smtx, 0, 0, 0, 0, 0);
				mat3_to_quat( quat,fmat);	// Actual transform
				
				mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
				/* this function works on end result */
				protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
			}
			else if (td->rotOrder == ROT_MODE_AXISANGLE) {
				/* calculate effect based on quats */
				float iquat[4], tquat[4];
				
				axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);
				
				mul_serie_m3(fmat, td->mtx, mat, td->smtx, 0, 0, 0, 0, 0);
				mat3_to_quat( quat,fmat);	// Actual transform
				mul_qt_qtqt(tquat, quat, iquat);
				
				quat_to_axis_angle( td->ext->rotAxis, td->ext->rotAngle,quat); 
				
				/* this function works on end result */
				protectedAxisAngleBits(td->protectflag, td->ext->rotAxis, td->ext->rotAngle, td->ext->irotAxis, td->ext->irotAngle);
			}
			else {
				float obmat[3][3];
				
				mul_m3_m3m3(totmat, mat, td->mtx);
				mul_m3_m3m3(smat, td->smtx, totmat);
				
				/* calculate the total rotatation in eulers */
				add_v3_v3v3(eul, td->ext->irot, td->ext->drot); /* we have to correct for delta rot */
				eulO_to_mat3( obmat,eul, td->rotOrder);
				/* mat = transform, obmat = object rotation */
				mul_m3_m3m3(fmat, smat, obmat);
				
				mat3_to_compatible_eulO( eul, td->ext->rot, td->rotOrder,fmat);
				
				/* correct back for delta rot */
				sub_v3_v3v3(eul, eul, td->ext->drot);
				
				/* and apply */
				protectedRotateBits(td->protectflag, eul, td->ext->irot);
				VECCOPY(td->ext->rot, eul);
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
	
	for(i = 0 ; i < t->total; i++, td++) {
		
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

int Rotation(TransInfo *t, short mval[2])
{
	char str[64];
	
	float final;
	
	float axis[3];
	float mat[3][3];
	
	VECCOPY(axis, t->viewinv[2]);
	mul_v3_fl(axis, -1.0f);
	normalize_v3(axis);
	
	final = t->values[0];
	
	applyNDofInput(&t->ndof, &final);
	
	snapGrid(t, &final);
	
	if (t->con.applyRot) {
		t->con.applyRot(t, NULL, axis, &final);
	}
	
	applySnapping(t, &final);
	
	if (hasNumInput(&t->num)) {
		char c[20];
		
		applyNumInput(&t->num, &final);
		
		outputNumInput(&(t->num), c);
		
		sprintf(str, "Rot: %s %s %s", &c[0], t->con.text, t->proptext);
		
		/* Clamp between -180 and 180 */
		while (final >= 180.0)
			final -= 360.0;
		
		while (final <= -180.0)
			final += 360.0;
		
		final *= (float)(M_PI / 180.0);
	}
	else {
		sprintf(str, "Rot: %.2f%s %s", 180.0*final/M_PI, t->con.text, t->proptext);
	}
	
	vec_rot_to_mat3( mat,axis, final);
	
	// TRANSFORM_FIX_ME
//	t->values[0] = final;		// used in manipulator
//	copy_m3_m3(t->mat, mat);	// used in manipulator
	
	applyRotation(t, final, axis);
	
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

	t->ndof.axis = 40;
	/* Scale down input for rotation */
	t->ndof.factor[0] = 0.2f;
	t->ndof.factor[1] = 0.2f;

	t->idx_max = 1;
	t->num.idx_max = 1;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;

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

	for(i = 0 ; i < t->total; i++, td++) {
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

int Trackball(TransInfo *t, short mval[2])
{
	char str[128];
	float axis1[3], axis2[3];
	float mat[3][3], totmat[3][3], smat[3][3];
	float phi[2];

	VECCOPY(axis1, t->persinv[0]);
	VECCOPY(axis2, t->persinv[1]);
	normalize_v3(axis1);
	normalize_v3(axis2);

	phi[0] = t->values[0];
	phi[1] = t->values[1];

	applyNDofInput(&t->ndof, phi);

	snapGrid(t, phi);

	if (hasNumInput(&t->num)) {
		char c[40];

		applyNumInput(&t->num, phi);

		outputNumInput(&(t->num), c);

		sprintf(str, "Trackball: %s %s %s", &c[0], &c[20], t->proptext);

		phi[0] *= (float)(M_PI / 180.0);
		phi[1] *= (float)(M_PI / 180.0);
	}
	else {
		sprintf(str, "Trackball: %.2f %.2f %s", 180.0*phi[0]/M_PI, 180.0*phi[1]/M_PI, t->proptext);
	}

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
	t->mode = TFM_TRANSLATION;
	t->transform = Translation;

	initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

	t->idx_max = (t->flag & T_2D_EDIT)? 1: 2;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	t->ndof.axis = (t->flag & T_2D_EDIT)? 1|2: 1|2|4;

	if(t->spacetype == SPACE_VIEW3D) {
		View3D *v3d = t->view;

		t->snap[0] = 0.0f;
		t->snap[1] = v3d->gridview * 1.0f;
		t->snap[2] = t->snap[1] * 0.1f;
	}
	else if(t->spacetype == SPACE_IMAGE) {
		t->snap[0] = 0.0f;
		t->snap[1] = 0.125f;
		t->snap[2] = 0.0625f;
	}
	else {
		t->snap[0] = 0.0f;
		t->snap[1] = t->snap[2] = 1.0f;
	}
}

static void headerTranslation(TransInfo *t, float vec[3], char *str) {
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

		VECCOPY(dvec, vec);
		applyAspectRatio(t, dvec);

		dist = len_v3(vec);
		if(t->scene->unit.system) {
			int i, do_split= t->scene->unit.flag & USER_UNIT_OPT_SPLIT ? 1:0;

			for(i=0; i<3; i++)
				bUnit_AsString(&tvec[i*20], 20, dvec[i]*t->scene->unit.scale_length, 4, t->scene->unit.system, B_UNIT_LENGTH, do_split, 1);
		}
		else {
			sprintf(&tvec[0], "%.4f", dvec[0]);
			sprintf(&tvec[20], "%.4f", dvec[1]);
			sprintf(&tvec[40], "%.4f", dvec[2]);
		}
	}

	if(t->scene->unit.system)
		bUnit_AsString(distvec, sizeof(distvec), dist*t->scene->unit.scale_length, 4, t->scene->unit.system, B_UNIT_LENGTH, t->scene->unit.flag & USER_UNIT_OPT_SPLIT, 0);
	else if( dist > 1e10 || dist < -1e10 )	/* prevent string buffer overflow */
		sprintf(distvec, "%.4e", dist);
	else
		sprintf(distvec, "%.4f", dist);

	if(t->flag & T_AUTOIK) {
		short chainlen= t->settings->autoik_chainlen;

		if(chainlen)
			sprintf(autoik, "AutoIK-Len: %d", chainlen);
		else
			strcpy(autoik, "");
	}
	else
		strcpy(autoik, "");

	if (t->con.mode & CON_APPLY) {
		switch(t->num.idx_max) {
		case 0:
			sprintf(str, "D: %s (%s)%s %s  %s", &tvec[0], distvec, t->con.text, t->proptext, &autoik[0]);
			break;
		case 1:
			sprintf(str, "D: %s   D: %s (%s)%s %s  %s", &tvec[0], &tvec[20], distvec, t->con.text, t->proptext, &autoik[0]);
			break;
		case 2:
			sprintf(str, "D: %s   D: %s  D: %s (%s)%s %s  %s", &tvec[0], &tvec[20], &tvec[40], distvec, t->con.text, t->proptext, &autoik[0]);
		}
	}
	else {
		if(t->flag & T_2D_EDIT)
			sprintf(str, "Dx: %s   Dy: %s (%s)%s %s", &tvec[0], &tvec[20], distvec, t->con.text, t->proptext);
		else
			sprintf(str, "Dx: %s   Dy: %s  Dz: %s (%s)%s %s  %s", &tvec[0], &tvec[20], &tvec[40], distvec, t->con.text, t->proptext, &autoik[0]);
	}
}

static void applyTranslation(TransInfo *t, float vec[3]) {
	TransData *td = t->data;
	float tvec[3];
	int i;

	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		/* handle snapping rotation before doing the translation */
		if (usingSnappingNormal(t))
		{
			if (validSnappingNormal(t))
			{
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
			else
			{
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
			VECCOPY(tvec, vec);
		}
		
		mul_m3_v3(td->smtx, tvec);
		mul_v3_fl(tvec, td->factor);
		
		protectedTransBits(td->protectflag, tvec);
		
		add_v3_v3v3(td->loc, td->iloc, tvec);
		
		constraintTransLim(t, td);
	}
}

/* uses t->vec to store actual translation in */
int Translation(TransInfo *t, short mval[2])
{
	float tvec[3];
	char str[250];

	if (t->con.mode & CON_APPLY) {
		float pvec[3] = {0.0f, 0.0f, 0.0f};
		applySnapping(t, t->values);
		t->con.applyVec(t, NULL, t->values, tvec, pvec);
		VECCOPY(t->values, tvec);
		headerTranslation(t, pvec, str);
	}
	else {
		applyNDofInput(&t->ndof, t->values);
		snapGrid(t, t->values);
		applyNumInput(&t->num, t->values);
		if (hasNumInput(&t->num))
		{
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

		t->flag |= T_NO_CONSTRAINT;
	}
}



int ShrinkFatten(TransInfo *t, short mval[2])
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


	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		VECCOPY(vec, td->axismtx[2]);
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

	t->ndof.axis = 16;
	/* Scale down and flip input for rotation */
	t->ndof.factor[0] = -0.2f;

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;

	t->flag |= T_NO_CONSTRAINT;
}



int Tilt(TransInfo *t, short mval[2])
{
	TransData *td = t->data;
	int i;
	char str[50];

	float final;

	final = t->values[0];

	applyNDofInput(&t->ndof, &final);

	snapGrid(t, &final);

	if (hasNumInput(&t->num)) {
		char c[20];

		applyNumInput(&t->num, &final);

		outputNumInput(&(t->num), c);

		sprintf(str, "Tilt: %s %s", &c[0], t->proptext);

		final *= (float)(M_PI / 180.0);
	}
	else {
		sprintf(str, "Tilt: %.2f %s", 180.0*final/M_PI, t->proptext);
	}

	for(i = 0 ; i < t->total; i++, td++) {
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

	t->flag |= T_NO_CONSTRAINT;
}

int CurveShrinkFatten(TransInfo *t, short mval[2])
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

	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if(td->val) {
			//*td->val= ratio;
			*td->val= td->ival*ratio;
			if (*td->val <= 0.0f) *td->val = 0.0001f;
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

	t->ndof.axis = 4;
	/* Flip direction */
	t->ndof.factor[0] = -1.0f;

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 1.0f;
	t->snap[2] = t->snap[1] * 0.1f;
}


int PushPull(TransInfo *t, short mval[2])
{
	float vec[3], axis[3];
	float distance;
	int i;
	char str[128];
	TransData *td = t->data;

	distance = t->values[0];

	applyNDofInput(&t->ndof, &distance);

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

	for(i = 0 ; i < t->total; i++, td++) {
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
				sub_v3_v3v3(vec, vec, dvec);
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
		if(!G.editBMesh) return 0;

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

int Bevel(TransInfo *t, short mval[2])
{
	float distance,d;
	int i;
	char str[128];
	char *mode;
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
	for(i = 0 ; i < t->total; i++, td++) {
		if (td->axismtx[1][0] > 0 && distance > td->axismtx[1][0]) {
			d = td->axismtx[1][0];
		}
		else {
			d = distance;
		}
		VECADDFAC(td->loc,td->center,td->axismtx[0],(*td->val)*d);
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

	t->flag |= T_NO_CONSTRAINT;
}

int BevelWeight(TransInfo *t, short mval[2])
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

	for(i = 0 ; i < t->total; i++, td++) {
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

	t->flag |= T_NO_CONSTRAINT;
}

int Crease(TransInfo *t, short mval[2])
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

	for(i = 0 ; i < t->total; i++, td++) {
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
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
}

static void headerBoneSize(TransInfo *t, float vec[3], char *str) {
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

int BoneSize(TransInfo *t, short mval[2])
{
	TransData *td = t->data;
	float size[3], mat[3][3];
	float ratio;
	int i;
	char str[60];
	
	// TRANSFORM_FIX_ME MOVE TO MOUSE INPUT
	/* for manipulator, center handle, the scaling can't be done relative to center */
	if( (t->flag & T_USES_MANIPULATOR) && t->con.mode==0)
	{
		ratio = 1.0f - ((t->imval[0] - mval[0]) + (t->imval[1] - mval[1]))/100.0f;
	}
	else
	{
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
	
	for(i = 0 ; i < t->total; i++, td++) {
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
	
	t->flag |= T_NO_CONSTRAINT;
}

int BoneEnvelope(TransInfo *t, short mval[2])
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
	
	for(i = 0 ; i < t->total; i++, td++) {
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

static int createSlideVerts(TransInfo *t)
{
	Mesh *me = t->obedit->data;
	EditMesh *em = me->edit_mesh;
	EditFace *efa;
	EditEdge *eed,*first=NULL,*last=NULL, *temp = NULL;
	EditVert *ev, *nearest = NULL;
	LinkNode *edgelist = NULL, *vertlist=NULL, *look;
	GHash *vertgh;
	TransDataSlideVert *tempsv;
	float vertdist; // XXX, projectMat[4][4];
	int i, j, numsel, numadded=0, timesthrough = 0, vertsel=0;
	/* UV correction vars */
	GHash **uvarray= NULL;
	SlideData *sld = MEM_callocN(sizeof(*sld), "sld");
	int  uvlay_tot= CustomData_number_of_layers(&em->fdata, CD_MTFACE);
	int uvlay_idx;
	TransDataSlideUv *slideuvs=NULL, *suv=NULL, *suv_last=NULL;
	RegionView3D *v3d = t->ar->regiondata;
	float projectMat[4][4];
	float start[3] = {0.0f, 0.0f, 0.0f}, end[3] = {0.0f, 0.0f, 0.0f};
	float vec[3];
	float totvec=0.0;

	if (!v3d) {
		/*ok, let's try to survive this*/
		unit_m4(projectMat);
	} else {
		view3d_get_object_project_mat(v3d, t->obedit, projectMat);
	}
	
	numsel =0;

	// Get number of selected edges and clear some flags
	for(eed=em->edges.first;eed;eed=eed->next) {
		eed->f1 = 0;
		eed->f2 = 0;
		if(eed->f & SELECT) numsel++;
	}

	for(ev=em->verts.first;ev;ev=ev->next) {
		ev->f1 = 0;
	}

	//Make sure each edge only has 2 faces
	// make sure loop doesn't cross face
	for(efa=em->faces.first;efa;efa=efa->next) {
		int ct = 0;
		if(efa->e1->f & SELECT) {
			ct++;
			efa->e1->f1++;
			if(efa->e1->f1 > 2) {
				//BKE_report(op->reports, RPT_ERROR, "3+ face edge");
				return 0;
			}
		}
		if(efa->e2->f & SELECT) {
			ct++;
			efa->e2->f1++;
			if(efa->e2->f1 > 2) {
				//BKE_report(op->reports, RPT_ERROR, "3+ face edge");
				return 0;
			}
		}
		if(efa->e3->f & SELECT) {
			ct++;
			efa->e3->f1++;
			if(efa->e3->f1 > 2) {
				//BKE_report(op->reports, RPT_ERROR, "3+ face edge");
				return 0;
			}
		}
		if(efa->e4 && efa->e4->f & SELECT) {
			ct++;
			efa->e4->f1++;
			if(efa->e4->f1 > 2) {
				//BKE_report(op->reports, RPT_ERROR, "3+ face edge");
				return 0;
			}
		}
		// Make sure loop is not 2 edges of same face
		if(ct > 1) {
		   //BKE_report(op->reports, RPT_ERROR, "Loop crosses itself");
		   return 0;
		}
	}

	// Get # of selected verts
	for(ev=em->verts.first;ev;ev=ev->next) {
		if(ev->f & SELECT) vertsel++;
	}

	// Test for multiple segments
	if(vertsel > numsel+1) {
		//BKE_report(op->reports, RPT_ERROR, "Please choose a single edge loop");
		return 0;
	}

	// Get the edgeloop in order - mark f1 with SELECT once added
	for(eed=em->edges.first;eed;eed=eed->next) {
		if((eed->f & SELECT) && !(eed->f1 & SELECT)) {
			// If this is the first edge added, just put it in
			if(!edgelist) {
				BLI_linklist_prepend(&edgelist,eed);
				numadded++;
				first = eed;
				last  = eed;
				eed->f1 = SELECT;
			} else {
				if(editedge_getSharedVert(eed, last)) {
					BLI_linklist_append(&edgelist,eed);
					eed->f1 = SELECT;
					numadded++;
					last = eed;
				}  else if(editedge_getSharedVert(eed, first)) {
					BLI_linklist_prepend(&edgelist,eed);
					eed->f1 = SELECT;
					numadded++;
					first = eed;
				}
			}
		}
		if(eed->next == NULL && numadded != numsel) {
			eed=em->edges.first;
			timesthrough++;
		}

		// It looks like there was an unexpected case - Hopefully should not happen
		if(timesthrough >= numsel*2) {
			BLI_linklist_free(edgelist,NULL);
			//BKE_report(op->reports, RPT_ERROR, "Could not order loop");
			return 0;
		}
	}

	// Put the verts in order in a linklist
	look = edgelist;
	while(look) {
		eed = look->link;
		if(!vertlist) {
			if(look->next) {
				temp = look->next->link;

				//This is the first entry takes care of extra vert
				if(eed->v1 != temp->v1 && eed->v1 != temp->v2) {
					BLI_linklist_append(&vertlist,eed->v1);
					eed->v1->f1 = 1;
				} else {
					BLI_linklist_append(&vertlist,eed->v2);
					eed->v2->f1 = 1;
				}
			} else {
				//This is the case that we only have 1 edge
				BLI_linklist_append(&vertlist,eed->v1);
				eed->v1->f1 = 1;
			}
		}
		// for all the entries
		if(eed->v1->f1 != 1) {
			BLI_linklist_append(&vertlist,eed->v1);
			eed->v1->f1 = 1;
		} else  if(eed->v2->f1 != 1) {
			BLI_linklist_append(&vertlist,eed->v2);
			eed->v2->f1 = 1;
		}
		look = look->next;
	}

	// populate the SlideVerts

	vertgh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	look = vertlist;
	while(look) {
		i=0;
		j=0;
		ev = look->link;
		tempsv = (struct TransDataSlideVert*)MEM_mallocN(sizeof(struct TransDataSlideVert),"SlideVert");
		tempsv->up = NULL;
		tempsv->down = NULL;
		tempsv->origvert.co[0] = ev->co[0];
		tempsv->origvert.co[1] = ev->co[1];
		tempsv->origvert.co[2] = ev->co[2];
		tempsv->origvert.no[0] = ev->no[0];
		tempsv->origvert.no[1] = ev->no[1];
		tempsv->origvert.no[2] = ev->no[2];
		// i is total edges that vert is on
		// j is total selected edges that vert is on

		for(eed=em->edges.first;eed;eed=eed->next) {
			if(eed->v1 == ev || eed->v2 == ev) {
				i++;
				if(eed->f & SELECT) {
					 j++;
				}
			}
		}
		// If the vert is in the middle of an edge loop, it touches 2 selected edges and 2 unselected edges
		if(i == 4 && j == 2) {
			for(eed=em->edges.first;eed;eed=eed->next) {
				if(editedge_containsVert(eed, ev)) {
					if(!(eed->f & SELECT)) {
						 if(!tempsv->up) {
							 tempsv->up = eed;
						 } else if (!(tempsv->down)) {
							 tempsv->down = eed;
						 }
					}
				}
			}
		}
		// If it is on the end of the loop, it touches 1 selected and as least 2 more unselected
		if(i >= 3 && j == 1) {
			for(eed=em->edges.first;eed;eed=eed->next) {
				if(editedge_containsVert(eed, ev) && eed->f & SELECT) {
					for(efa = em->faces.first;efa;efa=efa->next) {
						if(editface_containsEdge(efa, eed)) {
							if(editedge_containsVert(efa->e1, ev) && efa->e1 != eed) {
								 if(!tempsv->up) {
									 tempsv->up = efa->e1;
								 } else if (!(tempsv->down)) {
									 tempsv->down = efa->e1;
								 }
							}
							if(editedge_containsVert(efa->e2, ev) && efa->e2 != eed) {
								 if(!tempsv->up) {
									 tempsv->up = efa->e2;
								 } else if (!(tempsv->down)) {
									 tempsv->down = efa->e2;
								 }
							}
							if(editedge_containsVert(efa->e3, ev) && efa->e3 != eed) {
								 if(!tempsv->up) {
									 tempsv->up = efa->e3;
								 } else if (!(tempsv->down)) {
									 tempsv->down = efa->e3;
								 }
							}
							if(efa->e4) {
								if(editedge_containsVert(efa->e4, ev) && efa->e4 != eed) {
									 if(!tempsv->up) {
										 tempsv->up = efa->e4;
									 } else if (!(tempsv->down)) {
										 tempsv->down = efa->e4;
									 }
								}
							}

						}
					}
				}
			}
		}
		if(i > 4 && j == 2) {
			BLI_ghash_free(vertgh, NULL, (GHashValFreeFP)MEM_freeN);
			BLI_linklist_free(vertlist,NULL);
			BLI_linklist_free(edgelist,NULL);
			return 0;
		}
		BLI_ghash_insert(vertgh,ev,tempsv);

		look = look->next;
	}

	// make sure the UPs nad DOWNs are 'faceloops'
	// Also find the nearest slidevert to the cursor

	look = vertlist;
	nearest = NULL;
	vertdist = -1;
	while(look) {
		tempsv  = BLI_ghash_lookup(vertgh,(EditVert*)look->link);

		if(!tempsv->up || !tempsv->down) {
			//BKE_report(op->reports, RPT_ERROR, "Missing rails");
			BLI_ghash_free(vertgh, NULL, (GHashValFreeFP)MEM_freeN);
			BLI_linklist_free(vertlist,NULL);
			BLI_linklist_free(edgelist,NULL);
			return 0;
		}

		if(me->drawflag & ME_DRAW_EDGELEN) {
			if(!(tempsv->up->f & SELECT)) {
				tempsv->up->f |= SELECT;
				tempsv->up->f2 |= 16;
			} else {
				tempsv->up->f2 |= ~16;
			}
			if(!(tempsv->down->f & SELECT)) {
				tempsv->down->f |= SELECT;
				tempsv->down->f2 |= 16;
			} else {
				tempsv->down->f2 |= ~16;
			}
		}

		if(look->next != NULL) {
			TransDataSlideVert *sv;
			
			ev = (EditVert*)look->next->link;
			sv = BLI_ghash_lookup(vertgh, ev);

			if(sv) {
				float co[3], co2[3], vec[3];

				ev = (EditVert*)look->link;

				if(!sharesFace(em, tempsv->up,sv->up)) {
					EditEdge *swap;
					swap = sv->up;
					sv->up = sv->down;
					sv->down = swap;
				}
				
				if (v3d) {
					view3d_project_float(t->ar, tempsv->up->v1->co, co, projectMat);
					view3d_project_float(t->ar, tempsv->up->v2->co, co2, projectMat);
				}

				if (ev == tempsv->up->v1) {
					sub_v3_v3v3(vec, co, co2);
				} else {
					sub_v3_v3v3(vec, co2, co);
				}

				add_v3_v3v3(start, start, vec);

				if (v3d) {
					view3d_project_float(t->ar, tempsv->down->v1->co, co, projectMat);
					view3d_project_float(t->ar, tempsv->down->v2->co, co2, projectMat);
				}

				if (ev == tempsv->down->v1) {
					sub_v3_v3v3(vec, co2, co);
				} else {
					sub_v3_v3v3(vec, co, co2);
				}

				add_v3_v3v3(end, end, vec);

				totvec += 1.0f;
				nearest = (EditVert*)look->link;
			}
		}



		look = look->next;
	}

	add_v3_v3v3(start, start, end);
	mul_v3_fl(start, 0.5*(1.0/totvec));
	VECCOPY(vec, start);
	start[0] = t->mval[0];
	start[1] = t->mval[1];
	add_v3_v3v3(end, start, vec);
	
	sld->start[0] = (short) start[0];
	sld->start[1] = (short) start[1];
	sld->end[0] = (short) end[0];
	sld->end[1] = (short) end[1];
	
	if (uvlay_tot) { // XXX && (scene->toolsettings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT)) {
		int maxnum = 0;

		uvarray = MEM_callocN( uvlay_tot * sizeof(GHash *), "SlideUVs Array");
		sld->totuv = uvlay_tot;
		suv_last = slideuvs = MEM_callocN( uvlay_tot * (numadded+1) * sizeof(TransDataSlideUv), "SlideUVs"); /* uvLayers * verts */
		suv = NULL;

		for (uvlay_idx=0; uvlay_idx<uvlay_tot; uvlay_idx++) {

			uvarray[uvlay_idx] = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);

			for(ev=em->verts.first;ev;ev=ev->next) {
				ev->tmp.l = 0;
			}
			look = vertlist;
			while(look) {
				float *uv_new;
				tempsv  = BLI_ghash_lookup(vertgh,(EditVert*)look->link);

				ev = look->link;
				suv = NULL;
				for(efa = em->faces.first;efa;efa=efa->next) {
					if (ev->tmp.l != -1) { /* test for self, in this case its invalid */
						int k=-1; /* face corner */

						/* Is this vert in the faces corner? */
						if		(efa->v1==ev)				k=0;
						else if	(efa->v2==ev)				k=1;
						else if	(efa->v3==ev)				k=2;
						else if	(efa->v4 && efa->v4==ev)	k=3;

						if (k != -1) {
							MTFace *tf = CustomData_em_get_n(&em->fdata, efa->data, CD_MTFACE, uvlay_idx);
							EditVert *ev_up, *ev_down;

							uv_new = tf->uv[k];

							if (ev->tmp.l) {
								if (fabs(suv->origuv[0]-uv_new[0]) > 0.0001 || fabs(suv->origuv[1]-uv_new[1])) {
									ev->tmp.l = -1; /* Tag as invalid */
									BLI_linklist_free(suv->fuv_list,NULL);
									suv->fuv_list = NULL;
									BLI_ghash_remove(uvarray[uvlay_idx],ev, NULL, NULL);
									suv = NULL;
									break;
								}
							} else {
								ev->tmp.l = 1;
								suv = suv_last;

								suv->fuv_list = NULL;
								suv->uv_up = suv->uv_down = NULL;
								suv->origuv[0] = uv_new[0];
								suv->origuv[1] = uv_new[1];

								BLI_linklist_prepend(&suv->fuv_list, uv_new);
								BLI_ghash_insert(uvarray[uvlay_idx],ev,suv);

								suv_last++; /* advance to next slide UV */
								maxnum++;
							}

							/* Now get the uvs along the up or down edge if we can */
							if (suv) {
								if (!suv->uv_up) {
									ev_up = editedge_getOtherVert(tempsv->up,ev);
									if		(efa->v1==ev_up)				suv->uv_up = tf->uv[0];
									else if	(efa->v2==ev_up)				suv->uv_up = tf->uv[1];
									else if	(efa->v3==ev_up)				suv->uv_up = tf->uv[2];
									else if	(efa->v4 && efa->v4==ev_up)		suv->uv_up = tf->uv[3];
								}
								if (!suv->uv_down) { /* if the first face was apart of the up edge, it cant be apart of the down edge */
									ev_down = editedge_getOtherVert(tempsv->down,ev);
									if		(efa->v1==ev_down)				suv->uv_down = tf->uv[0];
									else if	(efa->v2==ev_down)				suv->uv_down = tf->uv[1];
									else if	(efa->v3==ev_down)				suv->uv_down = tf->uv[2];
									else if	(efa->v4 && efa->v4==ev_down)	suv->uv_down = tf->uv[3];
								}

								/* Copy the pointers to the face UV's */
								BLI_linklist_prepend(&suv->fuv_list, uv_new);
							}
						}
					}
				}
				look = look->next;
			}
		} /* end uv layer loop */
	} /* end uvlay_tot */

	sld->uvhash = uvarray;
	sld->slideuv = slideuvs;
	sld->vhash = vertgh;
	sld->nearest = nearest;
	sld->vertlist = vertlist;
	sld->edgelist = edgelist;
	sld->suv_last = suv_last;
	sld->uvlay_tot = uvlay_tot;

	// we should have enough info now to slide

	t->customData = sld;

	return 1;
}

void freeSlideVerts(TransInfo *t)
{
	TransDataSlideUv *suv;
	SlideData *sld = t->customData;
	int uvlay_idx;

	//BLI_ghash_free(edgesgh, freeGHash, NULL);
	BLI_ghash_free(sld->vhash, NULL, (GHashValFreeFP)MEM_freeN);
	BLI_linklist_free(sld->vertlist, NULL);
	BLI_linklist_free(sld->edgelist, NULL);

	if (sld->uvlay_tot) {
		for (uvlay_idx=0; uvlay_idx<sld->uvlay_tot; uvlay_idx++) {
			BLI_ghash_free(sld->uvhash[uvlay_idx], NULL, NULL);
		}

		suv = sld->suv_last-1;
		while (suv >= sld->slideuv) {
			if (suv->fuv_list) {
				BLI_linklist_free(suv->fuv_list,NULL);
			}
			suv--;
		}

		MEM_freeN(sld->slideuv);
		MEM_freeN(sld->uvhash);
	}

	MEM_freeN(sld);
	t->customData = NULL;
}

void initEdgeSlide(TransInfo *t)
{
	SlideData *sld;

	t->mode = TFM_EDGE_SLIDE;
	t->transform = EdgeSlide;
	
	createSlideVerts(t);
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
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;

	t->flag |= T_NO_CONSTRAINT;
}

int doEdgeSlide(TransInfo *t, float perc)
{
	Mesh *me= t->obedit->data;
	EditMesh *em = me->edit_mesh;
	SlideData *sld = t->customData;
	EditVert *ev, *nearest = sld->nearest;
	EditVert *centerVert, *upVert, *downVert;
	LinkNode *vertlist=sld->vertlist, *look;
	GHash *vertgh = sld->vhash;
	TransDataSlideVert *tempsv;
	float len = 0.0f;
	int prop=1, flip=0;
	/* UV correction vars */
	GHash **uvarray= sld->uvhash;
	int  uvlay_tot= CustomData_number_of_layers(&em->fdata, CD_MTFACE);
	int uvlay_idx;
	TransDataSlideUv *suv=sld->slideuv;
	float uv_tmp[2];
	LinkNode *fuv_link;

	len = 0.0f;

	tempsv = BLI_ghash_lookup(vertgh,nearest);

	centerVert = editedge_getSharedVert(tempsv->up, tempsv->down);
	upVert = editedge_getOtherVert(tempsv->up, centerVert);
	downVert = editedge_getOtherVert(tempsv->down, centerVert);

	len = MIN2(perc, len_v3v3(upVert->co,downVert->co));
	len = MAX2(len, 0);

	//Adjust Edgeloop
	if(prop) {
		look = vertlist;
		while(look) {
			EditVert *tempev;
			ev = look->link;
			tempsv = BLI_ghash_lookup(vertgh,ev);

			tempev = editedge_getOtherVert((perc>=0)?tempsv->up:tempsv->down, ev);
			interp_v3_v3v3(ev->co, tempsv->origvert.co, tempev->co, fabs(perc));

			if (uvlay_tot) { // XXX scene->toolsettings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) {
				for (uvlay_idx=0; uvlay_idx<uvlay_tot; uvlay_idx++) {
					suv = BLI_ghash_lookup( uvarray[uvlay_idx], ev );
					if (suv && suv->fuv_list && suv->uv_up && suv->uv_down) {
						interp_v2_v2v2(uv_tmp, suv->origuv,  (perc>=0)?suv->uv_up:suv->uv_down, fabs(perc));
						fuv_link = suv->fuv_list;
						while (fuv_link) {
							VECCOPY2D(((float *)fuv_link->link), uv_tmp);
							fuv_link = fuv_link->next;
						}
					}
				}
			}

			look = look->next;
		}
	}
	else {
		//Non prop code
		look = vertlist;
		while(look) {
			float newlen;
			ev = look->link;
			tempsv = BLI_ghash_lookup(vertgh,ev);
			newlen = (len / len_v3v3(editedge_getOtherVert(tempsv->up,ev)->co,editedge_getOtherVert(tempsv->down,ev)->co));
			if(newlen > 1.0) {newlen = 1.0;}
			if(newlen < 0.0) {newlen = 0.0;}
			if(flip == 0) {
				interp_v3_v3v3(ev->co, editedge_getOtherVert(tempsv->down,ev)->co, editedge_getOtherVert(tempsv->up,ev)->co, fabs(newlen));
				if (uvlay_tot) { // XXX scene->toolsettings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) {
					/* dont do anything if no UVs */
					for (uvlay_idx=0; uvlay_idx<uvlay_tot; uvlay_idx++) {
						suv = BLI_ghash_lookup( uvarray[uvlay_idx], ev );
						if (suv && suv->fuv_list && suv->uv_up && suv->uv_down) {
							interp_v2_v2v2(uv_tmp, suv->uv_down, suv->uv_up, fabs(newlen));
							fuv_link = suv->fuv_list;
							while (fuv_link) {
								VECCOPY2D(((float *)fuv_link->link), uv_tmp);
								fuv_link = fuv_link->next;
							}
						}
					}
				}
			} else{
				interp_v3_v3v3(ev->co, editedge_getOtherVert(tempsv->up,ev)->co, editedge_getOtherVert(tempsv->down,ev)->co, fabs(newlen));

				if (uvlay_tot) { // XXX scene->toolsettings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) {
					/* dont do anything if no UVs */
					for (uvlay_idx=0; uvlay_idx<uvlay_tot; uvlay_idx++) {
						suv = BLI_ghash_lookup( uvarray[uvlay_idx], ev );
						if (suv && suv->fuv_list && suv->uv_up && suv->uv_down) {
							interp_v2_v2v2(uv_tmp, suv->uv_up, suv->uv_down, fabs(newlen));
							fuv_link = suv->fuv_list;
							while (fuv_link) {
								VECCOPY2D(((float *)fuv_link->link), uv_tmp);
								fuv_link = fuv_link->next;
							}
						}
					}
				}
			}
			look = look->next;
		}

	}

	return 1;
}

int EdgeSlide(TransInfo *t, short mval[2])
{
	char str[50];
	float final;

	final = t->values[0];

	snapGrid(t, &final);

	if (hasNumInput(&t->num)) {
		char c[20];

		applyNumInput(&t->num, &final);

		outputNumInput(&(t->num), c);

		sprintf(str, "Edge Slide Percent: %s", &c[0]);
	}
	else {
		sprintf(str, "Edge Slide Percent: %.2f", final);
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

	t->flag |= T_NO_CONSTRAINT;
}

int BoneRoll(TransInfo *t, short mval[2])
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

		final *= (float)(M_PI / 180.0);
	}
	else {
		sprintf(str, "Roll: %.2f", 180.0*final/M_PI);
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
}

int BakeTime(TransInfo *t, short mval[2])
{
	TransData *td = t->data;
	float time;
	int i;
	char str[50];

	float fac = 0.1f;

	if(t->mouse.precision) {
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

	for(i = 0 ; i < t->total; i++, td++) {
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

int Mirror(TransInfo *t, short mval[2])
{
	TransData *td;
	float size[3], mat[3][3];
	int i;
	char str[200];

	/*
	 * OPTIMISATION:
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

		for(i = 0, td=t->data; i < t->total; i++, td++) {
			if (td->flag & TD_NOACTION)
				break;

			if (td->flag & TD_SKIP)
				continue;

			ElementResize(t, td, mat);
		}

		recalcData(t);

		ED_area_headerprint(t->sa, str);
	}
	else
	{
		size[0] = size[1] = size[2] = 1;

		size_to_mat3( mat,size);

		for(i = 0, td=t->data; i < t->total; i++, td++) {
			if (td->flag & TD_NOACTION)
				break;

			if (td->flag & TD_SKIP)
				continue;

			ElementResize(t, td, mat);
		}

		recalcData(t);

		if(t->flag & T_2D_EDIT)
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

int Align(TransInfo *t, short mval[2])
{
	TransData *td = t->data;
	float center[3];
	int i;

	/* saving original center */
	VECCOPY(center, t->center);

	for(i = 0 ; i < t->total; i++, td++)
	{
		float mat[3][3], invmat[3][3];

		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		/* around local centers */
		if (t->flag & (T_OBJECT|T_POSE)) {
			VECCOPY(t->center, td->center);
		}
		else {
			if(t->settings->selectmode & SCE_SELECT_FACE) {
				VECCOPY(t->center, td->center);
			}
		}

		invert_m3_m3(invmat, td->axismtx);

		mul_m3_m3m3(mat, t->spacemtx, invmat);

		ElementRotation(t, td, mat, t->around);
	}

	/* restoring original center */
	VECCOPY(t->center, center);

	recalcData(t);

	ED_area_headerprint(t->sa, "Align");

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

	/* currently, some of these are only for the action editor */
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
		// TRANSFORM_FIX_ME This needs to use proper defines for t->modifiers
//		// FIXME: this still toggles the modes...
//		if (ctrl)
//			autosnap= SACTSNAP_STEP;
//		else if (shift)
//			autosnap= SACTSNAP_FRAME;
//		else if (alt)
//			autosnap= SACTSNAP_MARKER;
//		else
			autosnap= SACTSNAP_OFF;
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

	/* currently, some of these are only for the action editor */
	if (t->spacetype == SPACE_ACTION) {
		SpaceAction *saction= (SpaceAction *)t->sa->spacedata.first;

		drawtime = (saction->flag & SACTION_DRAWTIME)? 1 : 0;
	}
	else if (t->spacetype == SPACE_NLA) {
		SpaceNla *snla= (SpaceNla *)t->sa->spacedata.first;

		drawtime = (snla->flag & SNLA_DRAWTIME)? 1 : 0;
	}
	else {
		drawtime = 0;
	}

	return drawtime;
}


/* This function is used by Animation Editor specific transform functions to do
 * the Snap Keyframe to Nearest Frame/Marker
 */
static void doAnimEdit_SnapFrame(TransInfo *t, TransData *td, AnimData *adt, short autosnap)
{
	/* snap key to nearest frame? */
	if (autosnap == SACTSNAP_FRAME) {
		const Scene *scene= t->scene;
		const short doTime= getAnimEdit_DrawTime(t);
		const double secf= FPS;
		double val;

		/* convert frame to nla-action time (if needed) */
		if (adt)
			val= BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_MAP);
		else
			val= *(td->val);

		/* do the snapping to nearest frame/second */
		if (doTime)
			val= (float)( floor((val/secf) + 0.5f) * secf );
		else
			val= (float)( floor(val+0.5f) );

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
}

/* ----------------- Translation ----------------------- */

void initTimeTranslate(TransInfo *t)
{
	t->mode = TFM_TIME_TRANSLATE;
	t->transform = TimeTranslate;

	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	/* num-input has max of (n-1) */
	t->idx_max = 0;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	/* initialise snap like for everything else */
	t->snap[0] = 0.0f;
	t->snap[1] = t->snap[2] = 1.0f;
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
				val= floor(val/secf + 0.5f);
			else
				val= floor(val + 0.5f);
		}
		else {
			if (doTime)
				val= val / secf;
		}

		sprintf(&tvec[0], "%.4f", val);
	}

	sprintf(str, "DeltaX: %s", &tvec[0]);
}

static void applyTimeTranslate(TransInfo *t, float sval)
{
	TransData *td = t->data;
	Scene *scene = t->scene;
	int i;

	const short doTime= getAnimEdit_DrawTime(t);
	const double secf= FPS;

	const short autosnap= getAnimEdit_SnapMode(t);

	float deltax, val;

	/* it doesn't matter whether we apply to t->data or t->data2d, but t->data2d is more convenient */
	for (i = 0 ; i < t->total; i++, td++) {
		/* it is assumed that td->extra is a pointer to the AnimData,
		 * whose active action is where this keyframe comes from
		 * (this is only valid when not in NLA)
		 */
		AnimData *adt= (t->spacetype != SPACE_NLA) ? td->extra : NULL;

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
		doAnimEdit_SnapFrame(t, td, adt, autosnap);
	}
}

int TimeTranslate(TransInfo *t, short mval[2])
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

	t->mode = TFM_TIME_SLIDE;
	t->transform = TimeSlide;
	t->flag |= T_FREE_CUSTOMDATA;

	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	/* num-input has max of (n-1) */
	t->idx_max = 0;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	/* initialise snap like for everything else */
	t->snap[0] = 0.0f;
	t->snap[1] = t->snap[2] = 1.0f;
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

int TimeSlide(TransInfo *t, short mval[2])
{
	View2D *v2d = (View2D *)t->view;
	float cval[2], sval[2];
	float minx= *((float *)(t->customData));
	float maxx= *((float *)(t->customData) + 1);
	char str[200];

	/* calculate mouse co-ordinates */
	UI_view2d_region_to_view(v2d, mval[0], mval[0], &cval[0], &cval[1]);
	UI_view2d_region_to_view(v2d, t->imval[0], t->imval[0], &sval[0], &sval[1]);

	/* t->values[0] stores cval[0], which is the current mouse-pointer location (in frames) */
	t->values[0] = cval[0];

	/* handle numeric-input stuff */
	t->vec[0] = 2.0f*(cval[0]-sval[0]) / (maxx-minx);
	applyNumInput(&t->num, &t->vec[0]);
	t->values[0] = (maxx-minx) * t->vec[0] / 2.0 + sval[0];

	headerTimeSlide(t, sval[0], str);
	applyTimeSlide(t, sval[0]);

	recalcData(t);

	ED_area_headerprint(t->sa, str);

	return 1;
}

/* ----------------- Scaling ----------------------- */

void initTimeScale(TransInfo *t)
{
	t->mode = TFM_TIME_SCALE;
	t->transform = TimeScale;

	initMouseInputMode(t, &t->mouse, INPUT_NONE);
	t->helpline = HLP_SPRING; /* set manually because we don't use a predefined input */

	t->flag |= T_NULL_ONE;
	t->num.flag |= NUM_NULL_ONE;

	/* num-input has max of (n-1) */
	t->idx_max = 0;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	/* initialise snap like for everything else */
	t->snap[0] = 0.0f;
	t->snap[1] = t->snap[2] = 1.0f;
}

static void headerTimeScale(TransInfo *t, char *str) {
	char tvec[60];

	if (hasNumInput(&t->num))
		outputNumInput(&(t->num), tvec);
	else
		sprintf(&tvec[0], "%.4f", t->values[0]);

	sprintf(str, "ScaleX: %s", &tvec[0]);
}

static void applyTimeScale(TransInfo *t) {
	Scene *scene = t->scene;
	TransData *td = t->data;
	int i;

	const short autosnap= getAnimEdit_SnapMode(t);
	const short doTime= getAnimEdit_DrawTime(t);
	const double secf= FPS;


	for (i = 0 ; i < t->total; i++, td++) {
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
		doAnimEdit_SnapFrame(t, td, adt, autosnap);
	}
}

int TimeScale(TransInfo *t, short mval[2])
{
	float cval, sval;
	float deltax, startx;
	float width= 0.0f;
	char str[200];

	sval= t->imval[0];
	cval= mval[0];

	/* calculate scaling factor */
	startx= sval-(width/2+(t->ar->winx)/2);
	deltax= cval-(width/2+(t->ar->winx)/2);
	t->values[0] = deltax / startx;

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

void BIF_TransformSetUndo(char *str)
{
	// TRANSFORM_FIX_ME
	//Trans.undostr= str;
}


void NDofTransform()
{
#if 0 // TRANSFORM_FIX_ME
    float fval[7];
    float maxval = 50.0f; // also serves as threshold
    int axis = -1;
    int mode = 0;
    int i;

	getndof(fval);

	for(i = 0; i < 6; i++)
	{
		float val = fabs(fval[i]);
		if (val > maxval)
		{
			axis = i;
			maxval = val;
		}
	}

	switch(axis)
	{
		case -1:
			/* No proper axis found */
			break;
		case 0:
		case 1:
		case 2:
			mode = TFM_TRANSLATION;
			break;
		case 4:
			mode = TFM_ROTATION;
			break;
		case 3:
		case 5:
			mode = TFM_TRACKBALL;
			break;
		default:
			printf("ndof: what we are doing here ?");
	}

	if (mode != 0)
	{
		initTransform(mode, CTX_NDOF);
		Transform();
	}
#endif
}
