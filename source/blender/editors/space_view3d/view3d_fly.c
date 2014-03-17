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
//#define NDOF_FLY_DRAW_TOOMUCH  /* is this needed for ndof? - commented so redraw doesnt thrash - campbell */

#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_report.h"

#include "BLF_translation.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "PIL_time.h" /* smoothview */

#include "UI_resources.h"

#include "view3d_intern.h"  /* own include */

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
enum {
	FLY_MODAL_CANCEL = 1,
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
	FLY_MODAL_FREELOOK_DISABLE,
	FLY_MODAL_SPEED,	/* mousepan typically */
};

/* relative view axis locking - xlock, zlock */
typedef enum eFlyPanState {
	/* disabled */
	FLY_AXISLOCK_STATE_OFF    = 0,

	/* enabled but not checking because mouse hasn't moved outside the margin since locking was checked an not needed
	 * when the mouse moves, locking is set to 2 so checks are done. */
	FLY_AXISLOCK_STATE_IDLE   = 1,

	/* mouse moved and checking needed, if no view altering is done its changed back to #FLY_AXISLOCK_STATE_IDLE */
	FLY_AXISLOCK_STATE_ACTIVE = 2
} eFlyPanState;

/* called in transform_ops.c, on each regeneration of keymaps  */
void fly_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{FLY_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{FLY_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
		{FLY_MODAL_ACCELERATE, "ACCELERATE", 0, "Accelerate", ""},
		{FLY_MODAL_DECELERATE, "DECELERATE", 0, "Decelerate", ""},

		{FLY_MODAL_PAN_ENABLE, "PAN_ENABLE", 0, "Pan Enable", ""},
		{FLY_MODAL_PAN_DISABLE, "PAN_DISABLE", 0, "Pan Disable", ""},

		{FLY_MODAL_DIR_FORWARD, "FORWARD", 0, "Fly Forward", ""},
		{FLY_MODAL_DIR_BACKWARD, "BACKWARD", 0, "Fly Backward", ""},
		{FLY_MODAL_DIR_LEFT, "LEFT", 0, "Fly Left", ""},
		{FLY_MODAL_DIR_RIGHT, "RIGHT", 0, "Fly Right", ""},
		{FLY_MODAL_DIR_UP, "UP", 0, "Fly Up", ""},
		{FLY_MODAL_DIR_DOWN, "DOWN", 0, "Fly Down", ""},

		{FLY_MODAL_AXIS_LOCK_X, "AXIS_LOCK_X", 0, "X Axis Correction", "X axis correction (toggle)"},
		{FLY_MODAL_AXIS_LOCK_Z, "AXIS_LOCK_Z", 0, "X Axis Correction", "Z axis correction (toggle)"},

		{FLY_MODAL_PRECISION_ENABLE, "PRECISION_ENABLE", 0, "Precision Enable", ""},
		{FLY_MODAL_PRECISION_DISABLE, "PRECISION_DISABLE", 0, "Precision Disable", ""},

		{FLY_MODAL_FREELOOK_ENABLE, "FREELOOK_ENABLE", 0, "Rotation Enable", ""},
		{FLY_MODAL_FREELOOK_DISABLE, "FREELOOK_DISABLE", 0, "Rotation Disable", ""},

		{0, NULL, 0, NULL, NULL}};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Fly Modal");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items)
		return;

	keymap = WM_modalkeymap_add(keyconf, "View3D Fly Modal", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_ANY, KM_ANY, 0, FLY_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_ANY, KM_ANY, 0, FLY_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, SPACEKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, FLY_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, PADPLUSKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_ACCELERATE);
	WM_modalkeymap_add_item(keymap, PADMINUS, KM_PRESS, KM_ANY, 0, FLY_MODAL_DECELERATE);
	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, KM_ANY, 0, FLY_MODAL_ACCELERATE);
	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, KM_ANY, 0, FLY_MODAL_DECELERATE);

	WM_modalkeymap_add_item(keymap, MOUSEPAN, 0, 0, 0, FLY_MODAL_SPEED);
	
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_PRESS, KM_ANY, 0, FLY_MODAL_PAN_ENABLE);
	/* XXX - Bug in the event system, middle mouse release doesnt work */
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, KM_ANY, 0, FLY_MODAL_PAN_DISABLE);

	/* WASD */
	WM_modalkeymap_add_item(keymap, WKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_FORWARD);
	WM_modalkeymap_add_item(keymap, SKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_BACKWARD);
	WM_modalkeymap_add_item(keymap, AKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_LEFT);
	WM_modalkeymap_add_item(keymap, DKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_RIGHT);
	WM_modalkeymap_add_item(keymap, EKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_UP);
	WM_modalkeymap_add_item(keymap, QKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_DOWN);

	/* for legacy reasons, leave R/F working */
	WM_modalkeymap_add_item(keymap, RKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_UP);
	WM_modalkeymap_add_item(keymap, FKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_DOWN);

	WM_modalkeymap_add_item(keymap, UPARROWKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_FORWARD);
	WM_modalkeymap_add_item(keymap, DOWNARROWKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_BACKWARD);
	WM_modalkeymap_add_item(keymap, LEFTARROWKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_LEFT);
	WM_modalkeymap_add_item(keymap, RIGHTARROWKEY, KM_PRESS, 0, 0, FLY_MODAL_DIR_RIGHT);

	WM_modalkeymap_add_item(keymap, XKEY, KM_PRESS, 0, 0, FLY_MODAL_AXIS_LOCK_X);
	WM_modalkeymap_add_item(keymap, ZKEY, KM_PRESS, 0, 0, FLY_MODAL_AXIS_LOCK_Z);

	WM_modalkeymap_add_item(keymap, LEFTALTKEY, KM_PRESS, KM_ANY, 0, FLY_MODAL_PRECISION_ENABLE);
	WM_modalkeymap_add_item(keymap, LEFTALTKEY, KM_RELEASE, KM_ANY, 0, FLY_MODAL_PRECISION_DISABLE);

	/* for legacy reasons, leave shift working */
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
	bool redraw;
	bool use_precision;
	/* if the user presses shift they can look about
	 * without moving the direction there looking */
	bool use_freelook;

	int mval[2]; /* latest 2D mouse values */
	int center_mval[2]; /* center mouse values */
	float width, height; /* camera viewport dimensions */
	wmNDOFMotionData *ndof;  /* latest 3D mouse values */

	/* fly state state */
	float speed; /* the speed the view is moving per redraw */
	short axis; /* Axis index to move along by default Z to move along the view */
	bool pan_view; /* when true, pan the view instead of rotating */

	eFlyPanState xlock, zlock;
	float xlock_momentum, zlock_momentum; /* nicer dynamics */
	float grid; /* world scale 1.0 default */

	/* compare between last state */
	double time_lastwheel; /* used to accelerate when using the mousewheel a lot */
	double time_lastdraw; /* time between draws */

	void *draw_handle_pixel;

	/* use for some lag */
	float dvec_prev[3]; /* old for some lag */

	struct View3DCameraControl *v3d_camera_control;

} FlyInfo;

static void drawFlyPixel(const struct bContext *UNUSED(C), ARegion *UNUSED(ar), void *arg)
{
	FlyInfo *fly = arg;
	rctf viewborder;
	int xoff, yoff;
	float x1, x2, y1, y2;

	if (fly->scene->camera) {
		ED_view3d_calc_camera_border(fly->scene, fly->ar, fly->v3d, fly->rv3d, &viewborder, false);
		xoff = viewborder.xmin;
		yoff = viewborder.ymin;
	}
	else {
		xoff = 0;
		yoff = 0;
	}

	/* draws 4 edge brackets that frame the safe area where the
	 * mouse can move during fly mode without spinning the view */

	x1 = xoff + 0.45f * fly->width;
	y1 = yoff + 0.45f * fly->height;
	x2 = xoff + 0.55f * fly->width;
	y2 = yoff + 0.55f * fly->height;

	UI_ThemeColor(TH_VIEW_OVERLAY);
	glBegin(GL_LINES);
	/* bottom left */
	glVertex2f(x1, y1);
	glVertex2f(x1, y1 + 5);

	glVertex2f(x1, y1);
	glVertex2f(x1 + 5, y1);

	/* top right */
	glVertex2f(x2, y2);
	glVertex2f(x2, y2 - 5);

	glVertex2f(x2, y2);
	glVertex2f(x2 - 5, y2);

	/* top left */
	glVertex2f(x1, y2);
	glVertex2f(x1, y2 - 5);

	glVertex2f(x1, y2);
	glVertex2f(x1 + 5, y2);

	/* bottom right */
	glVertex2f(x2, y1);
	glVertex2f(x2, y1 + 5);

	glVertex2f(x2, y1);
	glVertex2f(x2 - 5, y1);
	glEnd();
}

static void fly_update_header(bContext *C, FlyInfo *fly)
{
#define HEADER_LENGTH 256
	char header[HEADER_LENGTH];

	BLI_snprintf(header, HEADER_LENGTH, IFACE_("LMB/Return: confirm, "
	                                           "RMB/Esc: cancel, "
	                                           "MMB: pan, "
	                                           "WASDQE: direction, "
	                                           "Alt: slow, "
	                                           "Ctrl: free look, "
	                                           "X: Upright x axis (%s), "
	                                           "Z: Upright z axis (%s), "
                                               "(+/- | Wheel): speed"),

	    WM_bool_as_string(fly->xlock != FLY_AXISLOCK_STATE_OFF),
		WM_bool_as_string(fly->zlock != FLY_AXISLOCK_STATE_OFF));

	ED_area_headerprint(CTX_wm_area(C), header);
#undef HEADER_LENGTH
}

/* FlyInfo->state */
enum {
	FLY_RUNNING     = 0,
	FLY_CANCEL      = 1,
	FLY_CONFIRM     = 2,
};

static bool initFlyInfo(bContext *C, FlyInfo *fly, wmOperator *op, const wmEvent *event)
{
	wmWindow *win = CTX_wm_window(C);
	rctf viewborder;

	float upvec[3]; /* tmp */
	float mat[3][3];

	fly->rv3d = CTX_wm_region_view3d(C);
	fly->v3d = CTX_wm_view3d(C);
	fly->ar = CTX_wm_region(C);
	fly->scene = CTX_data_scene(C);

#ifdef NDOF_FLY_DEBUG
	puts("\n-- fly begin --");
#endif

	/* sanity check: for rare but possible case (if lib-linking the camera fails) */
	if ((fly->rv3d->persp == RV3D_CAMOB) && (fly->v3d->camera == NULL)) {
		fly->rv3d->persp = RV3D_PERSP;
	}

	if (fly->rv3d->persp == RV3D_CAMOB && fly->v3d->camera->id.lib) {
		BKE_report(op->reports, RPT_ERROR, "Cannot fly a camera from an external library");
		return false;
	}

	if (ED_view3d_offset_lock_check(fly->v3d, fly->rv3d)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot fly when the view offset is locked");
		return false;
	}

	if (fly->rv3d->persp == RV3D_CAMOB && fly->v3d->camera->constraints.first) {
		BKE_report(op->reports, RPT_ERROR, "Cannot fly an object with constraints");
		return false;
	}

	fly->state = FLY_RUNNING;
	fly->speed = 0.0f;
	fly->axis = 2;
	fly->pan_view = false;
	fly->xlock = FLY_AXISLOCK_STATE_OFF;
	fly->zlock = FLY_AXISLOCK_STATE_OFF;
	fly->xlock_momentum = 0.0f;
	fly->zlock_momentum = 0.0f;
	fly->grid = 1.0f;
	fly->use_precision = false;
	fly->use_freelook = false;

#ifdef NDOF_FLY_DRAW_TOOMUCH
	fly->redraw = 1;
#endif
	zero_v3(fly->dvec_prev);

	fly->timer = WM_event_add_timer(CTX_wm_manager(C), win, TIMER, 0.01f);

	copy_v2_v2_int(fly->mval, event->mval);
	fly->ndof = NULL;

	fly->time_lastdraw = fly->time_lastwheel = PIL_check_seconds_timer();

	fly->draw_handle_pixel = ED_region_draw_cb_activate(fly->ar->type, drawFlyPixel, fly, REGION_DRAW_POST_PIXEL);

	fly->rv3d->rflag |= RV3D_NAVIGATING;

	/* detect whether to start with Z locking */
	copy_v3_fl3(upvec, 1.0f, 0.0f, 0.0f);
	copy_m3_m4(mat, fly->rv3d->viewinv);
	mul_m3_v3(mat, upvec);
	if (fabsf(upvec[2]) < 0.1f) {
		fly->zlock = FLY_AXISLOCK_STATE_IDLE;
	}

	fly->v3d_camera_control = ED_view3d_cameracontrol_aquire(
	        fly->scene, fly->v3d, fly->rv3d,
	        (U.uiflag & USER_CAM_LOCK_NO_PARENT) == 0);

	/* calculate center */
	if (fly->scene->camera) {
		ED_view3d_calc_camera_border(fly->scene, fly->ar, fly->v3d, fly->rv3d, &viewborder, false);

		fly->width = BLI_rctf_size_x(&viewborder);
		fly->height = BLI_rctf_size_y(&viewborder);

		fly->center_mval[0] = viewborder.xmin + fly->width / 2;
		fly->center_mval[1] = viewborder.ymin + fly->height / 2;
	}
	else {
		fly->width = fly->ar->winx;
		fly->height = fly->ar->winy;

		fly->center_mval[0] = fly->width / 2;
		fly->center_mval[1] = fly->height / 2;
	}

	/* center the mouse, probably the UI mafia are against this but without its quite annoying */
	WM_cursor_warp(win, fly->ar->winrct.xmin + fly->center_mval[0], fly->ar->winrct.ymin + fly->center_mval[1]);

	fly_update_header(C, fly);
	return 1;
}

static int flyEnd(bContext *C, FlyInfo *fly)
{
	wmWindow *win;
	RegionView3D *rv3d;

	if (fly->state == FLY_RUNNING)
		return OPERATOR_RUNNING_MODAL;

#ifdef NDOF_FLY_DEBUG
	puts("\n-- fly end --");
#endif

	win = CTX_wm_window(C);
	rv3d = fly->rv3d;

	WM_event_remove_timer(CTX_wm_manager(C), win, fly->timer);

	ED_region_draw_cb_exit(fly->ar->type, fly->draw_handle_pixel);

	ED_view3d_cameracontrol_release(fly->v3d_camera_control, fly->state == FLY_CANCEL);

	rv3d->rflag &= ~RV3D_NAVIGATING;

	if (fly->ndof)
		MEM_freeN(fly->ndof);

	if (fly->state == FLY_CONFIRM) {
		MEM_freeN(fly);
		return OPERATOR_FINISHED;
	}

	MEM_freeN(fly);
	return OPERATOR_CANCELLED;
}

static void flyEvent(bContext *C, FlyInfo *fly, const wmEvent *event)
{
	if (event->type == TIMER && event->customdata == fly->timer) {
		fly->redraw = 1;
	}
	else if (event->type == MOUSEMOVE) {
		copy_v2_v2_int(fly->mval, event->mval);
	}
	else if (event->type == NDOF_MOTION) {
		/* do these automagically get delivered? yes. */
		// puts("ndof motion detected in fly mode!");
		// static const char *tag_name = "3D mouse position";

		const wmNDOFMotionData *incoming_ndof = event->customdata;
		switch (incoming_ndof->progress) {
			case P_STARTING:
				/* start keeping track of 3D mouse position */
#ifdef NDOF_FLY_DEBUG
				puts("start keeping track of 3D mouse position");
#endif
				/* fall-through */
			case P_IN_PROGRESS:
				/* update 3D mouse position */
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
				/* stop keeping track of 3D mouse position */
#ifdef NDOF_FLY_DEBUG
				puts("stop keeping track of 3D mouse position");
#endif
				if (fly->ndof) {
					MEM_freeN(fly->ndof);
					// free(fly->ndof);
					fly->ndof = NULL;
				}
				/* update the time else the view will jump when 2D mouse/timer resume */
				fly->time_lastdraw = PIL_check_seconds_timer();
				break;
			default:
				break; /* should always be one of the above 3 */
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
				
			/* speed adjusting with mousepan (trackpad) */
			case FLY_MODAL_SPEED:
			{
				float fac = 0.02f * (event->prevy - event->y);
				
				/* allowing to brake immediate */
				if (fac > 0.0f && fly->speed < 0.0f)
					fly->speed = 0.0f;
				else if (fac < 0.0f && fly->speed > 0.0f)
					fly->speed = 0.0f;
				else
					fly->speed += fly->grid * fac;
				
				break;
			}
			case FLY_MODAL_ACCELERATE:
			{
				double time_currwheel;
				float time_wheel;

				/* not quite correct but avoids confusion WASD/arrow keys 'locking up' */
				if (fly->axis == -1) {
					fly->axis = 2;
					fly->speed = fabsf(fly->speed);
				}

				time_currwheel = PIL_check_seconds_timer();
				time_wheel = (float)(time_currwheel - fly->time_lastwheel);
				fly->time_lastwheel = time_currwheel;
				/* Mouse wheel delays range from (0.5 == slow) to (0.01 == fast) */
				time_wheel = 1.0f + (10.0f - (20.0f * min_ff(time_wheel, 0.5f))); /* 0-0.5 -> 0-5.0 */

				if (fly->speed < 0.0f) {
					fly->speed = 0.0f;
				}
				else {
					fly->speed += fly->grid * time_wheel * (fly->use_precision ? 0.1f : 1.0f);
				}
				break;
			}
			case FLY_MODAL_DECELERATE:
			{
				double time_currwheel;
				float time_wheel;

				/* not quite correct but avoids confusion WASD/arrow keys 'locking up' */
				if (fly->axis == -1) {
					fly->axis = 2;
					fly->speed = -fabsf(fly->speed);
				}

				time_currwheel = PIL_check_seconds_timer();
				time_wheel = (float)(time_currwheel - fly->time_lastwheel);
				fly->time_lastwheel = time_currwheel;
				time_wheel = 1.0f + (10.0f - (20.0f * min_ff(time_wheel, 0.5f))); /* 0-0.5 -> 0-5.0 */

				if (fly->speed > 0.0f) {
					fly->speed = 0;
				}
				else {
					fly->speed -= fly->grid * time_wheel * (fly->use_precision ? 0.1f : 1.0f);
				}
				break;
			}
			case FLY_MODAL_PAN_ENABLE:
				fly->pan_view = true;
				break;
			case FLY_MODAL_PAN_DISABLE:
				fly->pan_view = false;
				break;

			/* implement WASD keys,
			 * comments only for 'forward '*/
			case FLY_MODAL_DIR_FORWARD:
				if (fly->axis == 2 && fly->speed < 0.0f) { /* reverse direction stops, tap again to continue */
					fly->axis = -1;
				}
				else {
					/* flip speed rather than stopping, game like motion,
					 * else increase like mousewheel if were already moving in that direction */
					if (fly->speed < 0.0f)   fly->speed = -fly->speed;
					else if (fly->axis == 2) fly->speed += fly->grid;
					fly->axis = 2;
				}
				break;
			case FLY_MODAL_DIR_BACKWARD:
				if (fly->axis == 2 && fly->speed > 0.0f) {
					fly->axis = -1;
				}
				else {
					if (fly->speed > 0.0f)   fly->speed = -fly->speed;
					else if (fly->axis == 2) fly->speed -= fly->grid;

					fly->axis = 2;
				}
				break;
			case FLY_MODAL_DIR_LEFT:
				if (fly->axis == 0 && fly->speed < 0.0f) {
					fly->axis = -1;
				}
				else {
					if (fly->speed < 0.0f)   fly->speed = -fly->speed;
					else if (fly->axis == 0) fly->speed += fly->grid;

					fly->axis = 0;
				}
				break;
			case FLY_MODAL_DIR_RIGHT:
				if (fly->axis == 0 && fly->speed > 0.0f) {
					fly->axis = -1;
				}
				else {
					if (fly->speed > 0.0f)   fly->speed = -fly->speed;
					else if (fly->axis == 0) fly->speed -= fly->grid;

					fly->axis = 0;
				}
				break;
			case FLY_MODAL_DIR_DOWN:
				if (fly->axis == 1 && fly->speed < 0.0f) {
					fly->axis = -1;
				}
				else {
					if (fly->speed < 0.0f)   fly->speed = -fly->speed;
					else if (fly->axis == 1) fly->speed += fly->grid;
					fly->axis = 1;
				}
				break;
			case FLY_MODAL_DIR_UP:
				if (fly->axis == 1 && fly->speed > 0.0f) {
					fly->axis = -1;
				}
				else {
					if (fly->speed > 0.0f)   fly->speed = -fly->speed;
					else if (fly->axis == 1) fly->speed -= fly->grid;
					fly->axis = 1;
				}
				break;

			case FLY_MODAL_AXIS_LOCK_X:
				if (fly->xlock != FLY_AXISLOCK_STATE_OFF)
					fly->xlock = FLY_AXISLOCK_STATE_OFF;
				else {
					fly->xlock = FLY_AXISLOCK_STATE_ACTIVE;
					fly->xlock_momentum = 0.0;
				}
				fly_update_header(C, fly);
				break;
			case FLY_MODAL_AXIS_LOCK_Z:
				if (fly->zlock != FLY_AXISLOCK_STATE_OFF)
					fly->zlock = FLY_AXISLOCK_STATE_OFF;
				else {
					fly->zlock = FLY_AXISLOCK_STATE_ACTIVE;
					fly->zlock_momentum = 0.0;
				}
				fly_update_header(C, fly);
				break;

			case FLY_MODAL_PRECISION_ENABLE:
				fly->use_precision = true;
				break;
			case FLY_MODAL_PRECISION_DISABLE:
				fly->use_precision = false;
				break;

			case FLY_MODAL_FREELOOK_ENABLE:
				fly->use_freelook = true;
				break;
			case FLY_MODAL_FREELOOK_DISABLE:
				fly->use_freelook = false;
				break;
		}
	}
}

static void flyMoveCamera(bContext *C, FlyInfo *fly,
                          const bool do_rotate, const bool do_translate)
{
	ED_view3d_cameracontrol_update(fly->v3d_camera_control, true, C, do_rotate, do_translate);
}

static int flyApply(bContext *C, FlyInfo *fly)
{
#define FLY_ROTATE_FAC 10.0f /* more is faster */
#define FLY_ZUP_CORRECT_FAC 0.1f /* amount to correct per step */
#define FLY_ZUP_CORRECT_ACCEL 0.05f /* increase upright momentum each step */
#define FLY_SMOOTH_FAC 20.0f  /* higher value less lag */

	/* fly mode - Shift+F
	 * a fly loop where the user can move move the view as if they are flying
	 */
	RegionView3D *rv3d = fly->rv3d;

	float mat[3][3]; /* 3x3 copy of the view matrix so we can move along the view axis */
	float dvec[3] = {0, 0, 0}; /* this is the direction thast added to the view offset per redraw */

	/* Camera Uprighting variables */
	float upvec[3] = {0, 0, 0}; /* stores the view's up vector */

	float moffset[2]; /* mouse offset from the views center */
	float tmp_quat[4]; /* used for rotating the view */

	int xmargin, ymargin; /* x and y margin are define the safe area where the mouses movement wont rotate the view */

#ifdef NDOF_FLY_DEBUG
	{
		static unsigned int iteration = 1;
		printf("fly timer %d\n", iteration++);
	}
#endif

	xmargin = fly->width / 20.0f;
	ymargin = fly->height / 20.0f;

	{

		/* mouse offset from the center */
		moffset[0] = fly->mval[0] - fly->center_mval[0];
		moffset[1] = fly->mval[1] - fly->center_mval[1];

		/* enforce a view margin */
		if      (moffset[0] >  xmargin) moffset[0] -= xmargin;
		else if (moffset[0] < -xmargin) moffset[0] += xmargin;
		else                            moffset[0] =  0;

		if      (moffset[1] >  ymargin) moffset[1] -= ymargin;
		else if (moffset[1] < -ymargin) moffset[1] += ymargin;
		else                            moffset[1] =  0;


		/* scale the mouse movement by this value - scales mouse movement to the view size
		 * moffset[0] / (ar->winx-xmargin * 2) - window size minus margin (same for y)
		 *
		 * the mouse moves isn't linear */

		if (moffset[0]) {
			moffset[0] /= fly->width - (xmargin * 2);
			moffset[0] *= fabsf(moffset[0]);
		}

		if (moffset[1]) {
			moffset[1] /= fly->height - (ymargin * 2);
			moffset[1] *= fabsf(moffset[1]);
		}

		/* Should we redraw? */
		if ((fly->speed != 0.0f) ||
		    moffset[0] || moffset[1] ||
		    (fly->zlock != FLY_AXISLOCK_STATE_OFF) ||
		    (fly->xlock != FLY_AXISLOCK_STATE_OFF) ||
		    dvec[0] || dvec[1] || dvec[2])
		{
			float dvec_tmp[3];

			/* time how fast it takes for us to redraw,
			 * this is so simple scenes don't fly too fast */
			double time_current;
			float time_redraw;
			float time_redraw_clamped;
#ifdef NDOF_FLY_DRAW_TOOMUCH
			fly->redraw = 1;
#endif
			time_current = PIL_check_seconds_timer();
			time_redraw = (float)(time_current - fly->time_lastdraw);
			time_redraw_clamped = min_ff(0.05f, time_redraw); /* clamp redraw time to avoid jitter in roll correction */
			fly->time_lastdraw = time_current;

			/* Scale the time to use shift to scale the speed down- just like
			 * shift slows many other areas of blender down */
			if (fly->use_precision)
				fly->speed = fly->speed * (1.0f - time_redraw_clamped);

			copy_m3_m4(mat, rv3d->viewinv);

			if (fly->pan_view == true) {
				/* pan only */
				copy_v3_fl3(dvec_tmp, -moffset[0], -moffset[1], 0.0f);

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
					copy_v3_fl3(upvec, 1.0f, 0.0f, 0.0f);
					mul_m3_v3(mat, upvec);
					/* Rotate about the relative up vec */
					axis_angle_to_quat(tmp_quat, upvec, (float)moffset[1] * time_redraw * -FLY_ROTATE_FAC);
					mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

					if (fly->xlock != FLY_AXISLOCK_STATE_OFF)
						fly->xlock = FLY_AXISLOCK_STATE_ACTIVE;  /* check for rotation */
					if (fly->zlock != FLY_AXISLOCK_STATE_OFF)
						fly->zlock = FLY_AXISLOCK_STATE_ACTIVE;
					fly->xlock_momentum = 0.0f;
				}

				/* rotate about the Y axis- look left/right */
				if (moffset[0]) {

					/* if we're upside down invert the moffset */
					copy_v3_fl3(upvec, 0.0f, 1.0f, 0.0f);
					mul_m3_v3(mat, upvec);

					if (upvec[2] < 0.0f)
						moffset[0] = -moffset[0];

					/* make the lock vectors */
					if (fly->zlock) {
						copy_v3_fl3(upvec, 0.0f, 0.0f, 1.0f);
					}
					else {
						copy_v3_fl3(upvec, 0.0f, 1.0f, 0.0f);
						mul_m3_v3(mat, upvec);
					}

					/* Rotate about the relative up vec */
					axis_angle_to_quat(tmp_quat, upvec, (float)moffset[0] * time_redraw * FLY_ROTATE_FAC);
					mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

					if (fly->xlock != FLY_AXISLOCK_STATE_OFF)
						fly->xlock = FLY_AXISLOCK_STATE_ACTIVE;  /* check for rotation */
					if (fly->zlock != FLY_AXISLOCK_STATE_OFF)
						fly->zlock = FLY_AXISLOCK_STATE_ACTIVE;
				}

				if (fly->zlock == FLY_AXISLOCK_STATE_ACTIVE) {
					copy_v3_fl3(upvec, 1.0f, 0.0f, 0.0f);
					mul_m3_v3(mat, upvec);

					/* make sure we have some z rolling */
					if (fabsf(upvec[2]) > 0.00001f) {
						roll = upvec[2] * 5.0f;
						/* rotate the view about this axis */
						copy_v3_fl3(upvec, 0.0f, 0.0f, 1.0f);
						mul_m3_v3(mat, upvec);
						/* Rotate about the relative up vec */
						axis_angle_to_quat(tmp_quat, upvec,
						                   roll * time_redraw_clamped * fly->zlock_momentum * FLY_ZUP_CORRECT_FAC);
						mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

						fly->zlock_momentum += FLY_ZUP_CORRECT_ACCEL;
					}
					else {
						fly->zlock = FLY_AXISLOCK_STATE_IDLE; /* don't check until the view rotates again */
						fly->zlock_momentum = 0.0f;
					}
				}

				/* only apply xcorrect when mouse isn't applying x rot */
				if (fly->xlock == FLY_AXISLOCK_STATE_ACTIVE && moffset[1] == 0) {
					copy_v3_fl3(upvec, 0.0f, 0.0f, 1.0f);
					mul_m3_v3(mat, upvec);
					/* make sure we have some z rolling */
					if (fabsf(upvec[2]) > 0.00001f) {
						roll = upvec[2] * -5.0f;
						/* rotate the view about this axis */
						copy_v3_fl3(upvec, 1.0f, 0.0f, 0.0f);
						mul_m3_v3(mat, upvec);

						/* Rotate about the relative up vec */
						axis_angle_to_quat(tmp_quat, upvec, roll * time_redraw_clamped * fly->xlock_momentum * 0.1f);
						mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, tmp_quat);

						fly->xlock_momentum += 0.05f;
					}
					else {
						fly->xlock = FLY_AXISLOCK_STATE_IDLE; /* see above */
						fly->xlock_momentum = 0.0f;
					}
				}

				if (fly->axis == -1) {
					/* pause */
					zero_v3(dvec_tmp);
				}
				else if (!fly->use_freelook) {
					/* Normal operation */
					/* define dvec, view direction vector */
					zero_v3(dvec_tmp);
					/* move along the current axis */
					dvec_tmp[fly->axis] = 1.0f;

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
			interp_v3_v3v3(dvec, dvec_tmp, fly->dvec_prev, (1.0f / (1.0f + (time_redraw * FLY_SMOOTH_FAC))));

			if (rv3d->persp == RV3D_CAMOB) {
				Object *lock_ob = ED_view3d_cameracontrol_object_get(fly->v3d_camera_control);
				if (lock_ob->protectflag & OB_LOCK_LOCX) dvec[0] = 0.0;
				if (lock_ob->protectflag & OB_LOCK_LOCY) dvec[1] = 0.0;
				if (lock_ob->protectflag & OB_LOCK_LOCZ) dvec[2] = 0.0;
			}

			add_v3_v3(rv3d->ofs, dvec);

			if (rv3d->persp == RV3D_CAMOB) {
				const bool do_rotate = ((fly->xlock != FLY_AXISLOCK_STATE_OFF) ||
				                        (fly->zlock != FLY_AXISLOCK_STATE_OFF) ||
				                        ((moffset[0] || moffset[1]) && !fly->pan_view));
				const bool do_translate = (fly->speed != 0.0f || fly->pan_view);
				flyMoveCamera(C, fly, do_rotate, do_translate);
			}

		}
		else {
			/* we're not redrawing but we need to update the time else the view will jump */
			fly->time_lastdraw = PIL_check_seconds_timer();
		}
		/* end drawing */
		copy_v3_v3(fly->dvec_prev, dvec);
	}

	return OPERATOR_FINISHED;
}

static void flyApply_ndof(bContext *C, FlyInfo *fly)
{
	Object *lock_ob = ED_view3d_cameracontrol_object_get(fly->v3d_camera_control);
	bool has_translate, has_rotate;

	view3d_ndof_fly(fly->ndof,
	                fly->v3d, fly->rv3d,
	                fly->use_precision, lock_ob ? lock_ob->protectflag : 0,
	                &has_translate, &has_rotate);

	if (has_translate || has_rotate) {
		fly->redraw = true;

		if (fly->rv3d->persp == RV3D_CAMOB) {
			flyMoveCamera(C, fly, has_rotate, has_translate);
		}
	}
}

static int fly_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	FlyInfo *fly;

	if (rv3d->viewlock & RV3D_LOCKED)
		return OPERATOR_CANCELLED;

	fly = MEM_callocN(sizeof(FlyInfo), "FlyOperation");

	op->customdata = fly;

	if (initFlyInfo(C, fly, op, event) == false) {
		MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}

	flyEvent(C, fly, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static void fly_cancel(bContext *C, wmOperator *op)
{
	FlyInfo *fly = op->customdata;

	fly->state = FLY_CANCEL;
	flyEnd(C, fly);
	op->customdata = NULL;
}

static int fly_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	int exit_code;
	bool do_draw = false;
	FlyInfo *fly = op->customdata;
	RegionView3D *rv3d = fly->rv3d;
	Object *fly_object = ED_view3d_cameracontrol_object_get(fly->v3d_camera_control);

	fly->redraw = 0;

	flyEvent(C, fly, event);

	if (fly->ndof) { /* 3D mouse overrules [2D mouse + timer] */
		if (event->type == NDOF_MOTION) {
			flyApply_ndof(C, fly);
		}
	}
	else if (event->type == TIMER && event->customdata == fly->timer) {
		flyApply(C, fly);
	}

	do_draw |= fly->redraw;

	exit_code = flyEnd(C, fly);

	if (exit_code != OPERATOR_RUNNING_MODAL)
		do_draw = true;

	if (do_draw) {
		if (rv3d->persp == RV3D_CAMOB) {
			WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, fly_object);
		}

		// puts("redraw!"); // too frequent, commented with NDOF_FLY_DRAW_TOOMUCH for now
		ED_region_tag_redraw(CTX_wm_region(C));
	}

	if (ELEM(exit_code, OPERATOR_FINISHED, OPERATOR_CANCELLED))
		ED_area_headerprint(CTX_wm_area(C), NULL);

	return exit_code;
}

void VIEW3D_OT_fly(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Fly Navigation";
	ot->description = "Interactively fly around the scene";
	ot->idname = "VIEW3D_OT_fly";

	/* api callbacks */
	ot->invoke = fly_invoke;
	ot->cancel = fly_cancel;
	ot->modal = fly_modal;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;
}
