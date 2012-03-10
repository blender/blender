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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_fly.c
 *  \ingroup spview3d
 */


/* defines VIEW3D_OT_fly modal operator */

//#define NDOF_FLY_DEBUG
//#define NDOF_FLY_DRAW_TOOMUCH // is this needed for ndof? - commented so redraw doesnt thrash - campbell

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "BKE_depsgraph.h" /* for fly mode updating */

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "PIL_time.h" /* smoothview */

#include "view3d_intern.h"	// own include

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
enum {
	FLY_MODAL_CANCEL= 1,
	FLY_MODAL_CONFIRM,
	FLY_MODAL_ACCELERATE,
	FLY_MODAL_DECELERATE,
	FLY_MODAL_PAN_ENABLE,
	FLY_MODAL_PAN_DISABLE,
	FLY_MODAL_DIR_FORWARD,
	FLY_MODAL_DIR_BACKWARD,
	FLY_MODAL_DIR_LEFT,
	FLY_MODAL_DIR_RIGHT,
	FLY_MODAL_DIR_UP,
	FLY_MODAL_DIR_DOWN,
	FLY_MODAL_AXIS_LOCK_X,
	FLY_MODAL_AXIS_LOCK_Z,
	FLY_MODAL_PRECISION_ENABLE,
	FLY_MODAL_PRECISION_DISABLE,
	FLY_MODAL_FREELOOK_ENABLE,
	FLY_MODAL_FREELOOK_DISABLE

};

/* called in transform_ops.c, on each regeneration of keymaps  */
void fly_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
	    {FLY_MODAL_CANCEL,	"CANCEL", 0, "Cancel", ""},
	    {FLY_MODAL_CONFIRM,	"CONFIRM", 0, "Confirm", ""},
	    {FLY_MODAL_ACCELERATE, "ACCELERATE", 0, "Accelerate", ""},
	    {FLY_MODAL_DECELERATE, "DECELERATE", 0, "Decelerate", ""},

	    {FLY_MODAL_PAN_ENABLE,	"PAN_ENABLE", 0, "Pan Enable", ""},
	    {FLY_MODAL_PAN_DISABLE,	"PAN_DISABLE", 0, "Pan Disable", ""},

	    {FLY_MODAL_DIR_FORWARD,	"FORWARD", 0, "Fly Forward", ""},
	    {FLY_MODAL_DIR_BACKWARD,"BACKWARD", 0, "Fly Backward", ""},
	    {FLY_MODAL_DIR_LEFT,	"LEFT", 0, "Fly Left", ""},
	    {FLY_MODAL_DIR_RIGHT,	"RIGHT", 0, "Fly Right", ""},
	    {FLY_MODAL_DIR_UP,		"UP", 0, "Fly Up", ""},
	    {FLY_MODAL_DIR_DOWN,	"DOWN", 0, "Fly Down", ""},

	    {FLY_MODAL_AXIS_LOCK_X,	"AXIS_LOCK_X", 0, "X Axis Correction", "X axis correction (toggle)"},
	    {FLY_MODAL_AXIS_LOCK_Z,	"AXIS_LOCK_Z", 0, "X Axis Correction", "Z axis correction (toggle)"},

	    {FLY_MODAL_PRECISION_ENABLE,	"PRECISION_ENABLE", 0, "Precision Enable", ""},
	    {FLY_MODAL_PRECISION_DISABLE,	"PRECISION_DISABLE", 0, "Precision Disable", ""},

	    {FLY_MODAL_FREELOOK_ENABLE, 	"FREELOOK_ENABLE", 0, "Rotation Enable", ""},
	    {FLY_MODAL_FREELOOK_DISABLE,	"FREELOOK_DISABLE", 0, "Rotation Disable", ""},

	    {0, NULL, 0, NULL, NULL}};

	wmKeyMap *keymap= WM_modalkeymap_get(keyconf, "View3D Fly Modal");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap) return;

	keymap= WM_modalkeymap_add(keyconf, "View3D Fly Modal", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, FLY_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_ANY, KM_ANY, 0, FLY_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_ANY, KM_ANY, 0, FLY_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, SPACEKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, FLY_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, PADPLUSKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_ACCELERATE);
	WM_modalkeymap_add_item(keymap, PADMINUS, KM_PRESS, KM_ANY, 0, FLY_MODAL_DECELERATE);
	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, KM_ANY, 0, FLY_MODAL_ACCELERATE);
	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, KM_ANY, 0, FLY_MODAL_DECELERATE);

	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_PRESS, KM_ANY, 0, FLY_MODAL_PAN_ENABLE);
	/* XXX - Bug in the event system, middle mouse release doesnt work */
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, KM_ANY, 0, FLY_MODAL_PAN_DISABLE);

	/* WASD */
	WM_modalkeymap_add_item(keymap, WKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_FORWARD);
	WM_modalkeymap_add_item(keymap, SKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_BACKWARD);
	WM_modalkeymap_add_item(keymap, AKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_LEFT);
	WM_modalkeymap_add_item(keymap, DKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_RIGHT);
	WM_modalkeymap_add_item(keymap, RKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_UP);
	WM_modalkeymap_add_item(keymap, FKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_DOWN);

	WM_modalkeymap_add_item(keymap, XKEY, KM_PRESS, 0, 0, FLY_MODAL_AXIS_LOCK_X);
	WM_modalkeymap_add_item(keymap, ZKEY, KM_PRESS, 0, 0, FLY_MODAL_AXIS_LOCK_Z);

	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_PRECISION_ENABLE);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_RELEASE, KM_ANY, 0, FLY_MODAL_PRECISION_DISABLE);

	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_FREELOOK_ENABLE);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, FLY_MODAL_FREELOOK_DISABLE);

	/* assign map to operators */
	WM_modalkeymap_assign(keymap, "VIEW3D_OT_fly");
}

typedef struct FlyInfo {
	/* context stuff */
	RegionView3D *rv3d;
	View3D *v3d;
	ARegion *ar;
	Scene *scene;

	wmTimer *timer; /* needed for redraws */

	short state;
	short redraw;
	unsigned char use_precision;
	unsigned char use_freelook; /* if the user presses shift they can look about
	                             * without moving the direction there looking */

	int mval[2]; /* latest 2D mouse values */
	wmNDOFMotionData* ndof; /* latest 3D mouse values */

	/* fly state state */
	float speed; /* the speed the view is moving per redraw */
	short axis; /* Axis index to move along by default Z to move along the view */
	short pan_view; /* when true, pan the view instead of rotating */

	/* relative view axis locking - xlock, zlock
	 * 0) disabled
	 * 1) enabled but not checking because mouse hasn't moved outside the margin since locking was checked an not needed
	 *    when the mouse moves, locking is set to 2 so checks are done.
	 * 2) mouse moved and checking needed, if no view altering is donem its changed back to 1 */
	short xlock, zlock;
	float xlock_momentum, zlock_momentum; /* nicer dynamics */
	float grid; /* world scale 1.0 default */

	/* root most parent */
	Object *root_parent;

	/* backup values */
	float dist_backup; /* backup the views distance since we use a zero dist for fly mode */
	float ofs_backup[3]; /* backup the views offset in case the user cancels flying in non camera mode */
	float rot_backup[4]; /* backup the views quat in case the user cancels flying in non camera mode.
	                      * (quat for view, eul for camera) */
	short persp_backup; /* remember if were ortho or not, only used for restoring the view if it was a ortho view */

	short is_ortho_cam; /* are we flying an ortho camera in perspective view,
	                     * which was originall in ortho view?
	                     * could probably figure it out but better be explicit */

	void *obtfm; /* backup the objects transform */

	/* compare between last state */
	double time_lastwheel; /* used to accelerate when using the mousewheel a lot */
	double time_lastdraw; /* time between draws */

	void *draw_handle_pixel;

	/* use for some lag */
	float dvec_prev[3]; /* old for some lag */

} FlyInfo;

static void drawFlyPixel(const struct bContext *UNUSED(C), struct ARegion *UNUSED(ar), void *arg)
{
	FlyInfo *fly = arg;

	/* draws 4 edge brackets that frame the safe area where the
	 * mouse can move during fly mode without spinning the view */
	float x1, x2, y1, y2;
	
	x1= 0.45f * (float)fly->ar->winx;
	y1= 0.45f * (float)fly->ar->winy;
	x2= 0.55f * (float)fly->ar->winx;
	y2= 0.55f * (float)fly->ar->winy;
	cpack(0);
	
	
	glBegin(GL_LINES);
	/* bottom left */
	glVertex2f(x1,y1); 
	glVertex2f(x1,y1+5);
	
	glVertex2f(x1,y1); 
	glVertex2f(x1+5,y1);
	
	/* top right */
	glVertex2f(x2,y2); 
	glVertex2f(x2,y2-5);
	
	glVertex2f(x2,y2); 
	glVertex2f(x2-5,y2);
	
	/* top left */
	glVertex2f(x1,y2); 
	glVertex2f(x1,y2-5);
	
	glVertex2f(x1,y2); 
	glVertex2f(x1+5,y2);
	
	/* bottom right */
	glVertex2f(x2,y1); 
	glVertex2f(x2,y1+5);
	
	glVertex2f(x2,y1); 
	glVertex2f(x2-5,y1);
	glEnd();
}

/* FlyInfo->state */
#define FLY_RUNNING		0
#define FLY_CANCEL		1
#define FLY_CONFIRM		2

static int initFlyInfo (bContext *C, FlyInfo *fly, wmOperator *op, wmEvent *event)
{
	float upvec[3]; // tmp
	float mat[3][3];

	fly->rv3d= CTX_wm_region_view3d(C);
	fly->v3d = CTX_wm_view3d(C);
	fly->ar = CTX_wm_region(C);
	fly->scene= CTX_data_scene(C);

#ifdef NDOF_FLY_DEBUG
	puts("\n-- fly begin --");
#endif

	if (fly->rv3d->persp==RV3D_CAMOB && fly->v3d->camera->id.lib) {
		BKE_report(op->reports, RPT_ERROR, "Cannot fly a camera from an external library");
		return FALSE;
	}

	if (fly->v3d->ob_centre) {
		BKE_report(op->reports, RPT_ERROR, "Cannot fly when the view is locked to an object");
		return FALSE;
	}

	if (fly->rv3d->persp==RV3D_CAMOB && fly->v3d->camera->constraints.first) {
		BKE_report(op->reports, RPT_ERROR, "Cannot fly an object with constraints");
		return FALSE;
	}

	fly->state= FLY_RUNNING;
	fly->speed= 0.0f;
	fly->axis= 2;
	fly->pan_view= FALSE;
	fly->xlock= FALSE;
	fly->zlock= FALSE;
	fly->xlock_momentum=0.0f;
	fly->zlock_momentum=0.0f;
	fly->grid= 1.0f;
	fly->use_precision= FALSE;
	fly->use_freelook= FALSE;

#ifdef NDOF_FLY_DRAW_TOOMUCH
	fly->redraw= 1;
#endif
	fly->dvec_prev[0]= fly->dvec_prev[1]= fly->dvec_prev[2]= 0.0f;

	fly->timer= WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);

	copy_v2_v2_int(fly->mval, event->mval);
	fly->ndof = NULL;

	fly->time_lastdraw= fly->time_lastwheel= PIL_check_seconds_timer();

	fly->draw_handle_pixel = ED_region_draw_cb_activate(fly->ar->type, drawFlyPixel, fly, REGION_DRAW_POST_PIXEL);

	fly->rv3d->rflag |= RV3D_NAVIGATING; /* so we draw the corner margins */

	/* detect weather to start with Z locking */
	upvec[0]=1.0f; upvec[1]=0.0f; upvec[2]=0.0f;
	copy_m3_m4(mat, fly->rv3d->viewinv);
	mul_m3_v3(mat, upvec);
	if (fabs(upvec[2]) < 0.1)
		fly->zlock = 1;
	upvec[0]=0; upvec[1]=0; upvec[2]=0;

	fly->persp_backup= fly->rv3d->persp;
	fly->dist_backup= fly->rv3d->dist;

	/* check for flying ortho camera - which we cant support well
	 * we _could_ also check for an ortho camera but this is easier */
	if (     (fly->rv3d->persp == RV3D_CAMOB) &&
	        (fly->v3d->camera != NULL) &&
	        (fly->rv3d->is_persp == FALSE))
	{
		((Camera *)fly->v3d->camera->data)->type= CAM_PERSP;
		fly->is_ortho_cam= TRUE;
	}

	if (fly->rv3d->persp==RV3D_CAMOB) {
		Object *ob_back;
		if ((U.uiflag & USER_CAM_LOCK_NO_PARENT)==0 && (fly->root_parent=fly->v3d->camera->parent)) {
			while (fly->root_parent->parent)
				fly->root_parent= fly->root_parent->parent;
			ob_back= fly->root_parent;
		}
		else {
			ob_back= fly->v3d->camera;
		}

		/* store the original camera loc and rot */
		/* TODO. axis angle etc */

		fly->obtfm= object_tfm_backup(ob_back);

		where_is_object(fly->scene, fly->v3d->camera);
		negate_v3_v3(fly->rv3d->ofs, fly->v3d->camera->obmat[3]);

		fly->rv3d->dist=0.0;
	}
	else {
		/* perspective or ortho */
		if (fly->rv3d->persp==RV3D_ORTHO)
			fly->rv3d->persp= RV3D_PERSP; /*if ortho projection, make perspective */

		copy_qt_qt(fly->rot_backup, fly->rv3d->viewquat);
		copy_v3_v3(fly->ofs_backup, fly->rv3d->ofs);

		/* the dist defines a vector that is infront of the offset
		 * to rotate the view about.
		 * this is no good for fly mode because we
		 * want to rotate about the viewers center.
		 * but to correct the dist removal we must
		 * alter offset so the view doesn't jump. */

		fly->rv3d->dist= 0.0f;

		upvec[2]= fly->dist_backup; /*x and y are 0*/
		mul_m3_v3(mat, upvec);
		sub_v3_v3(fly->rv3d->ofs, upvec);
		/*Done with correcting for the dist*/
	}
	
	/* center the mouse, probably the UI mafia are against this but without its quite annoying */
	WM_cursor_warp(CTX_wm_window(C), fly->ar->winrct.xmin + fly->ar->winx/2, fly->ar->winrct.ymin + fly->ar->winy/2);
	
	return 1;
}

static int flyEnd(bContext *C, FlyInfo *fly)
{
	RegionView3D *rv3d= fly->rv3d;
	View3D *v3d = fly->v3d;

	float upvec[3];

	if (fly->state == FLY_RUNNING)
		return OPERATOR_RUNNING_MODAL;

#ifdef NDOF_FLY_DEBUG
	puts("\n-- fly end --");
#endif

	WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), fly->timer);

	ED_region_draw_cb_exit(fly->ar->type, fly->draw_handle_pixel);

	rv3d->dist= fly->dist_backup;

	if (fly->state == FLY_CANCEL) {
	/* Revert to original view? */
		if (fly->persp_backup==RV3D_CAMOB) { /* a camera view */
			Object *ob_back;
			ob_back= (fly->root_parent) ? fly->root_parent : fly->v3d->camera;

			/* store the original camera loc and rot */
			object_tfm_restore(ob_back, fly->obtfm);

			DAG_id_tag_update(&ob_back->id, OB_RECALC_OB);
		}
		else {
			/* Non Camera we need to reset the view back to the original location bacause the user canceled*/
			copy_qt_qt(rv3d->viewquat, fly->rot_backup);
			copy_v3_v3(rv3d->ofs, fly->ofs_backup);
			rv3d->persp= fly->persp_backup;
		}
	}
	else if (fly->persp_backup==RV3D_CAMOB) {	/* camera */
		DAG_id_tag_update(fly->root_parent ? &fly->root_parent->id : &v3d->camera->id, OB_RECALC_OB);
	}
	else { /* not camera */
		/* Apply the fly mode view */
		/*restore the dist*/
		float mat[3][3];
		upvec[0]= upvec[1]= 0;
		upvec[2]= fly->dist_backup; /*x and y are 0*/
		copy_m3_m4(mat, rv3d->viewinv);
		mul_m3_v3(mat, upvec);
		add_v3_v3(rv3d->ofs, upvec);
		/*Done with correcting for the dist */
	}

	if (fly->is_ortho_cam) {
		((Camera *)fly->v3d->camera->data)->type= CAM_ORTHO;
	}

	rv3d->rflag &= ~RV3D_NAVIGATING;
//XXX2.5	BIF_view3d_previewrender_signal(fly->sa, PR_DBASE|PR_DISPRECT); /* not working at the moment not sure why */

	if (fly->obtfm)
		MEM_freeN(fly->obtfm);

	if (fly->ndof)
		MEM_freeN(fly->ndof);

	if (fly->state == FLY_CONFIRM) {
		MEM_freeN(fly);
		return OPERATOR_FINISHED;
	}

	MEM_freeN(fly);
	return OPERATOR_CANCELLED;
}

static void flyEvent(FlyInfo *fly, wmEvent *event)
{
	if (event->type == TIMER && event->customdata == fly->timer) {
		fly->redraw = 1;
	}
	else if (event->type == MOUSEMOVE) {
		copy_v2_v2_int(fly->mval, event->mval);
	}
	else if (event->type == NDOF_MOTION) {
		// do these automagically get delivered? yes.
		// puts("ndof motion detected in fly mode!");
		// static const char* tag_name = "3D mouse position";

		wmNDOFMotionData* incoming_ndof = (wmNDOFMotionData*) event->customdata;
		switch (incoming_ndof->progress) {
			case P_STARTING:
				// start keeping track of 3D mouse position
#ifdef NDOF_FLY_DEBUG
				puts("start keeping track of 3D mouse position");
#endif
				// fall through...
			case P_IN_PROGRESS:
				// update 3D mouse position
#ifdef NDOF_FLY_DEBUG
				putchar('.'); fflush(stdout);
#endif
				if (fly->ndof == NULL) {
					// fly->ndof = MEM_mallocN(sizeof(wmNDOFMotionData), tag_name);
					fly->ndof = MEM_dupallocN(incoming_ndof);
					// fly->ndof = malloc(sizeof(wmNDOFMotionData));
				}
				else {
					memcpy(fly->ndof, incoming_ndof, sizeof(wmNDOFMotionData));
				}
				break;
			case P_FINISHING:
				// stop keeping track of 3D mouse position
#ifdef NDOF_FLY_DEBUG
				puts("stop keeping track of 3D mouse position");
#endif
				if (fly->ndof) {
					MEM_freeN(fly->ndof);
					// free(fly->ndof);
					fly->ndof = NULL;
				}
				/* update the time else the view will jump when 2D mouse/timer resume */
				fly->time_lastdraw= PIL_check_seconds_timer();
				break;
			default:
				; // should always be one of the above 3
			}
		}
	/* handle modal keymap first */
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case FLY_MODAL_CANCEL:
				fly->state = FLY_CANCEL;
				break;
			case FLY_MODAL_CONFIRM:
				fly->state = FLY_CONFIRM;
				break;

			case FLY_MODAL_ACCELERATE:
			{
				double time_currwheel;
				float time_wheel;

				time_currwheel= PIL_check_seconds_timer();
				time_wheel = (float)(time_currwheel - fly->time_lastwheel);
				fly->time_lastwheel = time_currwheel;
				/*printf("Wheel %f\n", time_wheel);*/
				/*Mouse wheel delays range from 0.5==slow to 0.01==fast*/
				time_wheel = 1.0f + (10.0f - (20.0f * MIN2(time_wheel, 0.5f))); /* 0-0.5 -> 0-5.0 */

				if (fly->speed < 0.0f) {
					fly->speed= 0.0f;
				}
				else {
					fly->speed += fly->grid*time_wheel * (fly->use_precision ? 0.1f : 1.0f);
				}
				break;
			}
			case FLY_MODAL_DECELERATE:
			{
				double time_currwheel;
				float time_wheel;

				time_currwheel= PIL_check_seconds_timer();
				time_wheel = (float)(time_currwheel - fly->time_lastwheel);
				fly->time_lastwheel = time_currwheel;
				time_wheel = 1.0f + (10.0f - (20.0f * MIN2(time_wheel, 0.5f))); /* 0-0.5 -> 0-5.0 */

				if (fly->speed > 0.0f) {
					fly->speed=0;
				}
				else {
					fly->speed-= fly->grid*time_wheel * (fly->use_precision ? 0.1f : 1.0f);
				}
				break;
			}
			case FLY_MODAL_PAN_ENABLE:
				fly->pan_view= TRUE;
				break;
			case FLY_MODAL_PAN_DISABLE:
//XXX2.5		warp_pointer(cent_orig[0], cent_orig[1]);
				fly->pan_view= FALSE;
				break;

				/* implement WASD keys */
			case FLY_MODAL_DIR_FORWARD:
				if (fly->axis == 2 && fly->speed < 0.0f) { /* reverse direction stops, tap again to continue */
					fly->axis = -1;
				}
				else {
					if (fly->speed < 0.0f) fly->speed= -fly->speed; /* flip speed rather than stopping, game like motion */
					else if (fly->axis==2) fly->speed += fly->grid; /* increase like mousewheel if were already
				                                                     * moving in that difection*/
					fly->axis= 2;
				}
				break;
			case FLY_MODAL_DIR_BACKWARD:
				if (fly->axis == 2 && fly->speed > 0.0f) { /* reverse direction stops, tap again to continue */
					fly->axis = -1;
				}
				else {
					if (fly->speed > 0.0f) fly->speed= -fly->speed;
					else if (fly->axis==2) fly->speed -= fly->grid;

					fly->axis= 2;
				}
				break;
			case FLY_MODAL_DIR_LEFT:
				if (fly->axis == 0 && fly->speed < 0.0f) { /* reverse direction stops, tap again to continue */
					fly->axis = -1;
				}
				else {
					if (fly->speed < 0.0f) fly->speed= -fly->speed;
					else if (fly->axis==0) fly->speed += fly->grid;

					fly->axis= 0;
				}
				break;
			case FLY_MODAL_DIR_RIGHT:
				if (fly->axis == 0 && fly->speed > 0.0f) { /* reverse direction stops, tap again to continue */
					fly->axis = -1;
				}
				else {
					if (fly->speed > 0.0f) fly->speed= -fly->speed;
					else if (fly->axis==0) fly->speed -= fly->grid;

					fly->axis= 0;
				}
				break;
			case FLY_MODAL_DIR_DOWN:
				if (fly->axis == 1 && fly->speed < 0.0f) { /* reverse direction stops, tap again to continue */
					fly->axis = -1;
				}
				else {
					if (fly->speed < 0.0f) fly->speed= -fly->speed;
					else if (fly->axis==1) fly->speed += fly->grid;
					fly->axis= 1;
				}
				break;
			case FLY_MODAL_DIR_UP:
				if (fly->axis == 1 && fly->speed > 0.0f) { /* reverse direction stops, tap again to continue */
					fly->axis = -1;
				}
				else {
					if (fly->speed > 0.0f) fly->speed= -fly->speed;
					else if (fly->axis==1) fly->speed -= fly->grid;
					fly->axis= 1;
				}
				break;

			case FLY_MODAL_AXIS_LOCK_X:
				if (fly->xlock) fly->xlock=0;
				else {
					fly->xlock = 2;
					fly->xlock_momentum = 0.0;
				}
				break;
			case FLY_MODAL_AXIS_LOCK_Z:
				if (fly->zlock) fly->zlock=0;
				else {
					fly->zlock = 2;
					fly->zlock_momentum = 0.0;
				}
				break;

			case FLY_MODAL_PRECISION_ENABLE:
				fly->use_precision= TRUE;
				break;
			case FLY_MODAL_PRECISION_DISABLE:
				fly->use_precision= FALSE;
				break;

			case FLY_MODAL_FREELOOK_ENABLE:
				fly->use_freelook= TRUE;
				break;
			case FLY_MODAL_FREELOOK_DISABLE:
				fly->use_freelook= FALSE;
				break;
		}
	}
}


static void move_camera(bContext* C, RegionView3D* rv3d, FlyInfo* fly, int orientationChanged, int positionChanged)
{
	/* we are in camera view so apply the view ofs and quat to the view matrix and set the camera to the view */

	View3D* v3d = fly->v3d;
	Scene *scene= fly->scene;
	ID *id_key;

	/* transform the parent or the camera? */
	if (fly->root_parent) {
		Object *ob_update;

		float view_mat[4][4];
		float prev_view_mat[4][4];
		float prev_view_imat[4][4];
		float diff_mat[4][4];
		float parent_mat[4][4];

		ED_view3d_to_m4(prev_view_mat, fly->rv3d->ofs, fly->rv3d->viewquat, fly->rv3d->dist);
		invert_m4_m4(prev_view_imat, prev_view_mat);
		ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);
		mult_m4_m4m4(diff_mat, view_mat, prev_view_imat);
		mult_m4_m4m4(parent_mat, diff_mat, fly->root_parent->obmat);
		object_apply_mat4(fly->root_parent, parent_mat, TRUE, FALSE);

		// where_is_object(scene, fly->root_parent);

		ob_update= v3d->camera->parent;
		while (ob_update) {
			DAG_id_tag_update(&ob_update->id, OB_RECALC_OB);
			ob_update= ob_update->parent;
		}

		id_key= &fly->root_parent->id;
	}
	else {
		float view_mat[4][4];
		ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);
		object_apply_mat4(v3d->camera, view_mat, TRUE, FALSE);
		id_key= &v3d->camera->id;
	}

	/* record the motion */
	if (autokeyframe_cfra_can_key(scene, id_key)) {
		ListBase dsources = {NULL, NULL};
		
		/* add datasource override for the camera object */
		ANIM_relative_keyingset_add_source(&dsources, id_key, NULL, NULL); 
		
		/* insert keyframes 
		 *	1) on the first frame
		 *	2) on each subsequent frame
		 *		TODO: need to check in future that frame changed before doing this 
		 */
		if (orientationChanged) {
			KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_ROTATION_ID);
			ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
		}
		if (positionChanged) {
			KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOCATION_ID);
			ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
		}
		
		/* free temp data */
		BLI_freelistN(&dsources);
	}
}

static int flyApply(bContext *C, FlyInfo *fly)
{
#define FLY_ROTATE_FAC 2.5f /* more is faster */
#define FLY_ZUP_CORRECT_FAC 0.1f /* amount to correct per step */
#define FLY_ZUP_CORRECT_ACCEL 0.05f /* increase upright momentum each step */

	/* fly mode - Shift+F
	 * a fly loop where the user can move move the view as if they are flying
	 */
	RegionView3D *rv3d= fly->rv3d;
	ARegion *ar = fly->ar;

	float mat[3][3], /* 3x3 copy of the view matrix so we can move along the view axis */
	dvec[3]={0,0,0}, /* this is the direction thast added to the view offset per redraw */

	/* Camera Uprighting variables */
	upvec[3]={0,0,0}, /* stores the view's up vector */

	moffset[2], /* mouse offset from the views center */
	tmp_quat[4]; /* used for rotating the view */

	int
//	cent_orig[2], /* view center */
//XXX- can avoid using // 	cent[2], /* view center modified */
	xmargin, ymargin; /* x and y margin are define the safe area where the mouses movement wont rotate the view */

#ifdef NDOF_FLY_DEBUG
	{
		static unsigned int iteration = 1;
		printf("fly timer %d\n", iteration++);
	}
#endif


	xmargin= ar->winx/20.0f;
	ymargin= ar->winy/20.0f;

	// UNUSED
	// cent_orig[0]= ar->winrct.xmin + ar->winx/2;
	// cent_orig[1]= ar->winrct.ymin + ar->winy/2;

	{

		/* mouse offset from the center */
		moffset[0]= fly->mval[0]- ar->winx/2;
		moffset[1]= fly->mval[1]- ar->winy/2;

		/* enforce a view margin */
		if (moffset[0]>xmargin)			moffset[0]-=xmargin;
		else if (moffset[0] < -xmargin)	moffset[0]+=xmargin;
		else							moffset[0]=0;

		if (moffset[1]>ymargin)			moffset[1]-=ymargin;
		else if (moffset[1] < -ymargin)	moffset[1]+=ymargin;
		else							moffset[1]=0;


		/* scale the mouse movement by this value - scales mouse movement to the view size
		 * moffset[0]/(ar->winx-xmargin*2) - window size minus margin (same for y)
		 *
		 * the mouse moves isnt linear */

		if (moffset[0]) {
			moffset[0] /= ar->winx - (xmargin*2);
			moffset[0] *= fabsf(moffset[0]);
		}

		if (moffset[1]) {
			moffset[1] /= ar->winy - (ymargin*2);
			moffset[1] *= fabsf(moffset[1]);
		}

		/* Should we redraw? */
		if ( (fly->speed != 0.0f) ||
		     moffset[0] || moffset[1] ||
		     fly->zlock || fly->xlock ||
		     dvec[0] || dvec[1] || dvec[2])
		{
			float dvec_tmp[3];
			double time_current; /*time how fast it takes for us to redraw, this is so simple scenes dont fly too fast */
			float time_redraw;
			float time_redraw_clamped;
#ifdef NDOF_FLY_DRAW_TOOMUCH
			fly->redraw= 1;
#endif
			time_current= PIL_check_seconds_timer();
			time_redraw= (float)(time_current - fly->time_lastdraw);
			time_redraw_clamped= MIN2(0.05f, time_redraw); /* clamp redraw time to avoid jitter in roll correction */
			fly->time_lastdraw= time_current;
			/*fprintf(stderr, "%f\n", time_redraw);*/ /* 0.002 is a small redraw 0.02 is larger */

			/* Scale the time to use shift to scale the speed down- just like
			 * shift slows many other areas of blender down */
			if (fly->use_precision)
				fly->speed= fly->speed * (1.0f-time_redraw_clamped);

			copy_m3_m4(mat, rv3d->viewinv);

			if (fly->pan_view==TRUE) {
				/* pan only */
				dvec_tmp[0]= -moffset[0];
				dvec_tmp[1]= -moffset[1];
				dvec_tmp[2]= 0;

				if (fly->use_precision) {
					dvec_tmp[0] *= 0.1f;
					dvec_tmp[1] *= 0.1f;
				}

				mul_m3_v3(mat, dvec_tmp);
				mul_v3_fl(dvec_tmp, time_redraw * 200.0f * fly->grid);
			}
			else {
				float roll; /* similar to the angle between the camera's up and the Z-up,
				             * but its very rough so just roll */

				/* rotate about the X axis- look up/down */
				if (moffset[1]) {
					upvec[0]=1;
					upvec[1]=0;
					upvec[2]=0;
					mul_m3_v3(mat, upvec);
					/* Rotate about the relative up vec */
					axis_angle_to_quat( tmp_quat, upvec, (float)moffset[1] * time_redraw * -FLY_ROTATE_FAC);
					mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

					if (fly->xlock) fly->xlock = 2; /*check for rotation*/
					if (fly->zlock) fly->zlock = 2;
					fly->xlock_momentum= 0.0f;
				}

				/* rotate about the Y axis- look left/right */
				if (moffset[0]) {

					/* if we're upside down invert the moffset */
					upvec[0]= 0.0f;
					upvec[1]= 1.0f;
					upvec[2]= 0.0f;
					mul_m3_v3(mat, upvec);

					if (upvec[2] < 0.0f)
						moffset[0]= -moffset[0];

					/* make the lock vectors */
					if (fly->zlock) {
						upvec[0]= 0.0f;
						upvec[1]= 0.0f;
						upvec[2]= 1.0f;
					}
					else {
						upvec[0]= 0.0f;
						upvec[1]= 1.0f;
						upvec[2]= 0.0f;
						mul_m3_v3(mat, upvec);
					}

					/* Rotate about the relative up vec */
					axis_angle_to_quat(tmp_quat, upvec, (float)moffset[0] * time_redraw * FLY_ROTATE_FAC);
					mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

					if (fly->xlock) fly->xlock = 2;/*check for rotation*/
					if (fly->zlock) fly->zlock = 2;
				}

				if (fly->zlock==2) {
					upvec[0]= 1.0f;
					upvec[1]= 0.0f;
					upvec[2]= 0.0f;
					mul_m3_v3(mat, upvec);

					/*make sure we have some z rolling*/
					if (fabsf(upvec[2]) > 0.00001f) {
						roll= upvec[2] * 5.0f;
						upvec[0]= 0.0f; /*rotate the view about this axis*/
						upvec[1]= 0.0f;
						upvec[2]= 1.0f;

						mul_m3_v3(mat, upvec);
						/* Rotate about the relative up vec */
						axis_angle_to_quat(tmp_quat, upvec,
						                   roll * time_redraw_clamped * fly->zlock_momentum * FLY_ZUP_CORRECT_FAC);
						mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

						fly->zlock_momentum += FLY_ZUP_CORRECT_ACCEL;
					}
					else {
						fly->zlock= 1; /* dont check until the view rotates again */
						fly->zlock_momentum= 0.0f;
					}
				}

				if (fly->xlock==2 && moffset[1]==0) { /*only apply xcorrect when mouse isnt applying x rot*/
					upvec[0]=0;
					upvec[1]=0;
					upvec[2]=1;
					mul_m3_v3(mat, upvec);
					/*make sure we have some z rolling*/
					if (fabsf(upvec[2]) > 0.00001f) {
						roll= upvec[2] * -5.0f;

						upvec[0]= 1.0f; /*rotate the view about this axis*/
						upvec[1]= 0.0f;
						upvec[2]= 0.0f;

						mul_m3_v3(mat, upvec);

						/* Rotate about the relative up vec */
						axis_angle_to_quat( tmp_quat, upvec, roll*time_redraw_clamped*fly->xlock_momentum*0.1f);
						mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

						fly->xlock_momentum += 0.05f;
					}
					else {
						fly->xlock=1; /* see above */
						fly->xlock_momentum= 0.0f;
					}
				}

				if (fly->axis == -1) {
					/* pause */
					zero_v3(dvec_tmp);
				}
				if (!fly->use_freelook) {
					/* Normal operation */
					/* define dvec, view direction vector */
					dvec_tmp[0]= dvec_tmp[1]= dvec_tmp[2]= 0.0f;
					/* move along the current axis */
					dvec_tmp[fly->axis]= 1.0f;

					mul_m3_v3(mat, dvec_tmp);
				}
				else {
					normalize_v3_v3(dvec_tmp, fly->dvec_prev);
					if (fly->speed < 0.0f) {
						negate_v3(dvec_tmp);
					}
				}

				mul_v3_fl(dvec_tmp, fly->speed * time_redraw * 0.25f);
			}

			/* impose a directional lag */
			interp_v3_v3v3(dvec, dvec_tmp, fly->dvec_prev, (1.0f/(1.0f+(time_redraw*5.0f))));

			if (rv3d->persp==RV3D_CAMOB) {
				Object *lock_ob= fly->root_parent ? fly->root_parent : fly->v3d->camera;
				if (lock_ob->protectflag & OB_LOCK_LOCX) dvec[0] = 0.0;
				if (lock_ob->protectflag & OB_LOCK_LOCY) dvec[1] = 0.0;
				if (lock_ob->protectflag & OB_LOCK_LOCZ) dvec[2] = 0.0;
			}

			add_v3_v3(rv3d->ofs, dvec);

			if (rv3d->persp==RV3D_CAMOB)
				move_camera(C, rv3d, fly, (fly->xlock || fly->zlock || moffset[0] || moffset[1]), fly->speed);

		}
		else {
			/* we're not redrawing but we need to update the time else the view will jump */
			fly->time_lastdraw= PIL_check_seconds_timer();
		}
		/* end drawing */
		copy_v3_v3(fly->dvec_prev, dvec);
	}

	return OPERATOR_FINISHED;
}

static int flyApply_ndof(bContext *C, FlyInfo *fly)
{
	/* shorthand for oft-used variables */
	wmNDOFMotionData* ndof = fly->ndof;
	const float dt = ndof->dt;
	RegionView3D* rv3d = fly->rv3d;
	const int flag = U.ndof_flag;

#if 0
	int shouldRotate = (flag & NDOF_SHOULD_ROTATE) && (fly->pan_view == FALSE),
	    shouldTranslate = (flag & (NDOF_SHOULD_PAN | NDOF_SHOULD_ZOOM));
#endif

	int shouldRotate = (fly->pan_view == FALSE),
	    shouldTranslate = TRUE;

	float view_inv[4];
	invert_qt_qt(view_inv, rv3d->viewquat);

	rv3d->rot_angle = 0.f; // disable onscreen rotation doo-dad

	if (shouldTranslate) {
		const float forward_sensitivity = 1.f;
		const float vertical_sensitivity = 0.4f;
		const float lateral_sensitivity = 0.6f;

		float speed = 10.f; /* blender units per second */
		/* ^^ this is ok for default cube scene, but should scale with.. something */

		float trans[3] = {lateral_sensitivity  * ndof->tvec[0],
		                  vertical_sensitivity * ndof->tvec[1],
		                  forward_sensitivity  * ndof->tvec[2]};

		if (fly->use_precision)
			speed *= 0.2f;

		mul_v3_fl(trans, speed * dt);

		// transform motion from view to world coordinates
		mul_qt_v3(view_inv, trans);

		if (flag & NDOF_FLY_HELICOPTER) {
			/* replace world z component with device y (yes it makes sense) */
			trans[2] = speed * dt * vertical_sensitivity * ndof->tvec[1];
		}

		if (rv3d->persp==RV3D_CAMOB) {
			// respect camera position locks
			Object *lock_ob= fly->root_parent ? fly->root_parent : fly->v3d->camera;
			if (lock_ob->protectflag & OB_LOCK_LOCX) trans[0] = 0.f;
			if (lock_ob->protectflag & OB_LOCK_LOCY) trans[1] = 0.f;
			if (lock_ob->protectflag & OB_LOCK_LOCZ) trans[2] = 0.f;
		}

		if (!is_zero_v3(trans)) {
			// move center of view opposite of hand motion (this is camera mode, not object mode)
			sub_v3_v3(rv3d->ofs, trans);
			shouldTranslate = TRUE;
		}
		else {
			shouldTranslate = FALSE;
		}
	}

	if (shouldRotate) {
		const float turn_sensitivity = 1.f;

		float rotation[4];
		float axis[3];
		float angle = turn_sensitivity * ndof_to_axis_angle(ndof, axis);

		if (fabsf(angle) > 0.0001f) {
			shouldRotate = TRUE;

			if (fly->use_precision)
				angle *= 0.2f;

			/* transform rotation axis from view to world coordinates */
			mul_qt_v3(view_inv, axis);

			// apply rotation to view
			axis_angle_to_quat(rotation, axis, angle);
			mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);

			if (flag & NDOF_LOCK_HORIZON) {
				/* force an upright viewpoint
				 * TODO: make this less... sudden */
				float view_horizon[3] = {1.f, 0.f, 0.f}; /* view +x */
				float view_direction[3] = {0.f, 0.f, -1.f}; /* view -z (into screen) */

				/* find new inverse since viewquat has changed */
				invert_qt_qt(view_inv, rv3d->viewquat);
				/* could apply reverse rotation to existing view_inv to save a few cycles */

				/* transform view vectors to world coordinates */
				mul_qt_v3(view_inv, view_horizon);
				mul_qt_v3(view_inv, view_direction);

				/* find difference between view & world horizons
				 * true horizon lives in world xy plane, so look only at difference in z */
				angle = -asinf(view_horizon[2]);

#ifdef NDOF_FLY_DEBUG
				printf("lock horizon: adjusting %.1f degrees\n\n", RAD2DEG(angle));
#endif

				/* rotate view so view horizon = world horizon */
				axis_angle_to_quat(rotation, view_direction, angle);
				mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);
			}

			rv3d->view = RV3D_VIEW_USER;
		}
		else {
			shouldRotate = FALSE;
		}
	}

	if (shouldTranslate || shouldRotate) {
		fly->redraw = TRUE;

		if (rv3d->persp==RV3D_CAMOB) {
			move_camera(C, rv3d, fly, shouldRotate, shouldTranslate);
		}
	}

	return OPERATOR_FINISHED;
}


static int fly_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	FlyInfo *fly;

	if (rv3d->viewlock)
		return OPERATOR_CANCELLED;

	fly= MEM_callocN(sizeof(FlyInfo), "FlyOperation");

	op->customdata= fly;

	if (initFlyInfo(C, fly, op, event)==FALSE) {
		MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}

	flyEvent(fly, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int fly_cancel(bContext *C, wmOperator *op)
{
	FlyInfo *fly = op->customdata;

	fly->state = FLY_CANCEL;
	flyEnd(C, fly);
	op->customdata= NULL;

	return OPERATOR_CANCELLED;
}

static int fly_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int exit_code;
	short do_draw= FALSE;
	FlyInfo *fly= op->customdata;
	RegionView3D *rv3d= fly->rv3d;
	Object *fly_object= fly->root_parent ? fly->root_parent : fly->v3d->camera;

	fly->redraw= 0;

	flyEvent(fly, event);

	if (fly->ndof) { /* 3D mouse overrules [2D mouse + timer] */
		if (event->type==NDOF_MOTION) {
			flyApply_ndof(C, fly);
		}
	}
	else if (event->type==TIMER && event->customdata == fly->timer) {
		flyApply(C, fly);
	}

	do_draw |= fly->redraw;

	exit_code = flyEnd(C, fly);

	if (exit_code!=OPERATOR_RUNNING_MODAL)
		do_draw= TRUE;

	if (do_draw) {
		if (rv3d->persp==RV3D_CAMOB) {
			WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, fly_object);
		}

		// puts("redraw!"); // too frequent, commented with NDOF_FLY_DRAW_TOOMUCH for now
		ED_region_tag_redraw(CTX_wm_region(C));
	}

	return exit_code;
}

void VIEW3D_OT_fly(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Fly Navigation";
	ot->description= "Interactively fly around the scene";
	ot->idname= "VIEW3D_OT_fly";

	/* api callbacks */
	ot->invoke= fly_invoke;
	ot->cancel= fly_cancel;
	ot->modal= fly_modal;
	ot->poll= ED_operator_view3d_active;

	/* flags */
	ot->flag= OPTYPE_BLOCKING;
}
