/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"	/* PET modes			*/
#include "DNA_screen_types.h"	/* area dimensions		*/
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_ipo_types.h"		/* some silly ipo flag	*/
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BIF_editview.h"		/* arrows_move_cursor	*/
#include "BIF_gl.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"			/* undo					*/
#include "BIF_toets.h"			/* persptoetsen			*/
#include "BIF_mywindow.h"		/* warp_pointer			*/
#include "BIF_toolbox.h"		/* notice				*/
#include "BIF_editmesh.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_bad_level_calls.h"/* popmenu and error	*/

#include "BDR_editobject.h"		/* compatible_eul		*/

#include "BSE_view.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "PIL_time.h"			/* sleep				*/

#include "blendef.h"

#include "mydevice.h"

#include "transform.h"

/* GLOBAL VARIABLE THAT SHOULD MOVED TO SCREEN MEMBER OR SOMETHING  */
TransInfo Trans = {TFM_INIT, 0};	// enforce init on first usage
ListBase CSpaces = {0,0};
float MatSpace[3][3];

/* ************************** Dashed help line **************************** */


/* bad frontbuffer call... because it is used in transform after force_draw() */
static void helpline(TransInfo *t, float *vec)
{
	float vecrot[3], cent[2];
	short mval[2];
	
	VECCOPY(vecrot, vec);
	if(t->flag & T_EDIT) {
		Object *ob=G.obedit;
		if(ob) Mat4MulVecfl(ob->obmat, vecrot);
	}
	else if(t->flag & T_POSE) {
		Object *ob=t->poseobj;
		if(ob) Mat4MulVecfl(ob->obmat, vecrot);
	}
	
	getmouseco_areawin(mval);
	project_float(vecrot, cent);	// no overflow in extreme cases
	if(cent[0]!=3200.0f) {
		persp(PERSP_WIN);
		
		glDrawBuffer(GL_FRONT);
		
		BIF_ThemeColor(TH_WIRE);
		
		setlinestyle(3);
		glBegin(GL_LINE_STRIP); 
		glVertex2sv(mval); 
		glVertex2fv(cent); 
		glEnd();
		setlinestyle(0);
		
		persp(PERSP_VIEW);
		glFlush(); // flush display for frontbuffer
		glDrawBuffer(GL_BACK);
	}
}

/* ************************** TRANSFORMATIONS **************************** */

void BIF_selectOrientation() {
	short val;
	val= pupmenu("Orientation%t|Global|Local|Normal|View");
	if(val>0) {
		if(val==1) G.vd->twmode= V3D_MANIP_GLOBAL;
		else if(val==2) G.vd->twmode= V3D_MANIP_LOCAL;
		else if(val==3) G.vd->twmode= V3D_MANIP_NORMAL;
		else if(val==4) G.vd->twmode= V3D_MANIP_VIEW;
	}
}

static void view_editmove(unsigned short event)
{
	int refresh = 0;
	/* Regular:   Zoom in */
	/* Shift:     Scroll up */
	/* Ctrl:      Scroll right */
	/* Alt-Shift: Rotate up */
	/* Alt-Ctrl:  Rotate right */
	
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

	if (refresh) {
		Mat4CpyMat4(Trans.viewmat, G.vd->viewmat);
		Mat4CpyMat4(Trans.viewinv, G.vd->viewinv);
		Mat4CpyMat4(Trans.persinv, G.vd->persinv);
	}
}

void checkFirstTime() {
	if(Trans.mode==TFM_INIT) {
		memset(&Trans, 0, sizeof(TransInfo));
		Trans.propsize = 1.0;
	}
}

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
		case TFM_CREASE:
			return "Crease";
	}
	return "Transform";
}

/* ************************************************* */

static void transformEvent(unsigned short event, short val) {
	float mati[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
	char cmode = constraintModeToChar(&Trans);

	if (val) {
		switch (event){
		/* enforce redraw of transform when modifiers are used */
		case LEFTCTRLKEY:
		case RIGHTCTRLKEY:
			Trans.redraw = 1;
			break;
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			/* shift is modifier for higher resolution transform, works nice to store this mouse position */
			getmouseco_areawin(Trans.shiftmval);
			Trans.flag |= T_SHIFT_MOD;
			Trans.redraw = 1;
			break;

		case SPACEKEY:
			if (G.qual & LR_ALTKEY) {
				short mval[2];

				getmouseco_sc(mval);
				BIF_selectOrientation();
				calc_manipulator_stats(curarea);
				Mat3CpyMat4(Trans.spacemtx, G.vd->twmat);
				warp_pointer(mval[0], mval[1]);
			}
			else {
				Trans.state = TRANS_CONFIRM;
			}
			break;

			
		case MIDDLEMOUSE:
			if ((Trans.flag & T_NO_CONSTRAINT)==0) {
				/* exception for switching to dolly, or trackball, in camera view */
				if (Trans.flag & T_CAMERA) {
					if (Trans.mode==TFM_TRANSLATION)
						setLocalConstraint(&Trans, (CON_AXIS2), "along local Z");
					else if (Trans.mode==TFM_ROTATION) {
						restoreTransObjects(&Trans);
						initTransModeFlags(&Trans, TFM_TRACKBALL);
						initTrackball(&Trans);
					}
				}
				else {
					Trans.flag |= T_MMB_PRESSED;
					if (Trans.con.mode & CON_APPLY) {
						stopConstraint(&Trans);
					}
					else {
						initSelectConstraint(&Trans, Trans.spacemtx);
						postSelectConstraint(&Trans);
					}
				}
				Trans.redraw = 1;
			}
			break;
		case ESCKEY:
		case RIGHTMOUSE:
			Trans.state = TRANS_CANCEL;
			break;
		case LEFTMOUSE:
		case PADENTER:
		case RETKEY:
			Trans.state = TRANS_CONFIRM;
			break;
		case GKEY:
			restoreTransObjects(&Trans);
			initTransModeFlags(&Trans, TFM_TRANSLATION);
			initTranslation(&Trans);
			Trans.redraw = 1;
			break;
		case SKEY:
			restoreTransObjects(&Trans);
			initTransModeFlags(&Trans, TFM_RESIZE);
			initResize(&Trans);
			Trans.redraw = 1;
			break;
		case RKEY:
			if (Trans.mode == TFM_ROTATION) {
				restoreTransObjects(&Trans);
				initTransModeFlags(&Trans, TFM_TRACKBALL);
				initTrackball(&Trans);
			}
			else {
				restoreTransObjects(&Trans);
				initTransModeFlags(&Trans, TFM_ROTATION);
				initRotation(&Trans);
			}
			Trans.redraw = 1;
			break;
		case CKEY:
			if (G.qual & LR_ALTKEY) {
				Trans.flag ^= T_PROP_CONNECTED;
				sort_trans_data_dist(&Trans);
				calculatePropRatio(&Trans);
				Trans.redraw= 1;
			}
			else {
				stopConstraint(&Trans);
				Trans.redraw = 1;
			}
			break;
		case XKEY:
			if ((Trans.flag & T_NO_CONSTRAINT)==0) {
				if (cmode == 'X') {
					if (Trans.con.mode & CON_USER) {
						stopConstraint(&Trans);
					}
					else {
						if (G.qual == 0)
							setUserConstraint(&Trans, (CON_AXIS0), "along %s X");
						else if (G.qual == LR_SHIFTKEY)
							setUserConstraint(&Trans, (CON_AXIS1|CON_AXIS2), "locking %s X");
					}
				}
				else {
					if (G.qual == 0)
						setConstraint(&Trans, mati, (CON_AXIS0), "along global X");
					else if (G.qual == LR_SHIFTKEY)
						setConstraint(&Trans, mati, (CON_AXIS1|CON_AXIS2), "locking global X");
				}
				Trans.redraw = 1;
			}
			break;
		case YKEY:
			if ((Trans.flag & T_NO_CONSTRAINT)==0) {
				if (cmode == 'Y') {
					if (Trans.con.mode & CON_USER) {
						stopConstraint(&Trans);
					}
					else {
						if (G.qual == 0)
							setUserConstraint(&Trans, (CON_AXIS1), "along %s Y");
						else if (G.qual == LR_SHIFTKEY)
							setUserConstraint(&Trans, (CON_AXIS0|CON_AXIS2), "locking %s Y");
					}
				}
				else {
					if (G.qual == 0)
						setConstraint(&Trans, mati, (CON_AXIS1), "along global Y");
					else if (G.qual == LR_SHIFTKEY)
						setConstraint(&Trans, mati, (CON_AXIS0|CON_AXIS2), "locking global Y");
				}
				Trans.redraw = 1;
			}
			break;
		case ZKEY:
			if ((Trans.flag & T_NO_CONSTRAINT)==0) {
				if (cmode == 'Z') {
					if (Trans.con.mode & CON_USER) {
						stopConstraint(&Trans);
					}
					else {
						if (G.qual == 0)
							setUserConstraint(&Trans, (CON_AXIS2), "along %s Z");
						else if (G.qual == LR_SHIFTKEY)
							setUserConstraint(&Trans, (CON_AXIS0|CON_AXIS1), "locking %s Z");
					}
				}
				else {
					if (G.qual == 0)
						setConstraint(&Trans, mati, (CON_AXIS2), "along global Z");
					else if (G.qual == LR_SHIFTKEY)
						setConstraint(&Trans, mati, (CON_AXIS0|CON_AXIS1), "locking global Z");
				}
				Trans.redraw = 1;
			}
			break;
		case OKEY:
			if (Trans.flag & T_PROP_EDIT && G.qual==LR_SHIFTKEY) {
				G.scene->prop_mode = (G.scene->prop_mode+1)%6;
				calculatePropRatio(&Trans);
				Trans.redraw= 1;
			}
			break;
		case PADPLUSKEY:
			if(G.qual & LR_ALTKEY && Trans.flag & T_PROP_EDIT) {
				Trans.propsize*= 1.1f;
				calculatePropRatio(&Trans);
			}
			Trans.redraw= 1;
			break;
		case PAGEUPKEY:
		case WHEELDOWNMOUSE:
			if(Trans.flag & T_PROP_EDIT) {
				Trans.propsize*= 1.1f;
				calculatePropRatio(&Trans);
			}
			else view_editmove(event);
			Trans.redraw= 1;
			break;
		case PADMINUS:
			if(G.qual & LR_ALTKEY && Trans.flag & T_PROP_EDIT) {
				Trans.propsize*= 0.90909090f;
				calculatePropRatio(&Trans);
			}
			Trans.redraw= 1;
			break;
		case PAGEDOWNKEY:
		case WHEELUPMOUSE:
			if(Trans.flag & T_PROP_EDIT) {
				Trans.propsize*= 0.90909090f;
				calculatePropRatio(&Trans);
			}
			else view_editmove(event);
			Trans.redraw= 1;
			break;
		}
		Trans.redraw |= handleNumInput(&(Trans.num), event);
		arrows_move_cursor(event);
	}
	else {
		switch (event){
		/* no redraw on release modifier keys! this makes sure you can assign the 'grid' still 
		   after releasing modifer key */
		case MIDDLEMOUSE:
			if ((Trans.flag & T_NO_CONSTRAINT)==0) {
				Trans.flag &= ~T_MMB_PRESSED;
				postSelectConstraint(&Trans);
				Trans.redraw = 1;
			}
			break;
		case LEFTMOUSE:
		case RIGHTMOUSE:
			/* commented out, doesn't work for actions started with menu */
			// Trans.state = TRANS_CONFIRM;
			break;
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			/* shift is modifier for higher resolution transform */
			Trans.flag &= ~T_SHIFT_MOD;
			break;
		}
	}
}

void initTransform(int mode, int context) {
	/* added initialize, for external calls to set stuff in TransInfo, like undo string */
	checkFirstTime();

	Trans.state = TRANS_RUNNING;

	Trans.context = context;

	calc_manipulator_stats(curarea);
	Mat3CpyMat4(Trans.spacemtx, G.vd->twmat);

	initTrans(&Trans);					// internal data, mouse, vectors

	initTransModeFlags(&Trans, mode);	// modal settings in struct Trans

	createTransData(&Trans);			// make TransData structs from selection

	if (Trans.total == 0) {
		postTrans(&Trans);
		return;
	}

	/* EVIL! posemode code can switch translation to rotate when 1 bone is selected. will be removed (ton) */
	/* EVIL2: we gave as argument also texture space context bit... was cleared */
	mode= Trans.mode;
	
	calculatePropRatio(&Trans);
	calculateCenter(&Trans);

	switch (mode) {
	case TFM_TRANSLATION:
		initTranslation(&Trans);
		break;
	case TFM_ROTATION:
		initRotation(&Trans);
		break;
	case TFM_RESIZE:
		initResize(&Trans);
		break;
	case TFM_TOSPHERE:
		initToSphere(&Trans);
		break;
	case TFM_SHEAR:
		initShear(&Trans);
		break;
	case TFM_WARP:
		initWarp(&Trans);
		break;
	case TFM_SHRINKFATTEN:
		initShrinkFatten(&Trans);
		break;
	case TFM_TILT:
		initTilt(&Trans);
		break;
	case TFM_TRACKBALL:
		initTrackball(&Trans);
		break;
	case TFM_PUSHPULL:
		initPushPull(&Trans);
		break;
	case TFM_CREASE:
		initCrease(&Trans);
		break;
	case TFM_BONESIZE:
		initBoneSize(&Trans);
		break;
	}
}

void Transform() 
{
	short pmval[2] = {0, 0}, mval[2], val;
	unsigned short event;

	if(Trans.total==0) return;	// added, can happen now! (ton)
	
	// Emptying event queue
	while( qtest() ) {
		event= extern_qread(&val);
	}

	Trans.redraw = 1; /* initial draw */

	while (Trans.state == TRANS_RUNNING) {

		getmouseco_areawin(mval);
		
		if (mval[0] != pmval[0] || mval[1] != pmval[1]) {
			if (Trans.flag & T_MMB_PRESSED) {
				initSelectConstraint(&Trans, Trans.spacemtx);
			}
			Trans.redraw = 1;
		}
		if (Trans.redraw) {
			pmval[0] = mval[0];
			pmval[1] = mval[1];

			selectConstraint(&Trans);
			if (Trans.transform) {
				Trans.transform(&Trans, mval);  // calls recalcData()
			}
			Trans.redraw = 0;
		}
		
		/* essential for idling subloop */
		if( qtest()==0) PIL_sleep_ms(2);

		while( qtest() ) {
			event= extern_qread(&val);
			transformEvent(event, val);
		}
	}
	
	
	/* handle restoring objects and Undo */
	if(Trans.state == TRANS_CANCEL) {
		restoreTransObjects(&Trans);	// calls recalcData()
		if(Trans.undostr) BIF_undo_push(Trans.undostr);
	}
	else {
		if(Trans.undostr) BIF_undo_push(Trans.undostr);
		else BIF_undo_push(transform_to_undostr(&Trans));
	}
	Trans.undostr= NULL;
	
	/* free data */
	postTrans(&Trans);

	/* aftertrans does insert ipos and action channels, and clears base flags, doesnt read transdata */
	special_aftertrans_update(&Trans);

	/* send events out for redraws */
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	scrarea_queue_headredraw(curarea);
}

/* ************************** Manipulator init and main **************************** */

void initManipulator(int mode)
{
	Trans.state = TRANS_RUNNING;

	Trans.context = CTX_NONE;

	initTrans(&Trans);					// internal data, mouse, vectors

	initTransModeFlags(&Trans, mode);	// modal settings in struct Trans

	G.moving |= G_TRANSFORM_MANIP;		// signal to draw manipuls while transform
	createTransData(&Trans);			// make TransData structs from selection

	if (Trans.total == 0)
		return;

	/* EVIL! posemode code can switch translation to rotate when 1 bone is selected. will be removed (ton) */
	/* EVIL2: we gave as argument also texture space context bit... was cleared */
	mode= Trans.mode;
	
	calculatePropRatio(&Trans);
	calculateCenter(&Trans);

	switch (mode) {
	case TFM_TRANSLATION:
		initTranslation(&Trans);
		break;
	case TFM_ROTATION:
		initRotation(&Trans);
		break;
	case TFM_RESIZE:
		initResize(&Trans);
		break;
	case TFM_TRACKBALL:
		initTrackball(&Trans);
		break;
	}

	Trans.flag |= T_USES_MANIPULATOR;
}

void ManipulatorTransform() 
{
	int mouse_moved = 0;
	short pmval[2] = {0, 0}, mval[2], val;
	unsigned short event;

	if (Trans.total == 0)
		return;

	Trans.redraw = 1; /* initial draw */

	while (Trans.state == TRANS_RUNNING) {
		
		getmouseco_areawin(mval);
		
		if (mval[0] != pmval[0] || mval[1] != pmval[1]) {
			Trans.redraw = 1;
		}
		if (Trans.redraw) {
			pmval[0] = mval[0];
			pmval[1] = mval[1];

			//selectConstraint(&Trans);  needed?
			if (Trans.transform) {
				Trans.transform(&Trans, mval);
			}
			Trans.redraw = 0;
		}
		
		/* essential for idling subloop */
		if( qtest()==0) PIL_sleep_ms(2);

		while( qtest() ) {
			event= extern_qread(&val);

			switch (event){
			case MOUSEX:
			case MOUSEY:
				mouse_moved = 1;
				break;
			/* enforce redraw of transform when modifiers are used */
			case LEFTCTRLKEY:
			case RIGHTCTRLKEY:
				if(val) Trans.redraw = 1;
				break;
			case LEFTSHIFTKEY:
			case RIGHTSHIFTKEY:
				/* shift is modifier for higher resolution transform, works nice to store this mouse position */
				if(val) {
					getmouseco_areawin(Trans.shiftmval);
					Trans.flag |= T_SHIFT_MOD;
					Trans.redraw = 1;
				}
				else Trans.flag &= ~T_SHIFT_MOD; 
				break;
				
			case ESCKEY:
			case RIGHTMOUSE:
				Trans.state = TRANS_CANCEL;
				break;
			case LEFTMOUSE:
				if(mouse_moved==0 && val==0) break;
				// else we pass on event to next, which cancels
			case SPACEKEY:
			case PADENTER:
			case RETKEY:
				Trans.state = TRANS_CONFIRM;
				break;
			}
			if(val) {
				switch(event) {
				case WHEELDOWNMOUSE:
				case PADPLUSKEY:
					if(Trans.flag & T_PROP_EDIT) {
						Trans.propsize*= 1.1f;
						calculatePropRatio(&Trans);
						Trans.redraw= 1;
					}
					break;
				case WHEELUPMOUSE:
				case PADMINUS:
					if(Trans.flag & T_PROP_EDIT) {
						Trans.propsize*= 0.90909090f;
						calculatePropRatio(&Trans);
						Trans.redraw= 1;
					}
					break;
				}			
			}
		}
	}
	
	if(Trans.state == TRANS_CANCEL) {
		restoreTransObjects(&Trans);
	}
	else {
		BIF_undo_push(transform_to_undostr(&Trans));
	}
	
	/* free data, reset vars */
	postTrans(&Trans);
	
	/* aftertrans does insert ipos and action channels, and clears base flags */
	special_aftertrans_update(&Trans);
	
	/* send events out for redraws */
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	scrarea_queue_headredraw(curarea);
}

/* ************************** TRANSFORMATIONS **************************** */

/* ************************** WRAP *************************** */

/* warp is done fully in view space */
void initWarp(TransInfo *t) 
{
	float max[3], min[3];
	int i;
	
	calculateCenterCursor(t);
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->transform = Warp;
	t->snap[0] = 0.0f;
	t->snap[1] = 5.0f;
	t->snap[2] = 1.0f;
	
	t->fac = (float)(t->center2d[0] - t->imval[0]);
	
	/* we need min/max in view space */
	for(i = 0; i < t->total; i++) {
		float center[3];
		VECCOPY(center, t->data[i].center);
		Mat3MulVecfl(t->data[i].mtx, center);
		Mat4MulVecfl(G.vd->viewmat, center);
		VecSubf(center, center, G.vd->viewmat[3]);
		if (i)
			MinMax3(min, max, center);
		else {
			VECCOPY(max, center);
			VECCOPY(min, center);
		}
	}

	t->center[0]= (min[0]+max[0])/2.0f;
	t->center[1]= (min[1]+max[1])/2.0f;
	t->center[2]= (min[2]+max[2])/2.0f;
	
	t->val= (max[0]-min[0])/2.0f;	// t->val is free variable
}


int Warp(TransInfo *t, short mval[2])
{
	TransData *td = t->data;
	float vec[3], circumfac, dist, phi0, co, si, *curs, cursor[3], gcursor[3];
	int i;
	char str[50];
	
	curs= give_cursor();
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
		VecSubf(cursor, cursor, G.obedit->obmat[3]);
		VecSubf(gcursor, gcursor, G.obedit->obmat[3]);
		Mat3MulVecfl(t->data->smtx, gcursor);
	}
	Mat4MulVecfl(G.vd->viewmat, cursor);
	VecSubf(cursor, cursor, G.vd->viewmat[3]);

	// amount of degrees for warp, 450 = allow to create 360 degree warp
	circumfac= 450.0f*(mval[1] - t->imval[1]) / (float)(curarea->winy);
	circumfac+= 90.0f;

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
	
	for(i = 0 ; i < t->total; i++, td++) {
		float loc[3];
		if (td->flag & TD_NOACTION)
			break;

		/* translate point to centre, rotate in such a way that outline==distance */
		
		VECCOPY(vec, td->iloc);
		Mat3MulVecfl(td->mtx, vec);
		Mat4MulVecfl(G.vd->viewmat, vec);
		VecSubf(vec, vec, G.vd->viewmat[3]);
		
		dist= vec[0]-cursor[0];

		phi0= (circumfac*dist/t->val);	// t->val is X dimension projected boundbox
		
		vec[1]= (vec[1]-cursor[1]);
		
		co= (float)cos(phi0);
		si= (float)sin(phi0);
		loc[0]= -si*vec[1]+cursor[0];
		loc[1]= co*vec[1]+cursor[1];
		loc[2]= vec[2];
		
		Mat4MulVecfl(G.vd->viewinv, loc);
		VecSubf(loc, loc, G.vd->viewinv[3]);
		Mat3MulVecfl(td->smtx, loc);

		VecSubf(loc, loc, td->iloc);
		VecMulf(loc, td->factor);
		VecAddf(td->loc, td->iloc, loc);
	}

	recalcData(t);
	
	headerprint(str);
	
	force_draw(0);
	
	helpline(t, gcursor);
	
	return 1;
}

/* ************************** SHEAR *************************** */

void initShear(TransInfo *t) 
{
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	t->transform = Shear;
	t->fac = (float)(t->center2d[0] - t->imval[0]);
}

int Shear(TransInfo *t, short mval[2]) 
{
	TransData *td = t->data;
	float vec[3];
	float smat[3][3], tmat[3][3], totmat[3][3], persmat[3][3], persinv[3][3];
	float value;
	int i;
	char str[50];

	Mat3CpyMat4(persmat, G.vd->viewmat);
	Mat3Inv(persinv, persmat);

	value = -0.005f * ((float)(t->center2d[0] - mval[0]) - t->fac);

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
	
	Mat3One(smat);
	smat[1][0] = value;
	Mat3MulMat3(tmat, smat, persmat);
	Mat3MulMat3(totmat, persinv, tmat);
	
	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (G.obedit) {
			float mat3[3][3];
			Mat3MulMat3(mat3, totmat, td->mtx);
			Mat3MulMat3(tmat, td->smtx, mat3);
		}
		else {
			Mat3CpyMat3(tmat, totmat);
		}
		VecSubf(vec, td->center, t->center);

		Mat3MulVecfl(tmat, vec);

		VecAddf(vec, vec, t->center);
		VecSubf(vec, vec, td->center);

		VecMulf(vec, td->factor);

		VecAddf(td->loc, td->iloc, vec);
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t, t->center);

	return 1;
}

/* ************************** RESIZE *************************** */

void initResize(TransInfo *t) 
{
	t->fac = (float)sqrt( (float)
		(
			(t->center2d[1] - t->imval[1])*(t->center2d[1] - t->imval[1])
		+
			(t->center2d[0] - t->imval[0])*(t->center2d[0] - t->imval[0])
		) );

	if(t->fac==0.0f) t->fac= 1.0f;	// prevent Inf
	
	t->idx_max = 2;
	t->num.idx_max = 2;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	t->transform = Resize;
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
			sprintf(str, "Size: %s%s %s", &tvec[0], t->con.text, t->proptext);
			break;
		case 1:
			sprintf(str, "Size: %s : %s%s %s", &tvec[0], &tvec[20], t->con.text, t->proptext);
			break;
		case 2:
			sprintf(str, "Size: %s : %s : %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
		}
	}
	else {
		sprintf(str, "Size X: %s   Y: %s  Z: %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
	}
}

#define SIGN(a)		(a<-FLT_EPSILON?1:a>FLT_EPSILON?2:3)
#define VECSIGNFLIP(a, b) ((SIGN(a[0]) & SIGN(b[0]))==0 || (SIGN(a[1]) & SIGN(b[1]))==0 || (SIGN(a[2]) & SIGN(b[2]))==0)

/* smat is reference matrix, only scaled */
static void TransMat3ToSize( float mat[][3], float smat[][3], float *size)
{
	float vec[3];
	
	VecCopyf(vec, mat[0]);
	size[0]= Normalise(vec);
	VecCopyf(vec, mat[1]);
	size[1]= Normalise(vec);
	VecCopyf(vec, mat[2]);
	size[2]= Normalise(vec);
	
	/* first tried with dotproduct... but the sign flip is crucial */
	if( VECSIGNFLIP(mat[0], smat[0]) ) size[0]= -size[0]; 
	if( VECSIGNFLIP(mat[1], smat[1]) ) size[1]= -size[1]; 
	if( VECSIGNFLIP(mat[2], smat[2]) ) size[2]= -size[2]; 
}


static void ElementResize(TransInfo *t, TransData *td, float mat[3][3]) {
	float tmat[3][3], smat[3][3], center[3];
	float vec[3];

	if (t->flag & T_EDIT) {
		Mat3MulMat3(smat, mat, td->mtx);
		Mat3MulMat3(tmat, td->smtx, smat);
	}
	else {
		Mat3CpyMat3(tmat, mat);
	}

	if (t->con.applySize) {
		t->con.applySize(t, td, tmat);
	}

	/* local constraint shouldn't alter center */
	if (G.vd->around == V3D_LOCAL) {
		if (t->flag & T_OBJECT) {
			VECCOPY(center, td->center);	// not supported in editmode yet
		}
		else {
			VECCOPY(center, t->center);		// Editmode needs to define center too...
		}
	}
	else {
		VECCOPY(center, t->center);
	}

	if (td->ext) {
		float fsize[3];

		if (t->flag & T_OBJECT) {
			float obsizemat[3][3];
			// Reorient the size mat to fit the oriented object.
			Mat3MulMat3(obsizemat, tmat, td->axismtx);
			//printmatrix3("obsizemat", obsizemat);
			TransMat3ToSize(obsizemat, td->axismtx, fsize);
			//printvecf("fsize", fsize);
		}
		else {
			Mat3ToSize(tmat, fsize);
		}
		
		if ((G.vd->flag & V3D_ALIGN)==0) {	// align mode doesn't rotate objects itself
			/* handle ipokeys? */
			if(td->tdi) {
				TransDataIpokey *tdi= td->tdi;
				/* calculate delta size (equal for size and dsize) */
				
				vec[0]= (tdi->oldsize[0])*(fsize[0] -1.0f) * td->factor;
				vec[1]= (tdi->oldsize[1])*(fsize[1] -1.0f) * td->factor;
				vec[2]= (tdi->oldsize[2])*(fsize[2] -1.0f) * td->factor;
				
				add_tdi_poin(tdi->sizex, tdi->oldsize,   vec[0]);
				add_tdi_poin(tdi->sizey, tdi->oldsize+1, vec[1]);
				add_tdi_poin(tdi->sizez, tdi->oldsize+2, vec[2]);
				
			}
			else if((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)){
				/* scale val and reset size */
 				*td->val = td->ival * fsize[0] * td->factor;

				td->ext->size[0] = td->ext->isize[0];
				td->ext->size[1] = td->ext->isize[1];
				td->ext->size[2] = td->ext->isize[2];
 			}
			else {
				/* Reset val if SINGLESIZE but using a constraint */
				if (td->flag & TD_SINGLESIZE)
	 				*td->val = td->ival;

				td->ext->size[0] = td->ext->isize[0] * (fsize[0]) * td->factor;
				td->ext->size[1] = td->ext->isize[1] * (fsize[1]) * td->factor;
				td->ext->size[2] = td->ext->isize[2] * (fsize[2]) * td->factor;
			}
		}
	}
	/* For individual element center, Editmode need to use iloc */
	if (t->flag & T_EDIT)
		VecSubf(vec, td->iloc, center);
	else
		VecSubf(vec, td->center, center);

	Mat3MulVecfl(tmat, vec);

	VecAddf(vec, vec, center);
	if (t->flag & T_EDIT)
		VecSubf(vec, vec, td->iloc);
	else
		VecSubf(vec, vec, td->center);

	VecMulf(vec, td->factor);

	if (t->flag & T_OBJECT) {
		Mat3MulVecfl(td->smtx, vec);
	}

	if(td->tdi) {
		TransDataIpokey *tdi= td->tdi;
		add_tdi_poin(tdi->locx, tdi->oldloc, vec[0]);
		add_tdi_poin(tdi->locy, tdi->oldloc+1, vec[1]);
		add_tdi_poin(tdi->locz, tdi->oldloc+2, vec[2]);
	}
	else VecAddf(td->loc, td->iloc, vec);
}

int Resize(TransInfo *t, short mval[2]) 
{
	TransData *td = t->data;
	float size[3], mat[3][3];
	float ratio;
	int i;
	char str[50];

	/* for manipulator, center handle, the scaling can't be done relative to center */
	if( (t->flag & T_USES_MANIPULATOR) && t->con.mode==0) {
		ratio = 1.0f - ((t->imval[0] - mval[0]) + (t->imval[1] - mval[1]))/100.0f;
	}
	else {
		
		if(t->flag & T_SHIFT_MOD) {
			/* calculate ratio for shiftkey pos, and for total, and blend these for precision */
			float dx= (float)(t->center2d[0] - t->shiftmval[0]);
			float dy= (float)(t->center2d[1] - t->shiftmval[1]);
			ratio = (float)sqrt( dx*dx + dy*dy)/t->fac;
			
			dx= (float)(t->center2d[0] - mval[0]);
			dy= (float)(t->center2d[1] - mval[1]);
			ratio+= 0.1f*(float)(sqrt( dx*dx + dy*dy)/t->fac -ratio);
			
		}
		else {
			float dx= (float)(t->center2d[0] - mval[0]);
			float dy= (float)(t->center2d[1] - mval[1]);
			ratio = (float)sqrt( dx*dx + dy*dy)/t->fac;
		}
		
		/* flip scale, but not for manipulator center handle */
		if	((t->center2d[0] - mval[0]) * (t->center2d[0] - t->imval[0]) + 
			 (t->center2d[1] - mval[1]) * (t->center2d[1] - t->imval[1]) < 0)
				ratio *= -1.0f;
	}
	
	size[0] = size[1] = size[2] = ratio;

	snapGrid(t, size);

	if (hasNumInput(&t->num)) {
		applyNumInput(&t->num, size);
		constraintNumInput(t, size);
	}

	SizeToMat3(size, mat);

	if (t->con.applySize) {
		t->con.applySize(t, NULL, mat);
	}

	Mat3CpyMat3(t->mat, mat);	// used in manipulator
	
	headerResize(t, size, str);

	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		ElementResize(t, td, mat);
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	if(!(t->flag & T_USES_MANIPULATOR)) helpline (t, t->center);

	return 1;
}

/* ************************** TOSPHERE *************************** */

void initToSphere(TransInfo *t) 
{
	TransData *td = t->data;
	int i;

	// Calculate average radius
	for(i = 0 ; i < t->total; i++, td++) {
		t->val += VecLenf(t->center, td->iloc);
	}

	t->val /= (float)t->total;

	Trans.fac = (float)sqrt( (float)
		(
			(Trans.center2d[1] - Trans.imval[1])*(Trans.center2d[1] - Trans.imval[1])
		+
			(Trans.center2d[0] - Trans.imval[0])*(Trans.center2d[0] - Trans.imval[0])
		) );

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	t->transform = ToSphere;
}



int ToSphere(TransInfo *t, short mval[2]) 
{
	float vec[3];
	float ratio, radius;
	int i;
	char str[50];
	TransData *td = t->data;

	ratio = (float)sqrt( (float)
		(
			(t->center2d[1] - mval[1])*(t->center2d[1] - mval[1])
		+
			(t->center2d[0] - mval[0])*(t->center2d[0] - mval[0])
		) ) / t->fac;

	snapGrid(t, &ratio);

	applyNumInput(&t->num, &ratio);

	if (ratio > 1.0f)
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

		VecSubf(vec, td->iloc, t->center);

		radius = Normalise(vec);

		tratio = 1.0f - ((1.0f - ratio) * td->factor);

		VecMulf(vec, radius * tratio + t->val * (1.0f - tratio));

		VecAddf(td->loc, t->center, vec);
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t, t->center);

	return 1;
}

/* ************************** ROTATION *************************** */


void initRotation(TransInfo *t) 
{
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;
	t->fac = 0;
	t->transform = Rotation;
}

static void ElementRotation(TransInfo *t, TransData *td, float mat[3][3]) {
	float vec[3], totmat[3][3], smat[3][3];
	float eul[3], fmat[3][3], quat[4];

	if (t->flag & T_EDIT) {
		Mat3MulMat3(totmat, mat, td->mtx);
		Mat3MulMat3(smat, td->smtx, totmat);
		
		VecSubf(vec, td->iloc, t->center);
		Mat3MulVecfl(smat, vec);
		
		VecAddf(td->loc, vec, t->center);

		if(td->flag & TD_USEQUAT) {
			Mat3MulSerie(fmat, td->mtx, mat, td->smtx, 0, 0, 0, 0, 0);
			Mat3ToQuat(fmat, quat);	// Actual transform
			QuatMul(td->ext->quat, quat, td->ext->iquat);
		}
	}
	else {
		/* translation */
		
		VecSubf(vec, td->center, t->center);
		Mat3MulVecfl(mat, vec);
		VecAddf(vec, vec, t->center);
		/* vec now is the location where the object has to be */
		VecSubf(vec, vec, td->center);
		Mat3MulVecfl(td->smtx, vec);
		
		if(td->tdi) {
			TransDataIpokey *tdi= td->tdi;
			add_tdi_poin(tdi->locx, tdi->oldloc, vec[0]);
			add_tdi_poin(tdi->locy, tdi->oldloc+1, vec[1]);
			add_tdi_poin(tdi->locz, tdi->oldloc+2, vec[2]);
		}
		else VecAddf(td->loc, td->iloc, vec);
		
		if(td->flag & TD_USEQUAT) {
			Mat3MulSerie(fmat, td->mtx, mat, td->smtx, 0, 0, 0, 0, 0);
			Mat3ToQuat(fmat, quat);	// Actual transform
			QuatMul(td->ext->quat, quat, td->ext->iquat);
		}
		else if ((G.vd->flag & V3D_ALIGN)==0) {	// align mode doesn't rotate objects itself
			float obmat[3][3];
			
			/* are there ipo keys? */
			if(td->tdi) {
				TransDataIpokey *tdi= td->tdi;
				float rot[3];
				
				/* calculate the total rotatation in eulers */
				VecAddf(eul, td->ext->irot, td->ext->drot);
				EulToMat3(eul, obmat);
				/* mat = transform, obmat = object rotation */
				Mat3MulMat3(fmat, mat, obmat);
				Mat3ToEul(fmat, eul);
				compatible_eul(eul, td->ext->irot);
				
				/* correct back for delta rot */
				if(tdi->flag & TOB_IPODROT) {
					VecSubf(rot, eul, td->ext->irot);
				}
				else {
					VecSubf(rot, eul, td->ext->drot);
				}
				
				VecMulf(rot, (float)(9.0/M_PI_2));
				VecSubf(rot, rot, tdi->oldrot);
				
				add_tdi_poin(tdi->rotx, tdi->oldrot, rot[0]);
				add_tdi_poin(tdi->roty, tdi->oldrot+1, rot[1]);
				add_tdi_poin(tdi->rotz, tdi->oldrot+2, rot[2]);
			}
			else {
				Mat3MulMat3(totmat, mat, td->mtx);
				Mat3MulMat3(smat, td->smtx, totmat);
				
				/* calculate the total rotatation in eulers */
				VecAddf(eul, td->ext->irot, td->ext->drot); /* we have to correct for delta rot */
				EulToMat3(eul, obmat);
				/* mat = transform, obmat = object rotation */
				Mat3MulMat3(fmat, smat, obmat);
				Mat3ToEul(fmat, eul);
				compatible_eul(eul, td->ext->irot);
				
				/* correct back for delta rot */
				VecSubf(eul, eul, td->ext->drot);
				/* and apply */
				VECCOPY(td->ext->rot, eul);
			}
		}
	}
}

static void applyRotation(TransInfo *t, float angle, float axis[3]) 
{
	TransData *td = t->data;
	float mat[3][3], center[3];
	int i;

	/* saving original center */
	if (G.vd->around == V3D_LOCAL) {
		VECCOPY(center, t->center);
	}

	VecRotToMat3(axis, angle, mat);

	for(i = 0 ; i < t->total; i++, td++) {

		if (td->flag & TD_NOACTION)
			break;
		
		/* local constraint shouldn't alter center */
		if (G.vd->around == V3D_LOCAL) {
			if (t->flag & T_OBJECT)
				VECCOPY(t->center, td->center);		// not supported in editmode yet
		}
		
		if (t->con.applyRot) {
			t->con.applyRot(t, td, axis);
			VecRotToMat3(axis, angle * td->factor, mat);
		}
		else if (t->flag & T_PROP_EDIT) {
			VecRotToMat3(axis, angle * td->factor, mat);
		}

		ElementRotation(t, td, mat);
	}

	/* restoring original center */
	if (G.vd->around == V3D_LOCAL) {
		VECCOPY(t->center, center);
	}
}

int Rotation(TransInfo *t, short mval[2]) 
{
	TransData *td = t->data;
	char str[50];

	float final;

	int dx2 = t->center2d[0] - mval[0];
	int dy2 = t->center2d[1] - mval[1];
	double B = sqrt(dx2*dx2+dy2*dy2);

	int dx1 = t->center2d[0] - t->imval[0];
	int dy1 = t->center2d[1] - t->imval[1];
	double A = sqrt(dx1*dx1+dy1*dy1);

	int dx3 = mval[0] - t->imval[0];
	int dy3 = mval[1] - t->imval[1];
		/* use doubles here, to make sure a "1.0" (no rotation) doesnt become 9.999999e-01, which gives 0.02 for acos */
	double deler= ((double)((dx1*dx1+dy1*dy1)+(dx2*dx2+dy2*dy2)-(dx3*dx3+dy3*dy3) ))
		/ (2.0 * (A*B?A*B:1.0));
	/* (A*B?A*B:1.0f) this takes care of potential divide by zero errors */

	float dphi;

	float axis[3];
	float mat[3][3];

	VECCOPY(axis, t->viewinv[2]);
	VecMulf(axis, -1.0f);
	Normalise(axis);

	dphi = saacos((float)deler);
	if( (dx1*dy2-dx2*dy1)>0.0 ) dphi= -dphi;

	if(G.qual & LR_SHIFTKEY) t->fac += dphi/30.0f;
	else t->fac += dphi;

	/*
	clamping angle between -2 PI and 2 PI (not sure if useful so commented out - theeth)
	if (t->fac >= 2 * M_PI)
		t->fac -= 2 * M_PI;
	else if (t->fac <= -2 * M_PI)
		t->fac -= -2 * M_PI;
	*/

	final = t->fac;

	snapGrid(t, &final);

	t->imval[0] = mval[0];
	t->imval[1] = mval[1];

	if (t->con.applyRot) {
		t->con.applyRot(t, NULL, axis);
	}

	if (hasNumInput(&t->num)) {
		char c[20];

		applyNumInput(&t->num, &final);

		outputNumInput(&(t->num), c);

		sprintf(str, "Rot: %s %s", &c[0], t->proptext);

		final *= (float)(M_PI / 180.0);
	}
	else {
		sprintf(str, "Rot: %.2f%s %s", 180.0*final/M_PI, t->con.text, t->proptext);
	}

	VecRotToMat3(axis, final * td->factor, mat);

	t->val = final;				// used in manipulator
	Mat3CpyMat3(t->mat, mat);	// used in manipulator
	
	applyRotation(t, final, axis);
	
	recalcData(t);

	headerprint(str);

	force_draw(0);

	if(!(t->flag & T_USES_MANIPULATOR)) helpline (t, t->center);

	return 1;
}


/* ************************** TRACKBALL *************************** */

void initTrackball(TransInfo *t) 
{
	t->idx_max = 1;
	t->num.idx_max = 1;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;
	t->fac = 0;
	t->transform = Trackball;
}

static void applyTrackball(TransInfo *t, float axis1[3], float axis2[3], float angles[2])
{
	TransData *td = t->data;
	float mat[3][3], smat[3][3], totmat[3][3];
	int i;

	VecRotToMat3(axis1, angles[0], smat);
	VecRotToMat3(axis2, angles[1], totmat);
	
	Mat3MulMat3(mat, smat, totmat);

	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (G.vd->around == V3D_LOCAL) {
			if (t->flag & T_OBJECT)
				VECCOPY(t->center, td->center);	// not supported in editmode yet
		}
		
		if (t->flag & T_PROP_EDIT) {
			VecRotToMat3(axis1, td->factor * angles[0], smat);
			VecRotToMat3(axis2, td->factor * angles[1], totmat);
			
			Mat3MulMat3(mat, smat, totmat);
		}

		ElementRotation(t, td, mat);
	}
}

int Trackball(TransInfo *t, short mval[2]) 
{
	char str[80];
	float axis1[3], axis2[3];
	float mat[3][3], totmat[3][3], smat[3][3];
	float phi[2];
	
	VECCOPY(axis1, t->persinv[0]);
	VECCOPY(axis2, t->persinv[1]);
	Normalise(axis1);
	Normalise(axis2);
	
	/* factore has to become setting or so */
	phi[0]= 0.01f*(float)( t->imval[1] - mval[1] );
	phi[1]= 0.01f*(float)( mval[0] - t->imval[0] );
	
	//if(G.qual & LR_SHIFTKEY) t->fac += dphi/30.0f;
	//else t->fac += dphi;
	
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
	
	VecRotToMat3(axis1, phi[0], smat);
	VecRotToMat3(axis2, phi[1], totmat);
	
	Mat3MulMat3(mat, smat, totmat);
	
	Mat3CpyMat3(t->mat, mat);	// used in manipulator
	
	applyTrackball(t, axis1, axis2, phi);
	
	recalcData(t);
	
	headerprint(str);
	
	force_draw(0);
	
	if(!(t->flag & T_USES_MANIPULATOR)) helpline (t, t->center);
	
	return 1;
}

/* ************************** TRANSLATION *************************** */
	
void initTranslation(TransInfo *t) 
{
	t->idx_max = 2;
	t->num.idx_max = 2;
	t->snap[0] = 0.0f;
	t->snap[1] = G.vd->gridview * 1.0f;
	t->snap[2] = t->snap[1] * 0.1f;
	t->transform = Translation;
	
	/* initgrabz() defines a factor for perspective depth correction, used in window_to_3d() */
	if (G.obedit) {
		float vec[3];
		
		VECCOPY(vec, t->center);
		Mat4MulVecfl(G.obedit->obmat, vec);
		initgrabz(vec[0], vec[1], vec[2]);
	}
	else initgrabz(t->center[0], t->center[1], t->center[2]); 
}

static void headerTranslation(TransInfo *t, float vec[3], char *str) {
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
			sprintf(str, "D: %s%s %s", &tvec[0], t->con.text, t->proptext);
			break;
		case 1:
			sprintf(str, "D: %s   D: %s%s %s", &tvec[0], &tvec[20], t->con.text, t->proptext);
			break;
		case 2:
			sprintf(str, "D: %s   D: %s  D: %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
		}
	}
	else {
		sprintf(str, "Dx: %s   Dy: %s  Dz: %s%s %s", &tvec[0], &tvec[20], &tvec[40], t->con.text, t->proptext);
	}
}

static void applyTranslation(TransInfo *t, float vec[3]) {
	TransData *td = t->data;
	float tvec[3];
	int i;

	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (t->con.applyVec) {
			float pvec[3];
			t->con.applyVec(t, td, vec, tvec, pvec);
		}
		else {
			VECCOPY(tvec, vec);
		}

		Mat3MulVecfl(td->smtx, tvec);
		VecMulf(tvec, td->factor);
		
		/* transdata ipokey */
		if(td->tdi) {
			TransDataIpokey *tdi= td->tdi;
			add_tdi_poin(tdi->locx, tdi->oldloc, tvec[0]);
			add_tdi_poin(tdi->locy, tdi->oldloc+1, tvec[1]);
			add_tdi_poin(tdi->locz, tdi->oldloc+2, tvec[2]);
		}
		else VecAddf(td->loc, td->iloc, tvec);
	}
}

/* uses t->vec to store actual translation in */
int Translation(TransInfo *t, short mval[2]) 
{
	float tvec[3];
	char str[200];
	
	if(t->flag & T_SHIFT_MOD) {
		float dvec[3];
		/* calculate the main translation and the precise one separate */
		window_to_3d(dvec, (short)(mval[0] - t->shiftmval[0]), (short)(mval[1] - t->shiftmval[1]));
		VecMulf(dvec, 0.1f);
		window_to_3d(t->vec, (short)(t->shiftmval[0] - t->imval[0]), (short)(t->shiftmval[1] - t->imval[1]));
		VecAddf(t->vec, t->vec, dvec);
	}
	else window_to_3d(t->vec, (short)(mval[0] - t->imval[0]), (short)(mval[1] - t->imval[1]));

	if (t->con.mode & CON_APPLY) {
		float pvec[3] = {0.0f, 0.0f, 0.0f};
		t->con.applyVec(t, NULL, t->vec, tvec, pvec);
		VECCOPY(t->vec, tvec);
		headerTranslation(t, pvec, str);
	}
	else {
		snapGrid(t, t->vec);
		applyNumInput(&t->num, t->vec);
		headerTranslation(t, t->vec, str);
	}

	applyTranslation(t, t->vec);

	recalcData(t);

	headerprint(str);

	force_draw(0);

	return 1;
}

/* ************************** SHRINK/FATTEN *************************** */

void initShrinkFatten(TransInfo *t) 
{
	if (G.obedit==NULL || G.obedit->type != OB_MESH) {
		initTransModeFlags(t, TFM_RESIZE);
		initResize(t);
		return;
	}

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 1.0f;
	t->snap[2] = t->snap[1] * 0.1f;
	t->transform = ShrinkFatten;
}



int ShrinkFatten(TransInfo *t, short mval[2]) 
{
	float vec[3];
	float ratio;
	int i;
	char str[50];
	TransData *td = t->data;

	window_to_3d(t->vec, (short)(mval[0] - t->imval[0]), (short)(mval[1] - t->imval[1]));
	Projf(vec, t->vec, G.vd->viewinv[1]);
	ratio = Inpf(G.vd->viewinv[1], vec) * -2.0f;

	snapGrid(t, &ratio);

	applyNumInput(&t->num, &ratio);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);

		sprintf(str, "Shrink/Fatten: %s %s", c, t->proptext);
	}
	else {
		/* default header print */
		sprintf(str, "Shrink/Fatten: %.4f %s", ratio, t->proptext);
	}
	
	
	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		VECCOPY(vec, td->axismtx[2]);
		VecMulf(vec, ratio);
		VecMulf(vec, td->factor);

		VecAddf(td->loc, td->iloc, vec);
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	return 1;
}

/* ************************** TILT *************************** */

void initTilt(TransInfo *t) 
{
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = (float)((5.0/180)*M_PI);
	t->snap[2] = t->snap[1] * 0.2f;
	t->fac = 0;
	t->transform = Tilt;
}



int Tilt(TransInfo *t, short mval[2]) 
{
	TransData *td = t->data;
	int i;
	char str[50];

	float final;

	int dx2 = t->center2d[0] - mval[0];
	int dy2 = t->center2d[1] - mval[1];
	float B = (float)sqrt(dx2*dx2+dy2*dy2);

	int dx1 = t->center2d[0] - t->imval[0];
	int dy1 = t->center2d[1] - t->imval[1];
	float A = (float)sqrt(dx1*dx1+dy1*dy1);

	int dx3 = mval[0] - t->imval[0];
	int dy3 = mval[1] - t->imval[1];

	float deler= ((dx1*dx1+dy1*dy1)+(dx2*dx2+dy2*dy2)-(dx3*dx3+dy3*dy3))
		/ (2 * A * B);

	float dphi;

	dphi = saacos(deler);
	if( (dx1*dy2-dx2*dy1)>0.0 ) dphi= -dphi;

	if(G.qual & LR_SHIFTKEY) t->fac += dphi/30.0f;
	else t->fac += dphi;

	final = t->fac;

	snapGrid(t, &final);

	t->imval[0] = mval[0];
	t->imval[1] = mval[1];

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

		if (td->val) {
			*td->val = td->ival + final * td->factor;
		}
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t, t->center);

	return 1;
}

/* ************************** PUSH/PULL *************************** */

void initPushPull(TransInfo *t) 
{
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 1.0f;
	t->snap[2] = t->snap[1] * 0.1f;
	t->transform = PushPull;
}



int PushPull(TransInfo *t, short mval[2]) 
{
	float vec[3], axis[3];
	float distance;
	int i;
	char str[50];
	TransData *td = t->data;

	window_to_3d(t->vec, (short)(mval[0] - t->imval[0]), (short)(mval[1] - t->imval[1]));
	Projf(vec, t->vec, G.vd->viewinv[1]);
	distance = Inpf(G.vd->viewinv[1], vec) * 2.0f;

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
		t->con.applyRot(t, NULL, axis);
	}
	
	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		VecSubf(vec, t->center, td->center);
		if (t->con.applyRot && t->con.mode & CON_APPLY) {
			t->con.applyRot(t, td, axis);
			if (isLockConstraint(t)) {
				float dvec[3];
				Projf(dvec, vec, axis);
				VecSubf(vec, vec, dvec);
			}
			else {
				Projf(vec, vec, axis);
			}
		}
		Normalise(vec);
		VecMulf(vec, distance);
		VecMulf(vec, td->factor);

		VecAddf(td->loc, td->iloc, vec);
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	return 1;
}

/* ************************** CREASE *************************** */

void initCrease(TransInfo *t) 
{
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	t->transform = Crease;
	t->fac = (float)sqrt( (float)
		(
			(t->center2d[1] - t->imval[1])*(t->center2d[1] - t->imval[1])
		+
			(t->center2d[0] - t->imval[0])*(t->center2d[0] - t->imval[0])
		) );

	if(t->fac==0.0f) t->fac= 1.0f;	// prevent Inf
}

int Crease(TransInfo *t, short mval[2]) 
{
	TransData *td = t->data;
	float crease;
	int i;
	char str[50];

		
	if(t->flag & T_SHIFT_MOD) {
		/* calculate ratio for shiftkey pos, and for total, and blend these for precision */
		float dx= (float)(t->center2d[0] - t->shiftmval[0]);
		float dy= (float)(t->center2d[1] - t->shiftmval[1]);
		crease = (float)sqrt( dx*dx + dy*dy)/t->fac;
		
		dx= (float)(t->center2d[0] - mval[0]);
		dy= (float)(t->center2d[1] - mval[1]);
		crease+= 0.1f*(float)(sqrt( dx*dx + dy*dy)/t->fac -crease);
		
	}
	else {
		float dx= (float)(t->center2d[0] - mval[0]);
		float dy= (float)(t->center2d[1] - mval[1]);
		crease = (float)sqrt( dx*dx + dy*dy)/t->fac;
	}

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

		if (td->val) {
			*td->val = td->ival + crease * td->factor;
			if (*td->val < 0.0f) *td->val = 0.0f;
			if (*td->val > 1.0f) *td->val = 1.0f;
		}
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t, t->center);

	return 1;
}

/* ************************** MIRROR *************************** */

void Mirror(short mode) 
{
	TransData *td;
	float mati[3][3], matview[3][3], mat[3][3];
	float size[3];
	int i;

	Mat3One(mati);
	Mat3CpyMat4(matview, G.vd->viewinv);
	Mat3Ortho(matview);

	Trans.context = CTX_NO_PET;

	initTrans(&Trans);					// internal data, mouse, vectors

	initTransModeFlags(&Trans, TFM_MIRROR);	// modal settings in struct Trans

	createTransData(&Trans);			// make TransData structs from selection

	calculatePropRatio(&Trans);
	calculateCenter(&Trans);

	initResize(&Trans);

	if (Trans.total == 0) {
		postTrans(&Trans);
		return;
	}

	size[0] = size[1] = size[2] = 1.0f;
	td = Trans.data;

	switch (mode) {
	case 1:
		size[0] = -1.0f;
		setConstraint(&Trans, mati, (CON_AXIS0), "");
		break;
	case 2:
		size[1] = -1.0f;
		setConstraint(&Trans, mati, (CON_AXIS1), "");
		break;
	case 3:
		size[2] = -1.0f;
		setConstraint(&Trans, mati, (CON_AXIS2), "");
		break;
	case 4:
		size[0] = -1.0f;
		setLocalConstraint(&Trans, (CON_AXIS0), "");
		break;
	case 5:
		size[1] = -1.0f;
		setLocalConstraint(&Trans, (CON_AXIS1), "");
		break;
	case 6:
		size[2] = -1.0f;
		setLocalConstraint(&Trans, (CON_AXIS2), "");
		break;
	case 7:
		size[0] = -1.0f;
		setConstraint(&Trans, matview, (CON_AXIS0), "");
		break;
	case 8:
		size[1] = -1.0f;
		setConstraint(&Trans, matview, (CON_AXIS1), "");
		break;
	case 9:
		size[2] = -1.0f;
		setConstraint(&Trans, matview, (CON_AXIS2), "");
		break;
	default:
		return;
	}

	SizeToMat3(size, mat);

	if (Trans.con.applySize) {
		Trans.con.applySize(&Trans, NULL, mat);
	}

	for(i = 0 ; i < Trans.total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		ElementResize(&Trans, td, mat);
	}

	recalcData(&Trans);
	
	BIF_undo_push("Mirror");

	/* free data, reset vars */
	postTrans(&Trans);

	/* send events out for redraws */
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	scrarea_queue_headredraw(curarea);
}

/* ******************** EditBone scaling *************** */

static void ElementBoneSize(TransInfo *t, TransData *td, float mat[3][3]) 
{
	float tmat[3][3], smat[3][3], oldy;
	float sizemat[3][3];
		
	Mat3MulMat3(smat, mat, td->mtx);
	Mat3MulMat3(tmat, td->smtx, smat);
	
	if (t->con.applySize) {
		t->con.applySize(t, td, tmat);
	}
	
	/* we've tucked the scale in loc */
	oldy= td->iloc[1];
	SizeToMat3(td->iloc, sizemat);
	Mat3MulMat3(tmat, tmat, sizemat);
	Mat3ToSize(tmat, td->loc);
	td->loc[1]= oldy;
}


int BoneSize(TransInfo *t, short mval[2]) 
{
	TransData *td = t->data;
	float size[3], mat[3][3];
	float ratio;
	int i;
	char str[50];
	
	/* for manipulator, center handle, the scaling can't be done relative to center */
	if( (t->flag & T_USES_MANIPULATOR) && t->con.mode==0) {
		ratio = 1.0f - ((t->imval[0] - mval[0]) + (t->imval[1] - mval[1]))/100.0f;
	}
	else {
		
		if(t->flag & T_SHIFT_MOD) {
			/* calculate ratio for shiftkey pos, and for total, and blend these for precision */
			float dx= (float)(t->center2d[0] - t->shiftmval[0]);
			float dy= (float)(t->center2d[1] - t->shiftmval[1]);
			ratio = (float)sqrt( dx*dx + dy*dy)/t->fac;
			
			dx= (float)(t->center2d[0] - mval[0]);
			dy= (float)(t->center2d[1] - mval[1]);
			ratio+= 0.1f*(float)(sqrt( dx*dx + dy*dy)/t->fac -ratio);
			
		}
		else {
			float dx= (float)(t->center2d[0] - mval[0]);
			float dy= (float)(t->center2d[1] - mval[1]);
			ratio = (float)sqrt( dx*dx + dy*dy)/t->fac;
		}
		
		/* flip scale, but not for manipulator center handle */
		if	((t->center2d[0] - mval[0]) * (t->center2d[0] - t->imval[0]) + 
			 (t->center2d[1] - mval[1]) * (t->center2d[1] - t->imval[1]) < 0)
			ratio *= -1.0f;
	}
	
	size[0] = size[1] = size[2] = ratio;
	
	snapGrid(t, size);
	
	if (hasNumInput(&t->num)) {
		applyNumInput(&t->num, size);
		constraintNumInput(t, size);
	}
	
	SizeToMat3(size, mat);
	
	if (t->con.applySize) {
		t->con.applySize(t, NULL, mat);
	}
	
	Mat3CpyMat3(t->mat, mat);	// used in manipulator
	
	headerResize(t, size, str);
	
	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		ElementBoneSize(t, td, mat);
	}
	
	recalcData(t);
	
	headerprint(str);
	
	force_draw(0);
	
	if(!(t->flag & T_USES_MANIPULATOR)) helpline (t, t->center);
	
	return 1;
}

void initBoneSize(TransInfo *t)
{
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	t->transform = BoneSize;
	t->fac = (float)sqrt( (float)(
						   (t->center2d[1] - t->imval[1])*(t->center2d[1] - t->imval[1])
						   +
						   (t->center2d[0] - t->imval[0])*(t->center2d[0] - t->imval[0])
						   ) );
	
	if(t->fac==0.0f) t->fac= 1.0f;	// prevent Inf
}

/* ************************************ */

void BIF_TransformSetUndo(char *str)
{
	Trans.undostr= str;
}


