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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_handlers.c
 *  \ingroup edinterface
 */


#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_sensor_types.h"
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"
#include "BLI_string_cursor_utf8.h"

#include "BLF_translation.h"

#include "PIL_time.h"

#include "BKE_blender.h"
#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_tracking.h"
#include "BKE_unit.h"
#include "BKE_paint.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_keyframing.h"

#include "UI_interface.h"

#include "BLF_api.h"

#include "interface_intern.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

/* place the mouse at the scaled down location when un-grabbing */
#define USE_CONT_MOUSE_CORRECT
/* support dragging toggle buttons */
#define USE_DRAG_TOGGLE

/* support dragging multiple number buttons at once */
#define USE_DRAG_MULTINUM

/* so we can avoid very small mouse-moves from jumping away from keyboard navigation [#34936] */
#define USE_KEYNAV_LIMIT

/* drag popups by their header */
#define USE_DRAG_POPUP

/* proto */
static void ui_add_smart_controller(bContext *C, uiBut *from, uiBut *to);
static void ui_add_link(bContext *C, uiBut *from, uiBut *to);
static int ui_do_but_EXIT(bContext *C, uiBut *but, struct uiHandleButtonData *data, const wmEvent *event);

#ifdef USE_KEYNAV_LIMIT
static void ui_mouse_motion_keynav_init(struct uiKeyNavLock *keynav, const wmEvent *event);
static bool ui_mouse_motion_keynav_test(struct uiKeyNavLock *keynav, const wmEvent *event);
#endif

/***************** structs and defines ****************/

#define BUTTON_TOOLTIP_DELAY        0.500
#define BUTTON_FLASH_DELAY          0.020
#define MENU_SCROLL_INTERVAL        0.1
#define BUTTON_AUTO_OPEN_THRESH     0.3
#define BUTTON_MOUSE_TOWARDS_THRESH 1.0
/* pixels to move the cursor to get out of keyboard navigation */
#define BUTTON_KEYNAV_PX_LIMIT      8

#define MENU_TOWARDS_MARGIN 20  /* margin in pixels */
#define MENU_TOWARDS_WIGGLE_ROOM 64  /* tolerance in pixels */

typedef enum uiButtonActivateType {
	BUTTON_ACTIVATE_OVER,
	BUTTON_ACTIVATE,
	BUTTON_ACTIVATE_APPLY,
	BUTTON_ACTIVATE_TEXT_EDITING,
	BUTTON_ACTIVATE_OPEN
} uiButtonActivateType;

typedef enum uiHandleButtonState {
	BUTTON_STATE_INIT,
	BUTTON_STATE_HIGHLIGHT,
	BUTTON_STATE_WAIT_FLASH,
	BUTTON_STATE_WAIT_RELEASE,
	BUTTON_STATE_WAIT_KEY_EVENT,
	BUTTON_STATE_NUM_EDITING,
	BUTTON_STATE_TEXT_EDITING,
	BUTTON_STATE_TEXT_SELECTING,
	BUTTON_STATE_MENU_OPEN,
	BUTTON_STATE_WAIT_DRAG,
	BUTTON_STATE_EXIT
} uiHandleButtonState;


#ifdef USE_DRAG_MULTINUM

/* how far to drag before we check for gesture direction (in pixels),
 * note: half the height of a button is about right... */
#define DRAG_MULTINUM_THRESHOLD_DRAG_X (UI_UNIT_Y / 4)

/* how far to drag horizontally before we stop checkign which buttons the gesture spans (in pixels),
 * locking down the buttons so we can drag freely without worrying about vertical movement. */
#define DRAG_MULTINUM_THRESHOLD_DRAG_Y (UI_UNIT_Y / 4)

/* how strict to be when detecting a vertical gesture, [0.5 == sloppy], [0.9 == strict], (unsigned dot-product)
 * note: we should be quite strict here, since doing a vertical gesture by accident should be avoided,
 * however with some care a user should be able to do a vertical movement without *missing*. */
#define DRAG_MULTINUM_THRESHOLD_VERTICAL (0.75f)


/* a simple version of uiHandleButtonData when accessing multiple buttons */
typedef struct uiButMultiState {
	double origvalue;
	uiBut *but;
} uiButMultiState;

typedef struct uiHandleButtonMulti {
	enum {
		BUTTON_MULTI_INIT_UNSET = 0,    /* gesture direction unknown, wait until mouse has moved enough... */
		BUTTON_MULTI_INIT_SETUP,        /* vertical gesture detected, flag buttons interactively (UI_BUT_DRAG_MULTI) */
		BUTTON_MULTI_INIT_ENABLE,       /* flag buttons finished, apply horizontal motion to active and flagged */
		BUTTON_MULTI_INIT_DISABLE,      /* vertical gesture _not_ detected, take no further action */
	} init;

	bool has_mbuts;  /* any buttons flagged UI_BUT_DRAG_MULTI */
	LinkNode *mbuts;
	uiButStore *bs_mbuts;

	bool is_proportional;

	/* before activating, we need to check gesture direction
	 * accumulate signed cursor movement here so we can tell if this is a vertical motion or not. */
	float drag_dir[2];

	/* values copied direct from event->x,y
	 * used to detect buttons between the current and initial mouse position */
	int drag_start[2];

	/* store x location once BUTTON_MULTI_INIT_SETUP is set,
	 * moving outside this sets BUTTON_MULTI_INIT_ENABLE */
	int drag_lock_x;

} uiHandleButtonMulti;

#endif  /* USE_DRAG_MULTINUM */



typedef struct uiHandleButtonData {
	wmWindowManager *wm;
	wmWindow *window;
	ARegion *region;

	bool interactive;

	/* overall state */
	uiHandleButtonState state;
	int retval;
	/* booleans (could be made into flags) */
	bool cancel, escapecancel;
	bool applied, applied_interactive;
	wmTimer *flashtimer;

	/* edited value */
	char *str, *origstr;
	double value, origvalue, startvalue;
	float vec[3], origvec[3];
#if 0  /* UNUSED */
	int togdual, togonly;
#endif
	ColorBand *coba;

	/* tooltip */
	ARegion *tooltip;
	wmTimer *tooltiptimer;
	
	/* auto open */
	bool used_mouse;
	wmTimer *autoopentimer;

	/* text selection/editing */
	int maxlen, selextend;
	float selstartx;

	/* number editing / dragging */
	/* coords are Window/uiBlock relative (depends on the button) */
	int draglastx, draglasty;
	int dragstartx, dragstarty;
	int draglastvalue;
	int dragstartvalue;
	bool dragchange, draglock;
	int dragsel;
	float dragf, dragfstart;
	CBData *dragcbd;

#ifdef USE_CONT_MOUSE_CORRECT
	/* when ungrabbing buttons which are #ui_is_a_warp_but(), we may want to position them
	 * FLT_MAX signifies do-nothing, use #ui_block_to_window_fl() to get this into a usable space  */
	float ungrab_mval[2];
#endif

	/* menu open (watch uiFreeActiveButtons) */
	uiPopupBlockHandle *menu;
	int menuretval;
	
	/* search box (watch uiFreeActiveButtons) */
	ARegion *searchbox;
#ifdef USE_KEYNAV_LIMIT
	struct uiKeyNavLock searchbox_keynav_state;
#endif

#ifdef USE_DRAG_MULTINUM
	/* Multi-buttons will be updated in unison with the active button. */
	uiHandleButtonMulti multi_data;
#endif

	/* post activate */
	uiButtonActivateType posttype;
	uiBut *postbut;
} uiHandleButtonData;

typedef struct uiAfterFunc {
	struct uiAfterFunc *next, *prev;

	uiButHandleFunc func;
	void *func_arg1;
	void *func_arg2;
	
	uiButHandleNFunc funcN;
	void *func_argN;

	uiButHandleRenameFunc rename_func;
	void *rename_arg1;
	void *rename_orig;
	
	uiBlockHandleFunc handle_func;
	void *handle_func_arg;
	int retval;

	uiMenuHandleFunc butm_func;
	void *butm_func_arg;
	int a2;

	wmOperatorType *optype;
	int opcontext;
	PointerRNA *opptr;

	PointerRNA rnapoin;
	PropertyRNA *rnaprop;

	bContextStore *context;

	char undostr[BKE_UNDO_STR_MAX];
} uiAfterFunc;



static bool ui_is_but_interactive(const uiBut *but, const bool labeledit);
static bool ui_but_contains_pt(uiBut *but, float mx, float my);
static bool ui_mouse_inside_button(ARegion *ar, uiBut *but, int x, int y);
static uiBut *ui_but_find_mouse_over_ex(ARegion *ar, const int x, const int y, const bool labeledit);
static uiBut *ui_but_find_mouse_over(ARegion *ar, const wmEvent *event);
static void button_activate_init(bContext *C, ARegion *ar, uiBut *but, uiButtonActivateType type);
static void button_activate_state(bContext *C, uiBut *but, uiHandleButtonState state);
static void button_activate_exit(bContext *C, uiBut *but, uiHandleButtonData *data,
                                 const bool mousemove, const bool onfree);
static int ui_handler_region_menu(bContext *C, const wmEvent *event, void *userdata);
static void ui_handle_button_activate(bContext *C, ARegion *ar, uiBut *but, uiButtonActivateType type);
static void button_timers_tooltip_remove(bContext *C, uiBut *but);

#ifdef USE_DRAG_MULTINUM
static void ui_multibut_restore(uiHandleButtonData *data, uiBlock *block);
static uiButMultiState *ui_multibut_lookup(uiHandleButtonData *data, const uiBut *but);
#endif

/* buttons clipboard */
static ColorBand but_copypaste_coba = {0};
static CurveMapping but_copypaste_curve = {0};
static bool but_copypaste_curve_alive = false;

/* ******************** menu navigation helpers ************** */
enum eSnapType {
	SNAP_OFF = 0,
	SNAP_ON,
	SNAP_ON_SMALL,
};

static enum eSnapType ui_event_to_snap(const wmEvent *event)
{
	return (event->ctrl) ? (event->shift) ? SNAP_ON_SMALL : SNAP_ON : SNAP_OFF;
}

static void ui_color_snap_hue(const enum eSnapType snap, float *r_hue)
{
	const float snap_increment = (snap == SNAP_ON_SMALL) ? 24 : 12;
	BLI_assert(snap != SNAP_OFF);
	*r_hue = floorf(0.5f + ((*r_hue) * snap_increment)) / snap_increment;
}

/* assumes event type is MOUSEPAN */
void ui_pan_to_scroll(const wmEvent *event, int *type, int *val)
{
	static int lastdy = 0;
	int dy = event->prevy - event->y;

	/* This event should be originally from event->type,
	 * converting wrong event into wheel is bad, see [#33803] */
	BLI_assert(*type == MOUSEPAN);

	/* sign differs, reset */
	if ((dy > 0 && lastdy < 0) || (dy < 0 && lastdy > 0)) {
		lastdy = dy;
	}
	else {
		lastdy += dy;
		
		if (ABS(lastdy) > (int)UI_UNIT_Y) {
			if (U.uiflag2 & USER_TRACKPAD_NATURAL)
				dy = -dy;
			
			*val = KM_PRESS;
			
			if (dy > 0)
				*type = WHEELUPMOUSE;
			else
				*type = WHEELDOWNMOUSE;
			
			lastdy = 0;
		}
	}
}

bool ui_but_is_editable(const uiBut *but)
{
	return !ELEM(but->type, LABEL, SEPR, SEPRLINE, ROUNDBOX, LISTBOX, PROGRESSBAR);
}

static uiBut *ui_but_prev(uiBut *but)
{
	while (but->prev) {
		but = but->prev;
		if (ui_but_is_editable(but)) return but;
	}
	return NULL;
}

static uiBut *ui_but_next(uiBut *but)
{
	while (but->next) {
		but = but->next;
		if (ui_but_is_editable(but)) return but;
	}
	return NULL;
}

static uiBut *ui_but_first(uiBlock *block)
{
	uiBut *but;
	
	but = block->buttons.first;
	while (but) {
		if (ui_but_is_editable(but)) return but;
		but = but->next;
	}
	return NULL;
}

static uiBut *ui_but_last(uiBlock *block)
{
	uiBut *but;
	
	but = block->buttons.last;
	while (but) {
		if (ui_but_is_editable(but)) return but;
		but = but->prev;
	}
	return NULL;
}

static bool ui_is_a_warp_but(uiBut *but)
{
	if (U.uiflag & USER_CONTINUOUS_MOUSE) {
		if (ELEM(but->type, NUM, NUMSLI, HSVCIRCLE, TRACKPREVIEW, HSVCUBE, BUT_CURVE)) {
			return true;
		}
	}

	return false;
}

static float ui_mouse_scale_warp_factor(const bool shift)
{
	return shift ? 0.05f : 1.0f;
}

static void ui_mouse_scale_warp(uiHandleButtonData *data,
                                const float mx, const float my,
                                float *r_mx, float *r_my,
                                const bool shift)
{
	const float fac = ui_mouse_scale_warp_factor(shift);
	
	/* slow down the mouse, this is fairly picky */
	*r_mx = (data->dragstartx * (1.0f - fac) + mx * fac);
	*r_my = (data->dragstarty * (1.0f - fac) + my * fac);
}

/* file selectors are exempt from utf-8 checks */
bool ui_is_but_utf8(const uiBut *but)
{
	if (but->rnaprop) {
		const int subtype = RNA_property_subtype(but->rnaprop);
		return !(ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME, PROP_BYTESTRING));
	}
	else {
		return !(but->flag & UI_BUT_NO_UTF8);
	}
}

/* ********************** button apply/revert ************************/

static ListBase UIAfterFuncs = {NULL, NULL};

static uiAfterFunc *ui_afterfunc_new(void)
{
	uiAfterFunc *after;

	after = MEM_callocN(sizeof(uiAfterFunc), "uiAfterFunc");

	BLI_addtail(&UIAfterFuncs, after);

	return after;
}

/**
 * For executing operators after the button is pressed.
 * (some non operator buttons need to trigger operators), see: [#37795]
 *
 * \note Can only call while handling buttons.
 */
PointerRNA *ui_handle_afterfunc_add_operator(wmOperatorType *ot, int opcontext, bool create_props)
{
	PointerRNA *ptr = NULL;
	uiAfterFunc *after = ui_afterfunc_new();

	after->optype = ot;
	after->opcontext = opcontext;

	if (create_props) {
		ptr = MEM_callocN(sizeof(PointerRNA), __func__);
		WM_operator_properties_create_ptr(ptr, ot);
		after->opptr = ptr;
	}

	return ptr;
}

static void ui_apply_but_func(bContext *C, uiBut *but)
{
	uiAfterFunc *after;
	uiBlock *block = but->block;

	/* these functions are postponed and only executed after all other
	 * handling is done, i.e. menus are closed, in order to avoid conflicts
	 * with these functions removing the buttons we are working with */

	if (but->func || but->funcN || block->handle_func || but->rename_func ||
	    (but->type == BUTM && block->butm_func) || but->optype || but->rnaprop)
	{
		after = ui_afterfunc_new();

		if (but->func && ELEM(but, but->func_arg1, but->func_arg2)) {
			/* exception, this will crash due to removed button otherwise */
			but->func(C, but->func_arg1, but->func_arg2);
		}
		else
			after->func = but->func;

		after->func_arg1 = but->func_arg1;
		after->func_arg2 = but->func_arg2;

		after->funcN = but->funcN;
		after->func_argN = (but->func_argN) ? MEM_dupallocN(but->func_argN) : NULL;

		after->rename_func = but->rename_func;
		after->rename_arg1 = but->rename_arg1;
		after->rename_orig = but->rename_orig; /* needs free! */

		after->handle_func = block->handle_func;
		after->handle_func_arg = block->handle_func_arg;
		after->retval = but->retval;

		if (but->type == BUTM) {
			after->butm_func = block->butm_func;
			after->butm_func_arg = block->butm_func_arg;
			after->a2 = but->a2;
		}

		after->optype = but->optype;
		after->opcontext = but->opcontext;
		after->opptr = but->opptr;

		after->rnapoin = but->rnapoin;
		after->rnaprop = but->rnaprop;

		if (but->context)
			after->context = CTX_store_copy(but->context);

		but->optype = NULL;
		but->opcontext = 0;
		but->opptr = NULL;
	}
}

/* typically call ui_apply_undo(), ui_apply_autokey() */
static void ui_apply_undo(uiBut *but)
{
	uiAfterFunc *after;

	if (but->flag & UI_BUT_UNDO) {
		const char *str = NULL;

		/* define which string to use for undo */
		if (ELEM(but->type, LINK, INLINK)) str = "Add button link";
		else if (but->type == MENU) str = but->drawstr;
		else if (but->drawstr[0]) str = but->drawstr;
		else str = but->tip;

		/* fallback, else we don't get an undo! */
		if (str == NULL || str[0] == '\0') {
			str = "Unknown Action";
		}

		/* delayed, after all other funcs run, popups are closed, etc */
		after = ui_afterfunc_new();
		BLI_strncpy(after->undostr, str, sizeof(after->undostr));
	}
}

static void ui_apply_autokey(bContext *C, uiBut *but)
{
	Scene *scene = CTX_data_scene(C);

	/* try autokey */
	ui_but_anim_autokey(C, but, scene, scene->r.cfra);

	/* make a little report about what we've done! */
	if (but->rnaprop) {
		char *buf = WM_prop_pystring_assign(C, &but->rnapoin, but->rnaprop, but->rnaindex);
		if (buf) {
			BKE_report(CTX_wm_reports(C), RPT_PROPERTY, buf);
			MEM_freeN(buf);

			WM_event_add_notifier(C, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);
		}
	}
}

static void ui_apply_but_funcs_after(bContext *C)
{
	uiAfterFunc *afterf, after;
	PointerRNA opptr;
	ListBase funcs;

	/* copy to avoid recursive calls */
	funcs = UIAfterFuncs;
	BLI_listbase_clear(&UIAfterFuncs);

	for (afterf = funcs.first; afterf; afterf = after.next) {
		after = *afterf; /* copy to avoid memleak on exit() */
		BLI_freelinkN(&funcs, afterf);

		if (after.context)
			CTX_store_set(C, after.context);

		if (after.opptr) {
			/* free in advance to avoid leak on exit */
			opptr = *after.opptr,
			MEM_freeN(after.opptr);
		}

		if (after.optype)
			WM_operator_name_call_ptr(C, after.optype, after.opcontext, (after.opptr) ? &opptr : NULL);

		if (after.opptr)
			WM_operator_properties_free(&opptr);

		if (after.rnapoin.data)
			RNA_property_update(C, &after.rnapoin, after.rnaprop);

		if (after.context) {
			CTX_store_set(C, NULL);
			CTX_store_free(after.context);
		}

		if (after.func)
			after.func(C, after.func_arg1, after.func_arg2);
		if (after.funcN)
			after.funcN(C, after.func_argN, after.func_arg2);
		if (after.func_argN)
			MEM_freeN(after.func_argN);
		
		if (after.handle_func)
			after.handle_func(C, after.handle_func_arg, after.retval);
		if (after.butm_func)
			after.butm_func(C, after.butm_func_arg, after.a2);
		
		if (after.rename_func)
			after.rename_func(C, after.rename_arg1, after.rename_orig);
		if (after.rename_orig)
			MEM_freeN(after.rename_orig);
		
		if (after.undostr[0])
			ED_undo_push(C, after.undostr);
	}
}

static void ui_apply_but_BUT(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_BUTM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_set_but_val(but, but->hardmin);
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_BLOCK(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	if (but->type == MENU)
		ui_set_but_val(but, data->value);

	ui_check_but(but);
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_TOG(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	double value;
	int w, lvalue, push;
	
	value = ui_get_but_val(but);
	lvalue = (int)value;
	
	if (but->bit) {
		w = UI_BITBUT_TEST(lvalue, but->bitnr);
		if (w) lvalue = UI_BITBUT_CLR(lvalue, but->bitnr);
		else   lvalue = UI_BITBUT_SET(lvalue, but->bitnr);
		
		ui_set_but_val(but, (double)lvalue);
		if (but->type == ICONTOG || but->type == ICONTOGN) ui_check_but(but);
	}
	else {
		
		if (value == 0.0) push = 1;
		else push = 0;
		
		if (ELEM(but->type, TOGN, ICONTOGN, OPTIONN)) push = !push;
		ui_set_but_val(but, (double)push);
		if (but->type == ICONTOG || but->type == ICONTOGN) ui_check_but(but);
	}
	
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_ROW(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data)
{
	uiBut *bt;

	ui_set_but_val(but, but->hardmax);

	ui_apply_but_func(C, but);

	/* states of other row buttons */
	for (bt = block->buttons.first; bt; bt = bt->next)
		if (bt != but && bt->poin == but->poin && ELEM(bt->type, ROW, LISTROW))
			ui_check_but(bt);

	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_TEX(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	if (!data->str)
		return;

	ui_set_but_string(C, but, data->str);
	ui_check_but(but);

	/* give butfunc the original text too */
	/* feature used for bone renaming, channels, etc */
	/* afterfunc frees origstr */
	but->rename_orig = data->origstr;
	data->origstr = NULL;
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_NUM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	if (data->str) {
		if (ui_set_but_string(C, but, data->str)) {
			data->value = ui_get_but_val(but);
		}
		else {
			data->cancel = true;
			return;
		}
	}
	else
		ui_set_but_val(but, data->value);

	ui_check_but(but);
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_VEC(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_set_but_vectorf(but, data->vec);
	ui_check_but(but);
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_COLORBAND(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_CURVE(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = true;
}

/* ****************** drag drop code *********************** */


#ifdef USE_DRAG_MULTINUM

/* small multi-but api */
static void ui_multibut_add(uiHandleButtonData *data, uiBut *but)
{
	uiButMultiState *mbut_state;

	BLI_assert(but->flag & UI_BUT_DRAG_MULTI);
	BLI_assert(data->multi_data.has_mbuts);


	mbut_state = MEM_callocN(sizeof(*mbut_state), __func__);
	mbut_state->but = but;
	mbut_state->origvalue = ui_get_but_val(but);

	BLI_linklist_prepend(&data->multi_data.mbuts, mbut_state);

	UI_butstore_register(data->multi_data.bs_mbuts, &mbut_state->but);
}

static uiButMultiState *ui_multibut_lookup(uiHandleButtonData *data, const uiBut *but)
{
	LinkNode *l;

	for (l = data->multi_data.mbuts; l; l = l->next) {
		uiButMultiState *mbut_state;

		mbut_state = l->link;

		if (mbut_state->but == but) {
			return mbut_state;
		}
	}

	return NULL;
}

static void ui_multibut_restore(uiHandleButtonData *data, uiBlock *block)
{
	uiBut *but;

	for (but = block->buttons.first; but; but = but->next) {
		if (but->flag & UI_BUT_DRAG_MULTI) {
			uiButMultiState *mbut_state = ui_multibut_lookup(data, but);
			if (mbut_state) {
				ui_set_but_val(but, mbut_state->origvalue);
			}
		}
	}
}

static void ui_multibut_free(uiHandleButtonData *data, uiBlock *block)
{
	BLI_linklist_freeN(data->multi_data.mbuts);
	data->multi_data.mbuts = NULL;

	if (data->multi_data.bs_mbuts) {
		UI_butstore_free(block, data->multi_data.bs_mbuts);
		data->multi_data.bs_mbuts = NULL;
	}
}

static bool ui_multibut_states_tag(uiBut *but_active, uiHandleButtonData *data, const wmEvent *event)
{
	uiBut *but;
	float seg[2][2];
	bool changed = false;

	seg[0][0] = data->multi_data.drag_start[0];
	seg[0][1] = data->multi_data.drag_start[1];

	seg[1][0] = event->x;
	seg[1][1] = event->y;

	BLI_assert(data->multi_data.init == BUTTON_MULTI_INIT_SETUP);

	ui_window_to_block_fl(data->region, but_active->block, &seg[0][0], &seg[0][1]);
	ui_window_to_block_fl(data->region, but_active->block, &seg[1][0], &seg[1][1]);

	data->multi_data.has_mbuts = false;

	/* follow ui_but_find_mouse_over_ex logic */
	for (but = but_active->block->buttons.first; but; but = but->next) {
		bool drag_prev = false;
		bool drag_curr = false;

		/* re-set each time */
		if (but->flag & UI_BUT_DRAG_MULTI) {
			but->flag &= ~UI_BUT_DRAG_MULTI;
			drag_prev = true;
		}

		if (ui_is_but_interactive(but, false)) {

			/* drag checks */
			if (but_active != but) {
				if (ui_is_but_compatible(but_active, but)) {

					BLI_assert(but->active == NULL);

					/* finally check for overlap */
					if (BLI_rctf_isect_segment(&but->rect, seg[0], seg[1])) {

						but->flag |= UI_BUT_DRAG_MULTI;
						data->multi_data.has_mbuts = true;
						drag_curr = true;
					}
				}
			}
		}

		changed |= (drag_prev != drag_curr);
	}

	return changed;
}

static void ui_multibut_states_create(uiBut *but_active, uiHandleButtonData *data)
{
	uiBut *but;

	BLI_assert(data->multi_data.init == BUTTON_MULTI_INIT_SETUP);

	data->multi_data.bs_mbuts = UI_butstore_create(but_active->block);

	for (but = but_active->block->buttons.first; but; but = but->next) {
		if (but->flag & UI_BUT_DRAG_MULTI) {
			ui_multibut_add(data, but);
		}
	}

	/* edit buttons proportionally to eachother
	 * note: if we mix buttons which are proportional and others which are not,
	 * this may work a bit strangely */
	if (but_active->rnaprop) {
		if ((data->origvalue != 0.0) && (RNA_property_flag(but_active->rnaprop) & PROP_PROPORTIONAL)) {
			data->multi_data.is_proportional = true;
		}
	}
}

static void ui_multibut_states_apply(bContext *C, uiHandleButtonData *data, uiBlock *block)
{
	ARegion *ar = data->region;
	const double value_delta = data->value - data->origvalue;
	const double value_scale = data->multi_data.is_proportional ? (data->value / data->origvalue) : 0.0;
	uiBut *but;

	BLI_assert(data->multi_data.init == BUTTON_MULTI_INIT_ENABLE);

	for (but = block->buttons.first; but; but = but->next) {
		if (but->flag & UI_BUT_DRAG_MULTI) {
			/* mbut_states for delta */
			uiButMultiState *mbut_state = ui_multibut_lookup(data, but);

			if (mbut_state) {
				void *active_back;

				ui_button_execute_begin(C, ar, but, &active_back);
				BLI_assert(active_back == NULL);
				/* no need to check 'data->state' here */
				if (data->str) {
					/* entering text (set all) */
					but->active->value = data->value;
					ui_set_but_string(C, but, data->str);
				}
				else {
					/* dragging (use delta) */
					if (data->multi_data.is_proportional) {
						but->active->value = mbut_state->origvalue * value_scale;
					}
					else {
						but->active->value = mbut_state->origvalue + value_delta;
					}

					/* clamp based on soft limits, see: T40154 */
					CLAMP(but->active->value, (double)but->softmin, (double)but->softmax);
				}
				ui_button_execute_end(C, ar, but, active_back);
			}
			else {
				/* highly unlikely */
				printf("%s: cant find button\n", __func__);
			}
			/* end */

		}
	}
}

#endif  /* USE_DRAG_MULTINUM */


#ifdef USE_DRAG_TOGGLE

typedef struct uiDragToggleHandle {
	/* init */
	bool is_init;
	bool is_set;
	float but_cent_start[2];
	eButType but_type_start;

	bool xy_lock[2];
	int  xy_init[2];
	int  xy_last[2];
} uiDragToggleHandle;

static bool ui_drag_toggle_set_xy_xy(bContext *C, ARegion *ar, const bool is_set, const eButType but_type_start,
                                     const int xy_src[2], const int xy_dst[2])
{
	/* popups such as layers won't re-evaluate on redraw */
	const bool do_check = (ar->regiontype == RGN_TYPE_TEMPORARY);
	bool changed = false;
	uiBlock *block;

	for (block = ar->uiblocks.first; block; block = block->next) {
		uiBut *but;

		float xy_a_block[2] = {UNPACK2(xy_src)};
		float xy_b_block[2] = {UNPACK2(xy_dst)};

		ui_window_to_block_fl(ar, block, &xy_a_block[0], &xy_a_block[1]);
		ui_window_to_block_fl(ar, block, &xy_b_block[0], &xy_b_block[1]);

		for (but = block->buttons.first; but; but = but->next) {
			/* Note: ctrl is always true here because (at least for now) we always want to consider text control
			 *       in this case, even when not embossed. */
			if (ui_is_but_interactive(but, true)) {
				if (BLI_rctf_isect_segment(&but->rect, xy_a_block, xy_b_block)) {

					/* execute the button */
					if (ui_is_but_bool(but) && but->type == but_type_start) {
						/* is it pressed? */
						bool is_set_but = ui_is_but_push(but);
						BLI_assert(ui_is_but_bool(but) == true);
						if (is_set_but != is_set) {
							uiButExecute(C, but);
							if (do_check) {
								ui_check_but(but);
							}
							changed = true;
						}
					}
					/* done */

				}
			}
		}
	}

	return changed;
}

static void ui_drag_toggle_set(bContext *C, uiDragToggleHandle *drag_info, const int xy_input[2])
{
	ARegion *ar = CTX_wm_region(C);
	bool do_draw = false;
	int xy[2];

	/**
	 * Initialize Locking:
	 *
	 * Check if we need to initialize the lock axis by finding if the first
	 * button we mouse over is X or Y aligned, then lock the mouse to that axis after.
	 */
	if (drag_info->is_init == false) {
		/* first store the buttons original coords */
		uiBut *but = ui_but_find_mouse_over_ex(ar, xy_input[0], xy_input[1], true);

		if (but) {
			if (but->flag & UI_BUT_DRAG_LOCK) {
				const float but_cent_new[2] = {BLI_rctf_cent_x(&but->rect),
				                               BLI_rctf_cent_y(&but->rect)};

				/* check if this is a different button, chances are high the button wont move about :) */
				if (len_manhattan_v2v2(drag_info->but_cent_start, but_cent_new) > 1.0f) {
					if (fabsf(drag_info->but_cent_start[0] - but_cent_new[0]) <
					    fabsf(drag_info->but_cent_start[1] - but_cent_new[1]))
					{
						drag_info->xy_lock[0] = true;
					}
					else {
						drag_info->xy_lock[1] = true;
					}
					drag_info->is_init = true;
				}
			}
			else {
				drag_info->is_init = true;
			}
		}
	}
	/* done with axis locking */


	xy[0] = (drag_info->xy_lock[0] == false) ? xy_input[0] : drag_info->xy_last[0];
	xy[1] = (drag_info->xy_lock[1] == false) ? xy_input[1] : drag_info->xy_last[1];


	/* touch all buttons between last mouse coord and this one */
	do_draw = ui_drag_toggle_set_xy_xy(C, ar, drag_info->is_set, drag_info->but_type_start, drag_info->xy_last, xy);

	if (do_draw) {
		ED_region_tag_redraw(ar);
	}

	copy_v2_v2_int(drag_info->xy_last, xy);
}

static void ui_handler_region_drag_toggle_remove(bContext *UNUSED(C), void *userdata)
{
	uiDragToggleHandle *drag_info = userdata;
	MEM_freeN(drag_info);
}

static int ui_handler_region_drag_toggle(bContext *C, const wmEvent *event, void *userdata)
{
	uiDragToggleHandle *drag_info = userdata;
	bool done = false;

	switch (event->type) {
		case LEFTMOUSE:
		{
			if (event->val != KM_PRESS) {
				done = true;
			}
			break;
		}
		case MOUSEMOVE:
		{
			ui_drag_toggle_set(C, drag_info, &event->x);
			break;
		}
	}

	if (done) {
		wmWindow *win = CTX_wm_window(C);
		ARegion *ar = CTX_wm_region(C);
		uiBut *but = ui_but_find_mouse_over_ex(ar, drag_info->xy_init[0], drag_info->xy_init[1], true);

		if (but) {
			ui_apply_undo(but);
		}

		WM_event_remove_ui_handler(&win->modalhandlers,
		                           ui_handler_region_drag_toggle,
		                           ui_handler_region_drag_toggle_remove,
		                           drag_info, false);
		ui_handler_region_drag_toggle_remove(C, drag_info);

		WM_event_add_mousemove(C);
		return WM_UI_HANDLER_BREAK;
	}
	else {
		return WM_UI_HANDLER_CONTINUE;
	}
}

static bool ui_is_but_drag_toggle(const uiBut *but)
{
	return ((ui_is_but_bool(but) == true) &&
	        /* menu check is importnt so the button dragged over isn't removed instantly */
	        (ui_block_is_menu(but->block) == false));
}

#endif  /* USE_DRAG_TOGGLE */


static bool ui_but_mouse_inside_icon(uiBut *but, ARegion *ar, const wmEvent *event)
{
	rcti rect;
	int x = event->x, y = event->y;
	
	ui_window_to_block(ar, but->block, &x, &y);
	
	BLI_rcti_rctf_copy(&rect, &but->rect);
	
	if (but->imb || but->type == COLOR) {
		/* use button size itself */
	}
	else if (but->drawflag & UI_BUT_ICON_LEFT) {
		rect.xmax = rect.xmin + (BLI_rcti_size_y(&rect));
	}
	else {
		int delta = BLI_rcti_size_x(&rect) - BLI_rcti_size_y(&rect);
		rect.xmin += delta / 2;
		rect.xmax -= delta / 2;
	}
	
	return BLI_rcti_isect_pt(&rect, x, y);
}

static bool ui_but_start_drag(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	/* prevent other WM gestures to start while we try to drag */
	WM_gestures_remove(C);

	if (ABS(data->dragstartx - event->x) + ABS(data->dragstarty - event->y) > U.dragthreshold) {

		button_activate_state(C, but, BUTTON_STATE_EXIT);
		data->cancel = true;
#ifdef USE_DRAG_TOGGLE
		if (ui_is_but_bool(but)) {
			uiDragToggleHandle *drag_info = MEM_callocN(sizeof(*drag_info), __func__);
			ARegion *ar_prev;

			/* call here because regular mouse-up event wont run,
			 * typically 'button_activate_exit()' handles this */
			ui_apply_autokey(C, but);

			drag_info->is_set = ui_is_but_push(but);
			drag_info->but_cent_start[0] = BLI_rctf_cent_x(&but->rect);
			drag_info->but_cent_start[1] = BLI_rctf_cent_y(&but->rect);
			drag_info->but_type_start = but->type;
			copy_v2_v2_int(drag_info->xy_init, &event->x);
			copy_v2_v2_int(drag_info->xy_last, &event->x);

			/* needed for toggle drag on popups */
			ar_prev = CTX_wm_region(C);
			CTX_wm_region_set(C, data->region);

			WM_event_add_ui_handler(C, &data->window->modalhandlers,
			                        ui_handler_region_drag_toggle,
			                        ui_handler_region_drag_toggle_remove,
			                        drag_info);

			CTX_wm_region_set(C, ar_prev);
		}
		else
#endif
		if (but->type == COLOR) {
			bool valid = false;
			uiDragColorHandle *drag_info = MEM_callocN(sizeof(*drag_info), __func__);

			/* TODO support more button pointer types */
			if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
				RNA_property_float_get_array(&but->rnapoin, but->rnaprop, drag_info->color);
				drag_info->gamma_corrected = true;
				valid = true;
			}
			else if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
				RNA_property_float_get_array(&but->rnapoin, but->rnaprop, drag_info->color);
				drag_info->gamma_corrected = false;
				valid = true;
			}
			else if (but->pointype == UI_BUT_POIN_FLOAT) {
				copy_v3_v3(drag_info->color, (float *)but->poin);
				valid = true;
			}
			else if (but->pointype == UI_BUT_POIN_CHAR) {
				rgb_uchar_to_float(drag_info->color, (unsigned char *)but->poin);
				valid = true;
			}

			if (valid) {
				WM_event_start_drag(C, ICON_COLOR, WM_DRAG_COLOR, drag_info, 0.0, WM_DRAG_FREE_DATA);
			}
			else {
				MEM_freeN(drag_info);
				return false;
			}
		}
		else {
			wmDrag *drag;

			drag = WM_event_start_drag(C, but->icon, but->dragtype, but->dragpoin, ui_get_but_val(but), WM_DRAG_NOP);
			if (but->imb)
				WM_event_drag_image(drag, but->imb, but->imb_scale, BLI_rctf_size_x(&but->rect), BLI_rctf_size_y(&but->rect));
		}
		return true;
	}
	
	return false;
}

/* ********************** linklines *********************** */

static void ui_delete_active_linkline(uiBlock *block)
{
	uiBut *but;
	uiLink *link;
	uiLinkLine *line, *nline;
	int a, b;

	for (but = block->buttons.first; but; but = but->next) {
		if (but->type == LINK && but->link) {
			for (line = but->link->lines.first; line; line = nline) {
				nline = line->next;
				
				if (line->flag & UI_SELECT) {
					BLI_remlink(&but->link->lines, line);
					
					link = line->from->link;
					
					/* are there more pointers allowed? */
					if (link->ppoin) {
						
						if (*(link->totlink) == 1) {
							*(link->totlink) = 0;
							MEM_freeN(*(link->ppoin));
							*(link->ppoin) = NULL;
						}
						else {
							b = 0;
							for (a = 0; a < (*(link->totlink)); a++) {
								
								if ((*(link->ppoin))[a] != line->to->poin) {
									(*(link->ppoin))[b] = (*(link->ppoin))[a];
									b++;
								}
							}
							(*(link->totlink))--;
						}
					}
					else {
						*(link->poin) = NULL;
					}
					
					MEM_freeN(line);
				}
			}
		}
	}
}


static uiLinkLine *ui_is_a_link(uiBut *from, uiBut *to)
{
	uiLinkLine *line;
	uiLink *link;
	
	link = from->link;
	if (link) {
		for (line = link->lines.first; line; line = line->next) {
			if (line->from == from && line->to == to) {
				return line;
			}
		}
	}
	return NULL;
}

/* XXX BAD BAD HACK, fixme later **************** */
/* Try to add an AND Controller between the sensor and the actuator logic bricks and to connect them all */
static void ui_add_smart_controller(bContext *C, uiBut *from, uiBut *to)
{
	Object *ob = NULL;
	bSensor *sens_iter;
	bActuator *act_to, *act_iter;
	bController *cont;
	bController ***sens_from_links;
	uiBut *tmp_but;

	uiLink *link = from->link;

	PointerRNA props_ptr, object_ptr;
	
	if (link->ppoin)
		sens_from_links = (bController ***)(link->ppoin);
	else return;

	act_to = (bActuator *)(to->poin);

	/* (1) get the object */
	CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects)
	{
		for (sens_iter = ob_iter->sensors.first; sens_iter; sens_iter = sens_iter->next) {
			if (&(sens_iter->links) == sens_from_links) {
				ob = ob_iter;
				break;
			}
		}
		if (ob) break;
	} CTX_DATA_END;

	if (!ob) return;

	/* (2) check if the sensor and the actuator are from the same object */
	for (act_iter = ob->actuators.first; act_iter; act_iter = (bActuator *)act_iter->next) {
		if (act_iter == act_to)
			break;
	}

	/* only works if the sensor and the actuator are from the same object */
	if (!act_iter) return;
	
	/* in case the linked controller is not the active one */
	RNA_pointer_create((ID *)ob, &RNA_Object, ob, &object_ptr);
	
	WM_operator_properties_create(&props_ptr, "LOGIC_OT_controller_add");
	RNA_string_set(&props_ptr, "object", ob->id.name + 2);

	/* (3) add a new controller */
	if (WM_operator_name_call(C, "LOGIC_OT_controller_add", WM_OP_EXEC_DEFAULT, &props_ptr) & OPERATOR_FINISHED) {
		cont = (bController *)ob->controllers.last;
		cont->type = CONT_LOGIC_AND; /* Quick fix to make sure we always have an AND controller. It might be nicer to make sure the operator gives us the right one though... */

		/* (4) link the sensor->controller->actuator */
		tmp_but = MEM_callocN(sizeof(uiBut), "uiBut");
		uiSetButLink(tmp_but, (void **)&cont, (void ***)&(cont->links), &(cont->totlinks), from->link->tocode, (int)to->hardmin);
		tmp_but->hardmin = from->link->tocode;
		tmp_but->poin = (char *)cont;

		tmp_but->type = INLINK;
		ui_add_link(C, from, tmp_but);

		tmp_but->type = LINK;
		ui_add_link(C, tmp_but, to);

		/* (5) garbage collection */
		MEM_freeN(tmp_but->link);
		MEM_freeN(tmp_but);
	}
	WM_operator_properties_free(&props_ptr);
}

static void ui_add_link(bContext *C, uiBut *from, uiBut *to)
{
	/* in 'from' we have to add a link to 'to' */
	uiLink *link;
	uiLinkLine *line;
	void **oldppoin;
	int a;
	
	if ((line = ui_is_a_link(from, to))) {
		line->flag |= UI_SELECT;
		ui_delete_active_linkline(from->block);
		return;
	}

	if (from->type == INLINK && to->type == INLINK) {
		return;
	}
	else if (from->type == LINK && to->type == INLINK) {
		if (from->link->tocode != (int)to->hardmin) {
			ui_add_smart_controller(C, from, to);
			return;
		}
	}
	else if (from->type == INLINK && to->type == LINK) {
		if (to->link->tocode == (int)from->hardmin) {
			return;
		}
	}
	
	link = from->link;
	
	/* are there more pointers allowed? */
	if (link->ppoin) {
		oldppoin = *(link->ppoin);
		
		(*(link->totlink))++;
		*(link->ppoin) = MEM_callocN(*(link->totlink) * sizeof(void *), "new link");
		
		for (a = 0; a < (*(link->totlink)) - 1; a++) {
			(*(link->ppoin))[a] = oldppoin[a];
		}
		(*(link->ppoin))[a] = to->poin;
		
		if (oldppoin) MEM_freeN(oldppoin);
	}
	else {
		*(link->poin) = to->poin;
	}
	
}


static void ui_apply_but_LINK(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ARegion *ar = CTX_wm_region(C);
	uiBut *bt;
	
	for (bt = but->block->buttons.first; bt; bt = bt->next) {
		if (ui_mouse_inside_button(ar, bt, but->linkto[0] + ar->winrct.xmin, but->linkto[1] + ar->winrct.ymin) )
			break;
	}
	if (bt && bt != but) {
		if (!ELEM(bt->type, LINK, INLINK) || !ELEM(but->type, LINK, INLINK))
			return;
		
		if (but->type == LINK) ui_add_link(C, but, bt);
		else ui_add_link(C, bt, but);

		ui_apply_but_func(C, but);
		data->retval = but->retval;
	}
	data->applied = true;
}

static void ui_apply_but_IMAGE(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_HISTOGRAM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_WAVEFORM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = true;
}

static void ui_apply_but_TRACKPREVIEW(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = true;
}


static void ui_apply_button(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const bool interactive)
{
	char *editstr;
	double *editval;
	float *editvec;
	ColorBand *editcoba;
	CurveMapping *editcumap;

	data->retval = 0;

	/* if we cancel and have not applied yet, there is nothing to do,
	 * otherwise we have to restore the original value again */
	if (data->cancel) {
		if (!data->applied)
			return;

		if (data->str) MEM_freeN(data->str);
		data->str = data->origstr;
		data->origstr = NULL;
		data->value = data->origvalue;
		data->origvalue = 0.0;
		copy_v3_v3(data->vec, data->origvec);
		data->origvec[0] = data->origvec[1] = data->origvec[2] = 0.0f;
	}
	else {
		/* we avoid applying interactive edits a second time
		 * at the end with the appliedinteractive flag */
		if (interactive) {
			data->applied_interactive = true;
		}
		else if (data->applied_interactive) {
			return;
		}
	}

	/* ensures we are writing actual values */
	editstr = but->editstr;
	editval = but->editval;
	editvec = but->editvec;
	editcoba = but->editcoba;
	editcumap = but->editcumap;
	but->editstr = NULL;
	but->editval = NULL;
	but->editvec = NULL;
	but->editcoba = NULL;
	but->editcumap = NULL;

	/* handle different types */
	switch (but->type) {
		case BUT:
			ui_apply_but_BUT(C, but, data);
			break;
		case TEX:
		case SEARCH_MENU_UNLINK:
		case SEARCH_MENU:
			ui_apply_but_TEX(C, but, data);
			break;
		case TOGBUT: 
		case TOG: 
		case ICONTOG:
		case ICONTOGN:
		case TOGN:
		case OPTION:
		case OPTIONN:
			ui_apply_but_TOG(C, but, data);
			break;
		case ROW:
		case LISTROW:
			ui_apply_but_ROW(C, block, but, data);
			break;
		case SCROLL:
		case GRIP:
		case NUM:
		case NUMSLI:
			ui_apply_but_NUM(C, but, data);
			break;
		case MENU:
		case BLOCK:
		case PULLDOWN:
			ui_apply_but_BLOCK(C, but, data);
			break;
		case COLOR:
			if (data->cancel)
				ui_apply_but_VEC(C, but, data);
			else
				ui_apply_but_BLOCK(C, but, data);
			break;
		case BUTM:
			ui_apply_but_BUTM(C, but, data);
			break;
		case BUT_NORMAL:
		case HSVCUBE:
		case HSVCIRCLE:
			ui_apply_but_VEC(C, but, data);
			break;
		case BUT_COLORBAND:
			ui_apply_but_COLORBAND(C, but, data);
			break;
		case BUT_CURVE:
			ui_apply_but_CURVE(C, but, data);
			break;
		case KEYEVT:
		case HOTKEYEVT:
			ui_apply_but_BUT(C, but, data);
			break;
		case LINK:
		case INLINK:
			ui_apply_but_LINK(C, but, data);
			break;
		case BUT_IMAGE:
			ui_apply_but_IMAGE(C, but, data);
			break;
		case HISTOGRAM:
			ui_apply_but_HISTOGRAM(C, but, data);
			break;
		case WAVEFORM:
			ui_apply_but_WAVEFORM(C, but, data);
			break;
		case TRACKPREVIEW:
			ui_apply_but_TRACKPREVIEW(C, but, data);
			break;
		default:
			break;
	}

#ifdef USE_DRAG_MULTINUM
	if (data->multi_data.has_mbuts) {
		if (data->multi_data.init == BUTTON_MULTI_INIT_ENABLE) {
			if (data->cancel) {
				ui_multibut_restore(data, block);
			}
			else {
				ui_multibut_states_apply(C, data, block);
			}
		}
	}
#endif

	but->editstr = editstr;
	but->editval = editval;
	but->editvec = editvec;
	but->editcoba = editcoba;
	but->editcumap = editcumap;
}

/* ******************* drop event ********************  */

/* only call if event type is EVT_DROP */
static void ui_but_drop(bContext *C, const wmEvent *event, uiBut *but, uiHandleButtonData *data)
{
	wmDrag *wmd;
	ListBase *drags = event->customdata; /* drop event type has listbase customdata by default */
	
	for (wmd = drags->first; wmd; wmd = wmd->next) {
		if (wmd->type == WM_DRAG_ID) {
			/* align these types with UI_but_active_drop_name */
			if (ELEM(but->type, TEX, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
				ID *id = (ID *)wmd->poin;
				
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
				BLI_strncpy(data->str, id->name + 2, data->maxlen);

				if (ELEM(but->type, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
					but->changed = true;
					ui_searchbox_update(C, data->searchbox, but, true);
				}

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}
	
}

/* ******************* copy and paste ********************  */

/* c = copy, v = paste */
static void ui_but_copy_paste(bContext *C, uiBut *but, uiHandleButtonData *data, char mode)
{
	int buf_paste_len = 0;
	const char *buf_paste = "";
	bool buf_paste_alloc = false;

	if (mode == 'v' && but->lock  == true) {
		return;
	}

	if (mode == 'c') {
		/* disallow copying from any passwords */
		if (but->rnaprop && (RNA_property_subtype(but->rnaprop) == PROP_PASSWORD)) {
			return;
		}
	}

	if (mode == 'v') {
		/* extract first line from clipboard in case of multi-line copies */
		const char *buf_paste_test;

		buf_paste_test = WM_clipboard_text_get_firstline(false, &buf_paste_len);
		if (buf_paste_test) {
			buf_paste = buf_paste_test;
			buf_paste_alloc = true;
		}
	}

	/* No return from here down */


	/* numeric value */
	if (ELEM(but->type, NUM, NUMSLI)) {
		
		if (but->poin == NULL && but->rnapoin.data == NULL) {
			/* pass */
		}
		else if (mode == 'c') {
			/* Get many decimal places, then strip trailing zeros.
			 * note: too high values start to give strange results (6 or so is ok) */
			char buf_copy[UI_MAX_DRAW_STR];
			ui_get_but_string_ex(but, buf_copy, sizeof(buf_copy), 6);
			BLI_str_rstrip_float_zero(buf_copy, '\0');

			WM_clipboard_text_set(buf_copy, 0);
		}
		else {
			double val;

			if (ui_set_but_string_eval_num(C, but, buf_paste, &val)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				data->value = val;
				ui_set_but_string(C, but, buf_paste);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}

	/* NORMAL button */
	else if (but->type == BUT_NORMAL) {
		float xyz[3];

		if (but->poin == NULL && but->rnapoin.data == NULL) {
			/* pass */
		}
		else if (mode == 'c') {
			char buf_copy[UI_MAX_DRAW_STR];
			ui_get_but_vectorf(but, xyz);
			BLI_snprintf(buf_copy, sizeof(buf_copy), "[%f, %f, %f]", xyz[0], xyz[1], xyz[2]);
			WM_clipboard_text_set(buf_copy, 0);
		}
		else {
			if (sscanf(buf_paste, "[%f, %f, %f]", &xyz[0], &xyz[1], &xyz[2]) == 3) {
				if (normalize_v3(xyz) == 0.0f) {
					/* better set Z up then have a zero vector */
					xyz[2] = 1.0;
				}
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				ui_set_but_vectorf(but, xyz);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}


	/* RGB triple */
	else if (but->type == COLOR) {
		float rgba[4];
		
		if (but->poin == NULL && but->rnapoin.data == NULL) {
			/* pass */
		}
		else if (mode == 'c') {
			char buf_copy[UI_MAX_DRAW_STR];

			if (but->rnaprop && RNA_property_array_length(&but->rnapoin, but->rnaprop) == 4)
				rgba[3] = RNA_property_float_get_index(&but->rnapoin, but->rnaprop, 3);
			else
				rgba[3] = 1.0f;
			
			ui_get_but_vectorf(but, rgba);
			/* convert to linear color to do compatible copy between gamma and non-gamma */
			if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
				srgb_to_linearrgb_v3_v3(rgba, rgba);

			BLI_snprintf(buf_copy, sizeof(buf_copy), "[%f, %f, %f, %f]", rgba[0], rgba[1], rgba[2], rgba[3]);
			WM_clipboard_text_set(buf_copy, 0);
			
		}
		else {
			if (sscanf(buf_paste, "[%f, %f, %f, %f]", &rgba[0], &rgba[1], &rgba[2], &rgba[3]) == 4) {
				/* assume linear colors in buffer */
				if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
					linearrgb_to_srgb_v3_v3(rgba, rgba);

				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				ui_set_but_vectorf(but, rgba);
				if (but->rnaprop && RNA_property_array_length(&but->rnapoin, but->rnaprop) == 4)
					RNA_property_float_set_index(&but->rnapoin, but->rnaprop, 3, rgba[3]);

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}

	/* text/string and ID data */
	else if (ELEM(but->type, TEX, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
		uiHandleButtonData *active_data = but->active;

		if (but->poin == NULL && but->rnapoin.data == NULL) {
			/* pass */
		}
		else if (mode == 'c') {
			button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			WM_clipboard_text_set(active_data->str, 0);
			active_data->cancel = true;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else {
			button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);

			if (ui_is_but_utf8(but))
				BLI_strncpy_utf8(active_data->str, buf_paste, active_data->maxlen);
			else
				BLI_strncpy(active_data->str, buf_paste, active_data->maxlen);

			if (ELEM(but->type, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
				/* else uiSearchboxData.active member is not updated [#26856] */
				but->changed = true;
				ui_searchbox_update(C, data->searchbox, but, true);
			}
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}
	/* colorband (not supported by system clipboard) */
	else if (but->type == BUT_COLORBAND) {
		if (mode == 'c') {
			if (but->poin != NULL) {
				memcpy(&but_copypaste_coba, but->poin, sizeof(ColorBand));
			}
		}
		else {
			if (but_copypaste_coba.tot != 0) {
				if (!but->poin)
					but->poin = MEM_callocN(sizeof(ColorBand), "colorband");

				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				memcpy(data->coba, &but_copypaste_coba, sizeof(ColorBand));
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}
	else if (but->type == BUT_CURVE) {
		if (mode == 'c') {
			if (but->poin != NULL) {
				but_copypaste_curve_alive = true;
				curvemapping_free_data(&but_copypaste_curve);
				curvemapping_copy_data(&but_copypaste_curve, (CurveMapping *) but->poin);
			}
		}
		else {
			if (but_copypaste_curve_alive) {
				if (!but->poin)
					but->poin = MEM_callocN(sizeof(CurveMapping), "curvemapping");

				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				curvemapping_free_data((CurveMapping *) but->poin);
				curvemapping_copy_data((CurveMapping *) but->poin, &but_copypaste_curve);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}
	/* operator button (any type) */
	else if (but->optype) {
		if (mode == 'c') {
			PointerRNA *opptr;
			char *str;
			opptr = uiButGetOperatorPtrRNA(but); /* allocated when needed, the button owns it */

			str = WM_operator_pystring_ex(C, NULL, false, true, but->optype, opptr);

			WM_clipboard_text_set(str, 0);

			MEM_freeN(str);
		}
	}
	/* menu (any type) */
	else if (ELEM(but->type, MENU, PULLDOWN)) {
		MenuType *mt = uiButGetMenuType(but);
		if (mt) {
			char str[32 + sizeof(mt->idname)];
			BLI_snprintf(str, sizeof(str), "bpy.ops.wm.call_menu(name=\"%s\")", mt->idname);
			WM_clipboard_text_set(str, 0);
		}
	}

	if (buf_paste_alloc) {
		MEM_freeN((void *)buf_paste);
	}
}

/* ************************ password text ******************************
 *
 * Functions to convert password strings that should not be displayed
 * to asterisk representation (e.g. mysecretpasswd -> *************)
 *
 * It converts every UTF-8 character to an asterisk, and also remaps
 * the cursor position and selection start/end.
 *
 * Note: remaping is used, because password could contain UTF-8 characters.
 *
 */

static int ui_text_position_from_hidden(uiBut *but, int pos)
{
	const char *strpos;
	int i;

	for (i = 0, strpos = but->drawstr; i < pos; i++)
		strpos = BLI_str_find_next_char_utf8(strpos, NULL);
	
	return (strpos - but->drawstr);
}

static int ui_text_position_to_hidden(uiBut *but, int pos)
{
	return BLI_strnlen_utf8(but->drawstr, pos);
}

void ui_button_text_password_hide(char password_str[UI_MAX_DRAW_STR], uiBut *but, const bool restore)
{
	if (!(but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_PASSWORD))
		return;

	if (restore) {
		/* restore original string */
		BLI_strncpy(but->drawstr, password_str, UI_MAX_DRAW_STR);

		/* remap cursor positions */
		if (but->pos >= 0) {
			but->pos = ui_text_position_from_hidden(but, but->pos);
			but->selsta = ui_text_position_from_hidden(but, but->selsta);
			but->selend = ui_text_position_from_hidden(but, but->selend);
		}
	}
	else {
		/* convert text to hidden test using asterisks (e.g. pass -> ****) */
		const size_t len = BLI_strlen_utf8(but->drawstr);

		/* remap cursor positions */
		if (but->pos >= 0) {
			but->pos = ui_text_position_to_hidden(but, but->pos);
			but->selsta = ui_text_position_to_hidden(but, but->selsta);
			but->selend = ui_text_position_to_hidden(but, but->selend);
		}

		/* save original string */
		BLI_strncpy(password_str, but->drawstr, UI_MAX_DRAW_STR);

		memset(but->drawstr, '*', len);
		but->drawstr[len] = '\0';
	}
}


/* ************* in-button text selection/editing ************* */


static bool ui_textedit_delete_selection(uiBut *but, uiHandleButtonData *data)
{
	char *str = data->str;
	const int len = strlen(str);
	bool changed = false;
	if (but->selsta != but->selend && len) {
		memmove(str + but->selsta, str + but->selend, (len - but->selend) + 1);
		changed = true;
	}
	
	but->pos = but->selend = but->selsta;
	return changed;
}

/**
 * \param x  Screen space cursor location - #wmEvent.x
 *
 * \note ``but->block->aspect`` is used here, so drawing button style is getting scaled too.
 */
static void ui_textedit_set_cursor_pos(uiBut *but, uiHandleButtonData *data, const float x)
{
	uiStyle *style = UI_GetStyle();  // XXX pass on as arg
	uiFontStyle *fstyle = &style->widget;
	const float aspect = but->block->aspect;
	const short fstyle_points_prev = fstyle->points;

	float startx = but->rect.xmin;
	float starty_dummy = 0.0f;
	char *origstr, password_str[UI_MAX_DRAW_STR];

	ui_block_to_window_fl(data->region, but->block, &startx, &starty_dummy);

	ui_fontscale(&fstyle->points, aspect);

	uiStyleFontSet(fstyle);

	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	
	ui_button_text_password_hide(password_str, but, false);

	origstr = MEM_mallocN(sizeof(char) * data->maxlen, "ui_textedit origstr");

	BLI_strncpy(origstr, but->editstr, data->maxlen);

	if (ELEM(but->type, TEX, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
		if (but->flag & UI_HAS_ICON) {
			startx += UI_DPI_ICON_SIZE / aspect;
		}
	}
	/* but this extra .05 makes clicks inbetween characters feel nicer */
	startx += ((UI_TEXT_MARGIN_X + 0.05f) * U.widget_unit) / aspect;
	
	/* mouse dragged outside the widget to the left */
	if (x < startx) {
		int i = but->ofs;

		origstr[but->ofs] = '\0';
		
		while (i > 0) {
			if (BLI_str_cursor_step_prev_utf8(origstr, but->ofs, &i)) {
				/* 0.25 == scale factor for less sensitivity */
				if (BLF_width(fstyle->uifont_id, origstr + i, BLF_DRAW_STR_DUMMY_MAX) > (startx - x) * 0.25f) {
					break;
				}
			}
			else {
				break; /* unlikely but possible */
			}
		}
		but->ofs = i;
		but->pos = but->ofs;
	}
	/* mouse inside the widget, mouse coords mapped in widget space */
	else {  /* (x >= startx) */
		int pos_i;

		/* keep track of previous distance from the cursor to the char */
		float cdist, cdist_prev = 0.0f;
		short pos_prev;
		
		but->pos = pos_prev = strlen(origstr) - but->ofs;

		while (true) {
			cdist = startx + BLF_width(fstyle->uifont_id, origstr + but->ofs, BLF_DRAW_STR_DUMMY_MAX);

			/* check if position is found */
			if (cdist < x) {
				/* check is previous location was in fact closer */
				if ((x - cdist) > (cdist_prev - x)) {
					but->pos = pos_prev;
				}
				break;
			}
			cdist_prev = cdist;
			pos_prev   = but->pos;
			/* done with tricky distance checks */

			pos_i = but->pos;
			if (but->pos <= 0) break;
			if (BLI_str_cursor_step_prev_utf8(origstr, but->ofs, &pos_i)) {
				but->pos = pos_i;
				origstr[but->pos + but->ofs] = 0;
			}
			else {
				break; /* unlikely but possible */
			}
		}
		but->pos += but->ofs;
		if (but->pos < 0) but->pos = 0;
	}
	
	if (fstyle->kerning == 1)
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	
	ui_button_text_password_hide(password_str, but, true);

	MEM_freeN(origstr);

	fstyle->points = fstyle_points_prev;
}

static void ui_textedit_set_cursor_select(uiBut *but, uiHandleButtonData *data, const float x)
{
	if      (x > data->selstartx) data->selextend = EXTEND_RIGHT;
	else if (x < data->selstartx) data->selextend = EXTEND_LEFT;

	ui_textedit_set_cursor_pos(but, data, x);

	if      (data->selextend == EXTEND_RIGHT) but->selend = but->pos;
	else if (data->selextend == EXTEND_LEFT)  but->selsta = but->pos;

	ui_check_but(but);
}

/* this is used for both utf8 and ascii, its meant to be used for single keys,
 * notice the buffer is either copied or not, so its not suitable for pasting in
 * - campbell */
static bool ui_textedit_type_buf(uiBut *but, uiHandleButtonData *data,
                                 const char *utf8_buf, int utf8_buf_len)
{
	char *str;
	int len;
	bool changed = false;

	str = data->str;
	len = strlen(str);

	if (len - (but->selend - but->selsta) + 1 <= data->maxlen) {
		int step = utf8_buf_len;

		/* type over the current selection */
		if ((but->selend - but->selsta) > 0) {
			changed = ui_textedit_delete_selection(but, data);
			len = strlen(str);
		}

		if (len + step < data->maxlen) {
			memmove(&str[but->pos + step], &str[but->pos], (len + 1) - but->pos);
			memcpy(&str[but->pos], utf8_buf, step * sizeof(char));
			but->pos += step;
			changed = true;
		}
	}

	return changed;
}

static bool ui_textedit_type_ascii(uiBut *but, uiHandleButtonData *data, char ascii)
{
	char buf[2] = {ascii, '\0'};

	if (ui_is_but_utf8(but) && (BLI_str_utf8_size(buf) == -1)) {
		printf("%s: entering invalid ascii char into an ascii key (%d)\n",
		       __func__, (int)(unsigned char)ascii);

		return false;
	}

	/* in some cases we want to allow invalid utf8 chars */
	return ui_textedit_type_buf(but, data, buf, 1);
}

static void ui_textedit_move(uiBut *but, uiHandleButtonData *data, strCursorJumpDirection direction,
                             const bool select, strCursorJumpType jump)
{
	const char *str = data->str;
	const int len = strlen(str);
	const int pos_prev = but->pos;
	const bool has_sel = (but->selend - but->selsta) > 0;

	ui_check_but(but);

	/* special case, quit selection and set cursor */
	if (has_sel && !select) {
		if (jump == STRCUR_JUMP_ALL) {
			but->selsta = but->selend = but->pos = direction ? len : 0;
		}
		else {
			if (direction) {
				but->selsta = but->pos = but->selend;
			}
			else {
				but->pos = but->selend = but->selsta;
			}
		}
		data->selextend = 0;
	}
	else {
		int pos_i = but->pos;
		BLI_str_cursor_step_utf8(str, len, &pos_i, direction, jump, true);
		but->pos = pos_i;

		if (select) {
			/* existing selection */
			if (has_sel) {

				if (data->selextend == 0) {
					data->selextend = EXTEND_RIGHT;
				}

				if (direction) {
					if (data->selextend == EXTEND_RIGHT) {
						but->selend = but->pos;
					}
					else {
						but->selsta = but->pos;
					}
				}
				else {
					if (data->selextend == EXTEND_LEFT) {
						but->selsta = but->pos;
					}
					else {
						but->selend = but->pos;
					}
				}

				if (but->selend < but->selsta) {
					SWAP(short, but->selsta, but->selend);
					data->selextend = (data->selextend == EXTEND_RIGHT) ? EXTEND_LEFT : EXTEND_RIGHT;
				}

			} /* new selection */
			else {
				if (direction) {
					data->selextend = EXTEND_RIGHT;
					but->selend = but->pos;
					but->selsta = pos_prev;
				}
				else {
					data->selextend = EXTEND_LEFT;
					but->selend = pos_prev;
					but->selsta = but->pos;
				}
			}
		}
	}
}

static bool ui_textedit_delete(uiBut *but, uiHandleButtonData *data, int direction, strCursorJumpType jump)
{
	char *str = data->str;
	const int len = strlen(str);

	bool changed = false;

	if (jump == STRCUR_JUMP_ALL) {
		if (len) changed = true;
		str[0] = '\0';
		but->pos = 0;
	}
	else if (direction) { /* delete */
		if ((but->selend - but->selsta) > 0) {
			changed = ui_textedit_delete_selection(but, data);
		}
		else if (but->pos >= 0 && but->pos < len) {
			int pos = but->pos;
			int step;
			BLI_str_cursor_step_utf8(str, len, &pos, direction, jump, true);
			step = pos - but->pos;
			memmove(&str[but->pos], &str[but->pos + step], (len + 1) - (but->pos + step));
			changed = true;
		}
	}
	else { /* backspace */
		if (len != 0) {
			if ((but->selend - but->selsta) > 0) {
				changed = ui_textedit_delete_selection(but, data);
			}
			else if (but->pos > 0) {
				int pos = but->pos;
				int step;

				BLI_str_cursor_step_utf8(str, len, &pos, direction, jump, true);
				step = but->pos - pos;
				memmove(&str[but->pos - step], &str[but->pos], (len + 1) - but->pos);
				but->pos -= step;
				changed = true;
			}
		}
	}

	return changed;
}

static int ui_textedit_autocomplete(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	char *str;
	int changed;

	str = data->str;

	if (data->searchbox)
		changed = ui_searchbox_autocomplete(C, data->searchbox, but, data->str);
	else
		changed = but->autocomplete_func(C, str, but->autofunc_arg);

	but->pos = strlen(str);
	but->selsta = but->selend = but->pos;

	return changed;
}

/* mode for ui_textedit_copypaste() */
enum {
	UI_TEXTEDIT_PASTE = 1,
	UI_TEXTEDIT_COPY,
	UI_TEXTEDIT_CUT
};

static bool ui_textedit_copypaste(uiBut *but, uiHandleButtonData *data, const int mode)
{
	char *str, *pbuf;
	int x;
	bool changed = false;
	int str_len, buf_len;

	str = data->str;
	str_len = strlen(str);
	
	/* paste */
	if (mode == UI_TEXTEDIT_PASTE) {
		/* TODO, ensure UTF8 ui_is_but_utf8() - campbell */
		/* extract the first line from the clipboard */
		pbuf = WM_clipboard_text_get_firstline(false, &buf_len);

		if (pbuf) {
			char buf[UI_MAX_DRAW_STR] = {0};
			unsigned int y;

			buf_len = BLI_strncpy_rlen(buf, pbuf, sizeof(buf));

			/* paste over the current selection */
			if ((but->selend - but->selsta) > 0) {
				ui_textedit_delete_selection(but, data);
				str_len = strlen(str);
			}
			
			for (y = 0; y < buf_len; y++) {
				/* add contents of buffer */
				if (str_len + 1 < data->maxlen) {
					for (x = data->maxlen; x > but->pos; x--)
						str[x] = str[x - 1];
					str[but->pos] = buf[y];
					but->pos++; 
					str_len++;
					str[str_len] = '\0';
				}
			}

			changed = true;
		}

		if (pbuf) {
			MEM_freeN(pbuf);
		}
	}
	/* cut & copy */
	else if (ELEM(mode, UI_TEXTEDIT_COPY, UI_TEXTEDIT_CUT)) {
		/* copy the contents to the copypaste buffer */
		int sellen = but->selend - but->selsta;
		char *buf = MEM_mallocN(sizeof(char) * (sellen + 1), "ui_textedit_copypaste");

		BLI_strncpy(buf, str + but->selsta, sellen + 1);
		WM_clipboard_text_set(buf, 0);
		MEM_freeN(buf);
		
		/* for cut only, delete the selection afterwards */
		if (mode == UI_TEXTEDIT_CUT) {
			if ((but->selend - but->selsta) > 0) {
				changed = ui_textedit_delete_selection(but, data);
			}
		}
	}

	return changed;
}

static void ui_textedit_begin(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	int len;

	if (data->str) {
		MEM_freeN(data->str);
		data->str = NULL;
	}

#ifdef USE_DRAG_MULTINUM
	/* this can happen from multi-drag */
	if (data->applied_interactive) {
		/* remove any small changes so canceling edit doesn't restore invalid value: T40538 */
		data->cancel = true;
		ui_apply_button(C, but->block, but, data, true);
		data->cancel = false;

		data->applied_interactive = false;
	}
#endif

	/* retrieve string */
	data->maxlen = ui_get_but_string_max_length(but);
	data->str = MEM_callocN(sizeof(char) * data->maxlen + 1, "textedit str");
	ui_get_but_string(but, data->str, data->maxlen);

	if (ui_is_but_float(but) && !ui_is_but_unit(but)) {
		BLI_str_rstrip_float_zero(data->str, '\0');
	}

	if (ELEM(but->type, NUM, NUMSLI)) {
		ui_convert_to_unit_alt_name(but, data->str, data->maxlen);
	}

	/* won't change from now on */
	len = strlen(data->str);

	data->origstr = BLI_strdupn(data->str, len);
	data->selextend = 0;
	data->selstartx = 0.0f;

	/* set cursor pos to the end of the text */
	but->editstr = data->str;
	but->pos = len;
	but->selsta = 0;
	but->selend = len;

	/* optional searchbox */
	if (ELEM(but->type, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
		data->searchbox = ui_searchbox_create(C, data->region, but);
		ui_searchbox_update(C, data->searchbox, but, true); /* true = reset */
	}

	/* reset alert flag (avoid confusion, will refresh on exit) */
	but->flag &= ~UI_BUT_REDALERT;

	ui_check_but(but);
	
	WM_cursor_modal_set(CTX_wm_window(C), BC_TEXTEDITCURSOR);
}

static void ui_textedit_end(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	if (but) {
		if (ui_is_but_utf8(but)) {
			int strip = BLI_utf8_invalid_strip(but->editstr, strlen(but->editstr));
			/* not a file?, strip non utf-8 chars */
			if (strip) {
				/* wont happen often so isn't that annoying to keep it here for a while */
				printf("%s: invalid utf8 - stripped chars %d\n", __func__, strip);
			}
		}
		
		if (data->searchbox) {
			if (data->cancel == false) {
				if ((ui_searchbox_apply(but, data->searchbox) == false) &&
				    (ui_searchbox_find_index(data->searchbox, but->editstr) == -1))
				{
					data->cancel = true;
				}
			}

			ui_searchbox_free(C, data->searchbox);
			data->searchbox = NULL;
		}
		
		but->editstr = NULL;
		but->pos = -1;
	}
	
	WM_cursor_modal_restore(CTX_wm_window(C));
}

static void ui_textedit_next_but(uiBlock *block, uiBut *actbut, uiHandleButtonData *data)
{
	uiBut *but;

	/* label and roundbox can overlap real buttons (backdrops...) */
	if (ELEM(actbut->type, LABEL, SEPR, SEPRLINE, ROUNDBOX, LISTBOX))
		return;

	for (but = actbut->next; but; but = but->next) {
		if (ELEM(but->type, TEX, NUM, NUMSLI, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
			if (!(but->flag & UI_BUT_DISABLED)) {
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
				return;
			}
		}
	}
	for (but = block->buttons.first; but != actbut; but = but->next) {
		if (ELEM(but->type, TEX, NUM, NUMSLI, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
			if (!(but->flag & UI_BUT_DISABLED)) {
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
				return;
			}
		}
	}
}

static void ui_textedit_prev_but(uiBlock *block, uiBut *actbut, uiHandleButtonData *data)
{
	uiBut *but;

	/* label and roundbox can overlap real buttons (backdrops...) */
	if (ELEM(actbut->type, LABEL, SEPR, SEPRLINE, ROUNDBOX, LISTBOX))
		return;

	for (but = actbut->prev; but; but = but->prev) {
		if (ELEM(but->type, TEX, NUM, NUMSLI, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
			if (!(but->flag & UI_BUT_DISABLED)) {
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
				return;
			}
		}
	}
	for (but = block->buttons.last; but != actbut; but = but->prev) {
		if (ELEM(but->type, TEX, NUM, NUMSLI, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
			if (!(but->flag & UI_BUT_DISABLED)) {
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
				return;
			}
		}
	}
}


static void ui_do_but_textedit(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int retval = WM_UI_HANDLER_CONTINUE;
	bool changed = false, inbox = false, update = false;

	switch (event->type) {
		case MOUSEMOVE:
		case MOUSEPAN:
			if (data->searchbox) {
#ifdef USE_KEYNAV_LIMIT
				if ((event->type == MOUSEMOVE) && ui_mouse_motion_keynav_test(&data->searchbox_keynav_state, event)) {
					/* pass */
				}
				else {
					ui_searchbox_event(C, data->searchbox, but, event);
				}
#else
				ui_searchbox_event(C, data->searchbox, but, event);
#endif
			}
			
			break;
		case RIGHTMOUSE:
		case ESCKEY:
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_BREAK;
			}
			break;
		case LEFTMOUSE:
		{
			bool had_selection = but->selsta != but->selend;
			
			/* exit on LMB only on RELEASE for searchbox, to mimic other popups, and allow multiple menu levels */
			if (data->searchbox)
				inbox = ui_searchbox_inside(data->searchbox, event->x, event->y);

			/* for double click: we do a press again for when you first click on button (selects all text, no cursor pos) */
			if (event->val == KM_PRESS || event->val == KM_DBL_CLICK) {
				float mx, my;

				mx = event->x;
				my = event->y;
				ui_window_to_block_fl(data->region, block, &mx, &my);

				if (ui_but_contains_pt(but, mx, my)) {
					ui_textedit_set_cursor_pos(but, data, event->x);
					but->selsta = but->selend = but->pos;
					data->selstartx = event->x;

					button_activate_state(C, but, BUTTON_STATE_TEXT_SELECTING);
					retval = WM_UI_HANDLER_BREAK;
				}
				else if (inbox == false) {
					/* if searchbox, click outside will cancel */
					if (data->searchbox)
						data->cancel = data->escapecancel = true;
					button_activate_state(C, but, BUTTON_STATE_EXIT);
					retval = WM_UI_HANDLER_BREAK;
				}
			}
			
			/* only select a word in button if there was no selection before */
			if (event->val == KM_DBL_CLICK && had_selection == false) {
				ui_textedit_move(but, data, STRCUR_DIR_PREV, false, STRCUR_JUMP_DELIM);
				ui_textedit_move(but, data, STRCUR_DIR_NEXT, true, STRCUR_JUMP_DELIM);
				retval = WM_UI_HANDLER_BREAK;
				changed = true;
			}
			else if (inbox) {
				/* if we allow activation on key press, it gives problems launching operators [#35713] */
				if (event->val == KM_RELEASE) {
					button_activate_state(C, but, BUTTON_STATE_EXIT);
					retval = WM_UI_HANDLER_BREAK;
				}
			}
			break;
		}
	}

	if (event->val == KM_PRESS) {
		switch (event->type) {
			case VKEY:
			case XKEY:
			case CKEY:
				if (event->ctrl || event->oskey) {
					if (event->type == VKEY)
						changed = ui_textedit_copypaste(but, data, UI_TEXTEDIT_PASTE);
					else if (event->type == CKEY)
						changed = ui_textedit_copypaste(but, data, UI_TEXTEDIT_COPY);
					else if (event->type == XKEY)
						changed = ui_textedit_copypaste(but, data, UI_TEXTEDIT_CUT);

					retval = WM_UI_HANDLER_BREAK;
				}
				break;
			case RIGHTARROWKEY:
				ui_textedit_move(but, data, STRCUR_DIR_NEXT,
				                 event->shift != 0, event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case LEFTARROWKEY:
				ui_textedit_move(but, data, STRCUR_DIR_PREV,
				                 event->shift != 0, event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case WHEELDOWNMOUSE:
			case DOWNARROWKEY:
				if (data->searchbox) {
#ifdef USE_KEYNAV_LIMIT
					ui_mouse_motion_keynav_init(&data->searchbox_keynav_state, event);
#endif
					ui_searchbox_event(C, data->searchbox, but, event);
					break;
				}
				/* fall-through */
			case ENDKEY:
				ui_textedit_move(but, data, STRCUR_DIR_NEXT,
				                 event->shift != 0, STRCUR_JUMP_ALL);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case WHEELUPMOUSE:
			case UPARROWKEY:
				if (data->searchbox) {
#ifdef USE_KEYNAV_LIMIT
					ui_mouse_motion_keynav_init(&data->searchbox_keynav_state, event);
#endif
					ui_searchbox_event(C, data->searchbox, but, event);
					break;
				}
				/* fall-through */
			case HOMEKEY:
				ui_textedit_move(but, data, STRCUR_DIR_PREV,
				                 event->shift != 0, STRCUR_JUMP_ALL);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case PADENTER:
			case RETKEY:
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case DELKEY:
				changed = ui_textedit_delete(but, data, 1,
				                             event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
				retval = WM_UI_HANDLER_BREAK;
				break;

			case BACKSPACEKEY:
				changed = ui_textedit_delete(but, data, 0,
				                             event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
				retval = WM_UI_HANDLER_BREAK;
				break;
				
			case AKEY:

				/* Ctrl + A: Select all */
#if defined(__APPLE__)
				/* OSX uses cmd-a systemwide, so add it */
				if ((event->oskey && !(event->alt || event->shift || event->ctrl)) ||
				    (event->ctrl  && !(event->alt || event->shift || event->oskey)))
#else
				if (event->ctrl && !(event->alt || event->shift || event->oskey))
#endif
				{
					ui_textedit_move(but, data, STRCUR_DIR_PREV,
					                 false, STRCUR_JUMP_ALL);
					ui_textedit_move(but, data, STRCUR_DIR_NEXT,
					                 true, STRCUR_JUMP_ALL);
					retval = WM_UI_HANDLER_BREAK;
				}
				break;

			case TABKEY:
				/* there is a key conflict here, we can't tab with autocomplete */
				if (but->autocomplete_func || data->searchbox) {
					int autocomplete = ui_textedit_autocomplete(C, but, data);
					changed = autocomplete != AUTOCOMPLETE_NO_MATCH;

					if (autocomplete == AUTOCOMPLETE_FULL_MATCH)
						button_activate_state(C, but, BUTTON_STATE_EXIT);

					update = true;  /* do live update for tab key */
				}
				/* the hotkey here is not well defined, was G.qual so we check all */
				else if (event->shift || event->ctrl || event->alt || event->oskey) {
					ui_textedit_prev_but(block, but, data);
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				else {
					ui_textedit_next_but(block, but, data);
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				retval = WM_UI_HANDLER_BREAK;
				break;
		}

		if ((event->ascii || event->utf8_buf[0]) && (retval == WM_UI_HANDLER_CONTINUE)) {
			char ascii = event->ascii;
			const char *utf8_buf = event->utf8_buf;

			/* exception that's useful for number buttons, some keyboard
			 * numpads have a comma instead of a period */
			if (ELEM(but->type, NUM, NUMSLI)) { /* could use data->min*/
				if (event->type == PADPERIOD && ascii == ',') {
					ascii = '.';
					utf8_buf = NULL; /* force ascii fallback */
				}
			}

			if (utf8_buf && utf8_buf[0]) {
				int utf8_buf_len = BLI_str_utf8_size(utf8_buf);
				/* keep this printf until utf8 is well tested */
				if (utf8_buf_len != 1) {
					printf("%s: utf8 char '%.*s'\n", __func__, utf8_buf_len, utf8_buf);
				}

				// strcpy(utf8_buf, "12345");
				changed = ui_textedit_type_buf(but, data, event->utf8_buf, utf8_buf_len);
			}
			else {
				changed = ui_textedit_type_ascii(but, data, ascii);
			}

			retval = WM_UI_HANDLER_BREAK;
			
		}
		/* textbutton with magnifier icon: do live update for search button */
		if (but->icon == ICON_VIEWZOOM)
			update = true;
	}

	if (changed) {
		/* only update when typing for TAB key */
		if (update && data->interactive) {
			ui_apply_button(C, block, but, data, true);
		}
		else {
			ui_check_but(but);
		}
		but->changed = true;
		
		if (data->searchbox)
			ui_searchbox_update(C, data->searchbox, but, true);  /* true = reset */
	}

	if (changed || (retval == WM_UI_HANDLER_BREAK))
		ED_region_tag_redraw(data->region);
}

static void ui_do_but_textedit_select(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my, retval = WM_UI_HANDLER_CONTINUE;

	switch (event->type) {
		case MOUSEMOVE:
		{
			mx = event->x;
			my = event->y;
			ui_window_to_block(data->region, block, &mx, &my);

			ui_textedit_set_cursor_select(but, data, event->x);
			retval = WM_UI_HANDLER_BREAK;
			break;
		}
		case LEFTMOUSE:
			if (event->val == KM_RELEASE)
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			retval = WM_UI_HANDLER_BREAK;
			break;
	}

	if (retval == WM_UI_HANDLER_BREAK) {
		ui_check_but(but);
		ED_region_tag_redraw(data->region);
	}
}

/* ************* number editing for various types ************* */

static void ui_numedit_begin(uiBut *but, uiHandleButtonData *data)
{
	if (but->type == BUT_CURVE) {
		but->editcumap = (CurveMapping *)but->poin;
	}
	else if (but->type == BUT_COLORBAND) {
		data->coba = (ColorBand *)but->poin;
		but->editcoba = data->coba;
	}
	else if (ELEM(but->type, BUT_NORMAL, HSVCUBE, HSVCIRCLE, COLOR)) {
		ui_get_but_vectorf(but, data->origvec);
		copy_v3_v3(data->vec, data->origvec);
		but->editvec = data->vec;
	}
	else {
		float softrange, softmin, softmax;

		data->startvalue = ui_get_but_val(but);
		data->origvalue = data->startvalue;
		data->value = data->origvalue;
		but->editval = &data->value;

		softmin = but->softmin;
		softmax = but->softmax;
		softrange = softmax - softmin;

		data->dragfstart = (softrange == 0.0f) ? 0.0f : ((float)data->value - softmin) / softrange;
		data->dragf = data->dragfstart;
	}

	data->dragchange = false;
	data->draglock = true;
}

static void ui_numedit_end(uiBut *but, uiHandleButtonData *data)
{
	but->editval = NULL;
	but->editvec = NULL;
	but->editcoba = NULL;
	but->editcumap = NULL;

	data->dragstartx = 0;
	data->draglastx = 0;
	data->dragchange = false;
	data->dragcbd = NULL;
	data->dragsel = 0;
}

static void ui_numedit_apply(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data)
{
	if (data->interactive) {
		ui_apply_button(C, block, but, data, true);
	}
	else {
		ui_check_but(but);
	}

	ED_region_tag_redraw(data->region);
}

/* ****************** menu opening for various types **************** */

static void ui_blockopen_begin(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	uiBlockCreateFunc func = NULL;
	uiBlockHandleCreateFunc handlefunc = NULL;
	uiMenuCreateFunc menufunc = NULL;
	void *arg = NULL;

	switch (but->type) {
		case BLOCK:
		case PULLDOWN:
			if (but->menu_create_func) {
				menufunc = but->menu_create_func;
				arg = but->poin;
			}
			else {
				func = but->block_create_func;
				arg = but->poin ? but->poin : but->func_argN;
			}
			break;
		case MENU:
			BLI_assert(but->menu_create_func);
			menufunc = but->menu_create_func;
			arg = but->poin;
			break;
		case COLOR:
			ui_get_but_vectorf(but, data->origvec);
			copy_v3_v3(data->vec, data->origvec);
			but->editvec = data->vec;

			handlefunc = ui_block_func_COLOR;
			arg = but;
			break;

			/* quiet warnings for unhandled types */
		default:
			break;
	}

	if (func || handlefunc) {
		data->menu = ui_popup_block_create(C, data->region, but, func, handlefunc, arg);
		if (but->block->handle)
			data->menu->popup = but->block->handle->popup;
	}
	else if (menufunc) {
		data->menu = ui_popup_menu_create(C, data->region, but, menufunc, arg);
		if (but->block->handle)
			data->menu->popup = but->block->handle->popup;
	}

	/* this makes adjacent blocks auto open from now on */
	//if (but->block->auto_open == 0) but->block->auto_open = 1;
}

static void ui_blockopen_end(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	if (but) {
		but->editval = NULL;
		but->editvec = NULL;

		but->block->auto_open_last = PIL_check_seconds_timer();
	}

	if (data->menu) {
		ui_popup_block_free(C, data->menu);
		data->menu = NULL;
	}
}

int ui_button_open_menu_direction(uiBut *but)
{
	uiHandleButtonData *data = but->active;

	if (data && data->menu)
		return data->menu->direction;
	
	return 0;
}

/* Hack for uiList LISTROW buttons to "give" events to overlaying TEX buttons (cltr-clic rename feature & co). */
static uiBut *ui_but_list_row_text_activate(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event,
                                            uiButtonActivateType activate_type)
{
	ARegion *ar = CTX_wm_region(C);
	uiBut *labelbut = ui_but_find_mouse_over_ex(ar, event->x, event->y, true);

	if (labelbut && labelbut->type == TEX && !(labelbut->flag & UI_BUT_DISABLED)) {
		/* exit listrow */
		data->cancel = true;
		button_activate_exit(C, but, data, false, false);

		/* Activate the text button. */
		button_activate_init(C, ar, labelbut, activate_type);

		return labelbut;
	}
	return NULL;
}

/* ***************** events for different button types *************** */

static int ui_do_but_BUT(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_WAIT_RELEASE);
			return WM_UI_HANDLER_BREAK;
		}
		else if (event->type == LEFTMOUSE && but->block->handle) {
			/* regular buttons will be 'UI_SELECT', menu items 'UI_ACTIVE' */
			if (!(but->flag & (UI_SELECT | UI_ACTIVE)))
				data->cancel = true;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
		else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_WAIT_FLASH);
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_RELEASE) {
		if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			if (!(but->flag & UI_SELECT))
				data->cancel = true;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_HOTKEYEVT(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
			but->drawstr[0] = 0;
			but->modifier_key = 0;
			button_activate_state(C, but, BUTTON_STATE_WAIT_KEY_EVENT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_KEY_EVENT) {
		
		if (event->type == MOUSEMOVE)
			return WM_UI_HANDLER_CONTINUE;
		
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			/* only cancel if click outside the button */
			if (ui_mouse_inside_button(but->active->region, but, event->x, event->y) == 0) {
				/* data->cancel doesnt work, this button opens immediate */
				if (but->flag & UI_BUT_IMMEDIATE)
					ui_set_but_val(but, 0);
				else
					data->cancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				return WM_UI_HANDLER_BREAK;
			}
		}
		
		/* always set */
		but->modifier_key = 0;
		if (event->shift) but->modifier_key |= KM_SHIFT;
		if (event->alt) but->modifier_key |= KM_ALT;
		if (event->ctrl) but->modifier_key |= KM_CTRL;
		if (event->oskey) but->modifier_key |= KM_OSKEY;

		ui_check_but(but);
		ED_region_tag_redraw(data->region);
			
		if (event->val == KM_PRESS) {
			if (ISHOTKEY(event->type)) {
				
				if (WM_key_event_string(event->type)[0])
					ui_set_but_val(but, event->type);
				else
					data->cancel = true;
				
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				return WM_UI_HANDLER_BREAK;
			}
			else if (event->type == ESCKEY) {
				if (event->val == KM_PRESS) {
					data->cancel = true;
					data->escapecancel = true;
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
			}
			
		}
	}
	
	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_KEYEVT(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_WAIT_KEY_EVENT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_KEY_EVENT) {
		if (event->type == MOUSEMOVE)
			return WM_UI_HANDLER_CONTINUE;

		if (event->val == KM_PRESS) {
			if (WM_key_event_string(event->type)[0])
				ui_set_but_val(but, event->type);
			else
				data->cancel = true;

			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_TEX(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (ELEM(event->type, LEFTMOUSE, EVT_BUT_OPEN, PADENTER, RETKEY) && event->val == KM_PRESS) {
			if (ELEM(event->type, PADENTER, RETKEY) && (!ui_is_but_utf8(but))) {
				/* pass - allow filesel, enter to execute */
			}
			else if (but->dt == UI_EMBOSSN && !event->ctrl) {
				/* pass */
			}
			else {
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
				return WM_UI_HANDLER_BREAK;
			}
		}
	}
	else if (data->state == BUTTON_STATE_TEXT_EDITING) {
		ui_do_but_textedit(C, block, but, data, event);
		return WM_UI_HANDLER_BREAK;
	}
	else if (data->state == BUTTON_STATE_TEXT_SELECTING) {
		ui_do_but_textedit_select(C, block, but, data, event);
		return WM_UI_HANDLER_BREAK;
	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_SEARCH_UNLINK(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	/* unlink icon is on right */
	if (ELEM(event->type, LEFTMOUSE, EVT_BUT_OPEN, PADENTER, RETKEY) && event->val == KM_PRESS &&
	    ui_is_but_search_unlink_visible(but))
	{
		ARegion *ar = data->region;
		rcti rect;
		int x = event->x, y = event->y;
		
		ui_window_to_block(ar, but->block, &x, &y);
		
		BLI_rcti_rctf_copy(&rect, &but->rect);
		
		rect.xmin = rect.xmax - (BLI_rcti_size_y(&rect));
		if (BLI_rcti_isect_pt(&rect, x, y)) {
			/* most likely NULL, but let's check, and give it temp zero string */
			if (data->str == NULL)
				data->str = MEM_callocN(1, "temp str");
			data->str[0] = 0;

			ui_apply_but_TEX(C, but, data);
			button_activate_state(C, but, BUTTON_STATE_EXIT);

			return WM_UI_HANDLER_BREAK;
		}
	}
	return ui_do_but_TEX(C, block, but, data, event);
}

static int ui_do_but_TOG(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
#ifdef USE_DRAG_TOGGLE
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS && ui_is_but_drag_toggle(but)) {
#if 0		/* UNUSED */
			data->togdual = event->ctrl;
			data->togonly = !event->shift;
#endif
			ui_apply_button(C, but->block, but, data, true);
			button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
			data->dragstartx = event->x;
			data->dragstarty = event->y;
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_DRAG) {
		/* note: the 'BUTTON_STATE_WAIT_DRAG' part of 'ui_do_but_EXIT' could be refactored into its own function */
		data->applied = false;
		return ui_do_but_EXIT(C, but, data, event);
	}
#endif
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
#if 0		/* UNUSED */
			data->togdual = event->ctrl;
			data->togonly = !event->shift;
#endif
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_EXIT(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	
	if (data->state == BUTTON_STATE_HIGHLIGHT) {

		/* first handle click on icondrag type button */
		if (event->type == LEFTMOUSE && but->dragpoin) {
			if (ui_but_mouse_inside_icon(but, data->region, event)) {
				
				/* tell the button to wait and keep checking further events to
				 * see if it should start dragging */
				button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
				data->dragstartx = event->x;
				data->dragstarty = event->y;
				return WM_UI_HANDLER_CONTINUE;
			}
		}
#ifdef USE_DRAG_TOGGLE
		if (event->type == LEFTMOUSE && ui_is_but_drag_toggle(but)) {
			button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
			data->dragstartx = event->x;
			data->dragstarty = event->y;
			return WM_UI_HANDLER_CONTINUE;
		}
#endif

		if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
			int ret = WM_UI_HANDLER_BREAK;
			/* XXX (a bit ugly) Special case handling for filebrowser drag button */
			if (but->dragpoin && but->imb && ui_but_mouse_inside_icon(but, data->region, event)) {
				ret = WM_UI_HANDLER_CONTINUE;
			}
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return ret;
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_DRAG) {
		
		/* this function also ends state */
		if (ui_but_start_drag(C, but, data, event)) {
			return WM_UI_HANDLER_BREAK;
		}
		
		/* If the mouse has been pressed and released, getting to 
		 * this point without triggering a drag, then clear the 
		 * drag state for this button and continue to pass on the event */
		if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_CONTINUE;
		}
		
		/* while waiting for a drag to be triggered, always block 
		 * other events from getting handled */
		return WM_UI_HANDLER_BREAK;
	}
	
	return WM_UI_HANDLER_CONTINUE;
}

/* var names match ui_numedit_but_NUM */
static float ui_numedit_apply_snapf(uiBut *but, float tempf, float softmin, float softmax, float softrange,
                                    const enum eSnapType snap)
{
	if (tempf == softmin || tempf == softmax || snap == SNAP_OFF) {
		/* pass */
	}
	else {
		float fac = 1.0f;
		
		if (ui_is_but_unit(but)) {
			UnitSettings *unit = but->block->unit;
			int unit_type = RNA_SUBTYPE_UNIT_VALUE(uiButGetUnitType(but));

			if (bUnit_IsValid(unit->system, unit_type)) {
				fac = (float)bUnit_BaseScalar(unit->system, unit_type);
				if (ELEM(unit_type, B_UNIT_LENGTH, B_UNIT_AREA, B_UNIT_VOLUME)) {
					fac /= unit->scale_length;
				}
			}
		}

		if (fac != 1.0f) {
			/* snap in unit-space */
			tempf /= fac;
			/* softmin /= fac; */ /* UNUSED */
			/* softmax /= fac; */ /* UNUSED */
			softrange /= fac;
		}

		if (snap == SNAP_ON) {
			if      (softrange < 2.10f) tempf = 0.1f  * floorf(10.0f * tempf);
			else if (softrange < 21.0f) tempf = floorf(tempf);
			else                        tempf = 10.0f * floorf(tempf / 10.0f);
		}
		else if (snap == SNAP_ON_SMALL) {
			if      (softrange < 2.10f) tempf = 0.01f * floorf(100.0f * tempf);
			else if (softrange < 21.0f) tempf = 0.1f  * floorf(10.0f * tempf);
			else                        tempf = floor(tempf);
		}
		else {
			BLI_assert(0);
		}
		
		if (fac != 1.0f)
			tempf *= fac;
	}

	return tempf;
}

static float ui_numedit_apply_snap(int temp, float softmin, float softmax,
                                   const enum eSnapType snap)
{
	if (temp == softmin || temp == softmax)
		return temp;

	switch (snap) {
		case SNAP_OFF:
			break;
		case SNAP_ON:
			temp = 10 * (temp / 10);
			break;
		case SNAP_ON_SMALL:
			temp = 100 * (temp / 100);
			break;
	}

	return temp;
}

static bool ui_numedit_but_NUM(uiBut *but, uiHandleButtonData *data,
                               int mx,
                               const enum eSnapType snap, float fac)
{
	float deler, tempf, softmin, softmax, softrange;
	int lvalue, temp;
	bool changed = false;
	const bool is_float = ui_is_but_float(but);
	
	if (mx == data->draglastx)
		return changed;
	
	/* drag-lock - prevent unwanted scroll adjustments */
	/* change value (now 3) to adjust threshold in pixels */
	if (data->draglock) {
		if (abs(mx - data->dragstartx) <= 3)
			return changed;
#ifdef USE_DRAG_MULTINUM
		if (ELEM(data->multi_data.init, BUTTON_MULTI_INIT_UNSET, BUTTON_MULTI_INIT_SETUP)) {
			return changed;
		}
#endif

		data->draglock = false;
		data->dragstartx = mx;  /* ignore mouse movement within drag-lock */
	}

	softmin = but->softmin;
	softmax = but->softmax;
	softrange = softmax - softmin;

	if (ui_is_a_warp_but(but)) {
		/* Mouse location isn't screen clamped to the screen so use a linear mapping
		 * 2px == 1-int, or 1px == 1-ClickStep */
		if (is_float) {
			fac *= 0.01f * but->a1;
			tempf = (float)data->startvalue + ((float)(mx - data->dragstartx) * fac);
			tempf = ui_numedit_apply_snapf(but, tempf, softmin, softmax, softrange, snap);

#if 1       /* fake moving the click start, nicer for dragging back after passing the limit */
			if (tempf < softmin) {
				data->dragstartx -= (softmin - tempf) / fac;
				tempf = softmin;
			}
			else if (tempf > softmax) {
				data->dragstartx += (tempf - softmax) / fac;
				tempf = softmax;
			}
#else
			CLAMP(tempf, softmin, softmax);
#endif

			if (tempf != (float)data->value) {
				data->dragchange = true;
				data->value = tempf;
				changed = true;
			}
		}
		else {
			if      (softrange > 256) fac = 1.0;        /* 1px == 1 */
			else if (softrange >  32) fac = 1.0 / 2.0;  /* 2px == 1 */
			else                      fac = 1.0 / 16.0; /* 16px == 1? */

			temp = data->startvalue + (((double)mx - data->dragstartx) * (double)fac);
			temp = ui_numedit_apply_snap(temp, softmin, softmax, snap);

#if 1       /* fake moving the click start, nicer for dragging back after passing the limit */
			if (temp < softmin) {
				data->dragstartx -= (softmin - temp) / fac;
				temp = softmin;
			}
			else if (temp > softmax) {
				data->dragstartx += (temp - softmax) / fac;
				temp = softmax;
			}
#else
			CLAMP(temp, softmin, softmax);
#endif

			if (temp != data->value) {
				data->dragchange = true;
				data->value = temp;
				changed = true;
			}
		}

		data->draglastx = mx;
	}
	else {
		/* Use a non-linear mapping of the mouse drag especially for large floats (normal behavior) */
		deler = 500;
		if (!is_float) {
			/* prevent large ranges from getting too out of control */
			if      (softrange > 600) deler = powf(softrange, 0.75f);
			else if (softrange <  25) deler = 50.0;
			else if (softrange < 100) deler = 100.0;
		}
		deler /= fac;

		if ((is_float == true) && (softrange > 11)) {
			/* non linear change in mouse input- good for high precicsion */
			data->dragf += (((float)(mx - data->draglastx)) / deler) * ((float)abs(mx - data->dragstartx) / 500.0f);
		}
		else if ((is_float == false) && (softrange > 129)) { /* only scale large int buttons */
			/* non linear change in mouse input- good for high precicsionm ints need less fine tuning */
			data->dragf += (((float)(mx - data->draglastx)) / deler) * ((float)abs(mx - data->dragstartx) / 250.0f);
		}
		else {
			/*no scaling */
			data->dragf += ((float)(mx - data->draglastx)) / deler;
		}
	
		CLAMP(data->dragf, 0.0f, 1.0f);
		data->draglastx = mx;
		tempf = (softmin + data->dragf * softrange);


		if (!is_float) {
			temp = floorf(tempf + 0.5f);

			temp = ui_numedit_apply_snap(temp, softmin, softmax, snap);

			CLAMP(temp, softmin, softmax);
			lvalue = (int)data->value;
			
			if (temp != lvalue) {
				data->dragchange = true;
				data->value = (double)temp;
				changed = true;
			}
		}
		else {
			temp = 0;
			tempf = ui_numedit_apply_snapf(but, tempf, softmin, softmax, softrange, snap);

			CLAMP(tempf, softmin, softmax);

			if (tempf != (float)data->value) {
				data->dragchange = true;
				data->value = tempf;
				changed = true;
			}
		}
	}


	return changed;
}

static int ui_do_but_NUM(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my; /* mouse location scaled to fit the UI */
	int screen_mx, screen_my; /* mouse location kept at screen pixel coords */
	int click = 0;
	int retval = WM_UI_HANDLER_CONTINUE;

	mx = screen_mx = event->x;
	my = screen_my = event->y;

	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		int type = event->type, val = event->val;
		
		if (type == MOUSEPAN) {
			ui_pan_to_scroll(event, &type, &val);
		}
		
		/* XXX hardcoded keymap check.... */
		if (type == MOUSEPAN && event->alt)
			retval = WM_UI_HANDLER_BREAK; /* allow accumulating values, otherwise scrolling gets preference */
		else if (type == WHEELDOWNMOUSE && event->alt) {
			mx = but->rect.xmin;
			click = 1;
		}
		else if (type == WHEELUPMOUSE && event->alt) {
			mx = but->rect.xmax;
			click = 1;
		}
		else if (event->val == KM_PRESS) {
			if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->ctrl) {
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
			else if (event->type == LEFTMOUSE) {
				data->dragstartx = data->draglastx = ui_is_a_warp_but(but) ? screen_mx : mx;
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
			else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS) {
				click = 1;
			}
			else if (event->type == MINUSKEY && event->val == KM_PRESS) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				data->value = -data->value;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_BREAK;
			}

#ifdef USE_DRAG_MULTINUM
			copy_v2_v2_int(data->multi_data.drag_start, &event->x);
#endif
		}
		
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY || event->type == RIGHTMOUSE) {
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			if (data->dragchange) {
#ifdef USE_DRAG_MULTINUM
				/* if we started multibutton but didnt drag, then edit */
				if (data->multi_data.init == BUTTON_MULTI_INIT_SETUP) {
					click = 1;
				}
				else
#endif
				{
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
			}
			else {
				click = 1;
			}
		}
		else if (event->type == MOUSEMOVE) {
			const enum eSnapType snap = ui_event_to_snap(event);
			float fac;

#ifdef USE_DRAG_MULTINUM
			data->multi_data.drag_dir[0] += abs(data->draglastx - mx);
			data->multi_data.drag_dir[1] += abs(data->draglasty - my);
#endif

			fac = 1.0f;
			if (event->shift) fac /= 10.0f;
			if (event->alt)   fac /= 20.0f;

			if (ui_numedit_but_NUM(but, data, (ui_is_a_warp_but(but) ? screen_mx : mx), snap, fac))
				ui_numedit_apply(C, block, but, data);
#ifdef USE_DRAG_MULTINUM
			else if (data->multi_data.has_mbuts) {
				if (data->multi_data.init == BUTTON_MULTI_INIT_ENABLE) {
					ui_multibut_states_apply(C, data, block);
				}
			}
#endif
		}
		retval = WM_UI_HANDLER_BREAK;
	}
	else if (data->state == BUTTON_STATE_TEXT_EDITING) {
		ui_do_but_textedit(C, block, but, data, event);
		retval = WM_UI_HANDLER_BREAK;
	}
	else if (data->state == BUTTON_STATE_TEXT_SELECTING) {
		ui_do_but_textedit_select(C, block, but, data, event);
		retval = WM_UI_HANDLER_BREAK;
	}
	
	if (click) {
		/* we can click on the side arrows to increment/decrement,
		 * or click inside to edit the value directly */
		float tempf, softmin, softmax;
		float handlewidth;
		int temp;

		softmin = but->softmin;
		softmax = but->softmax;

		handlewidth = min_ff(BLI_rctf_size_x(&but->rect) / 3, BLI_rctf_size_y(&but->rect));

		if (!ui_is_but_float(but)) {
			if (mx < (but->rect.xmin + handlewidth)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				temp = (int)data->value - 1;
				if (temp >= softmin && temp <= softmax)
					data->value = (double)temp;
				else
					data->cancel = true;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else if (mx > (but->rect.xmax - handlewidth)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				temp = (int)data->value + 1;
				if (temp >= softmin && temp <= softmax)
					data->value = (double)temp;
				else
					data->cancel = true;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else {
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			}
		}
		else {
			if (mx < (but->rect.xmin + handlewidth)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				tempf = (float)data->value - 0.01f * but->a1;
				if (tempf < softmin) tempf = softmin;
				data->value = tempf;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else if (mx > but->rect.xmax - handlewidth) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				tempf = (float)data->value + 0.01f * but->a1;
				if (tempf > softmax) tempf = softmax;
				data->value = tempf;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else {
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			}
		}

		retval = WM_UI_HANDLER_BREAK;
	}
	
	data->draglastx = mx;
	data->draglasty = my;

	return retval;
}

static bool ui_numedit_but_SLI(uiBut *but, uiHandleButtonData *data,
                               int mx, const bool is_horizontal,
                               const bool snap, const bool shift)
{
	float deler, f, tempf, softmin, softmax, softrange;
	int temp, lvalue;
	bool changed = false;
	float mx_fl, my_fl;
	/* note, 'offs' is really from the widget drawing rounded corners see 'widget_numslider' */
	float offs;

	softmin = but->softmin;
	softmax = but->softmax;
	softrange = softmax - softmin;

	/* yes, 'mx' as both x/y is intentional */
	ui_mouse_scale_warp(data, mx, mx, &mx_fl, &my_fl, shift);

	if (but->type == NUMSLI) {
		offs = (BLI_rctf_size_y(&but->rect) / 2.0f);
		deler = BLI_rctf_size_x(&but->rect) - offs;
	}
	else if (but->type == SCROLL) {
		const float size = (is_horizontal) ? BLI_rctf_size_x(&but->rect) : -BLI_rctf_size_y(&but->rect);
		deler = size * (but->softmax - but->softmin) / (but->softmax - but->softmin + but->a1);
		offs = 0.0;
	}
	else {
		offs = (BLI_rctf_size_y(&but->rect) / 2.0f);
		deler = (BLI_rctf_size_x(&but->rect) - offs);
	}

	f = (mx_fl - data->dragstartx) / deler + data->dragfstart;
	CLAMP(f, 0.0f, 1.0f);


	/* deal with mouse correction */
#ifdef USE_CONT_MOUSE_CORRECT
	if (ui_is_a_warp_but(but)) {
		/* OK but can go outside bounds */
		if (is_horizontal) {
			data->ungrab_mval[0] = (but->rect.xmin + offs) + (f * deler);
			data->ungrab_mval[1] = BLI_rctf_cent_y(&but->rect);
		}
		else {
			data->ungrab_mval[1] = (but->rect.ymin + offs) + (f * deler);
			data->ungrab_mval[0] = BLI_rctf_cent_x(&but->rect);
		}
		BLI_rctf_clamp_pt_v(&but->rect, data->ungrab_mval);
	}
#endif
	/* done correcting mouse */


	tempf = softmin + f * softrange;
	temp = floorf(tempf + 0.5f);

	if (snap) {
		if (tempf == softmin || tempf == softmax) {
			/* pass */
		}
		else if (ui_is_but_float(but)) {

			if (shift) {
				if      (tempf == softmin || tempf == softmax) {}
				else if (softmax - softmin < 2.10f) tempf = 0.01f * floorf(100.0f * tempf);
				else if (softmax - softmin < 21.0f) tempf = 0.1f  * floorf(10.0f * tempf);
				else                                tempf = floorf(tempf);
			}
			else {
				if      (softmax - softmin < 2.10f) tempf = 0.1f * floorf(10.0f * tempf);
				else if (softmax - softmin < 21.0f) tempf = floorf(tempf);
				else                                tempf = 10.0f * floorf(tempf / 10.0f);
			}
		}
		else {
			temp = 10 * (temp / 10);
			tempf = temp;
		}
	}

	if (!ui_is_but_float(but)) {
		lvalue = floor(data->value + 0.5);

		CLAMP(temp, softmin, softmax);

		if (temp != lvalue) {
			data->value = temp;
			data->dragchange = true;
			changed = true;
		}
	}
	else {
		CLAMP(tempf, softmin, softmax);

		if (tempf != (float)data->value) {
			data->value = tempf;
			data->dragchange = true;
			changed = true;
		}
	}

	return changed;
}

static int ui_do_but_SLI(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my, click = 0;
	int retval = WM_UI_HANDLER_CONTINUE;

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		int type = event->type, val = event->val;

		if (type == MOUSEPAN) {
			ui_pan_to_scroll(event, &type, &val);
		}

		/* XXX hardcoded keymap check.... */
		if (type == MOUSEPAN && event->alt)
			retval = WM_UI_HANDLER_BREAK; /* allow accumulating values, otherwise scrolling gets preference */
		else if (type == WHEELDOWNMOUSE && event->alt) {
			mx = but->rect.xmin;
			click = 2;
		}
		else if (type == WHEELUPMOUSE && event->alt) {
			mx = but->rect.xmax;
			click = 2;
		}
		else if (event->val == KM_PRESS) {
			if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->ctrl) {
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
			/* alt-click on sides to get "arrows" like in NUM buttons, and match wheel usage above */
			else if (event->type == LEFTMOUSE && event->alt) {
				int halfpos = BLI_rctf_cent_x(&but->rect);
				click = 2;
				if (mx < halfpos)
					mx = but->rect.xmin;
				else
					mx = but->rect.xmax;
			}
			else if (event->type == LEFTMOUSE) {
				data->dragstartx = mx;
				data->draglastx = mx;
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
			else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS) {
				click = 1;
			}
			else if (event->type == MINUSKEY && event->val == KM_PRESS) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				data->value = -data->value;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_BREAK;
			}
		}
#ifdef USE_DRAG_MULTINUM
		copy_v2_v2_int(data->multi_data.drag_start, &event->x);
#endif
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY || event->type == RIGHTMOUSE) {
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			if (data->dragchange) {
#ifdef USE_DRAG_MULTINUM
				/* if we started multibutton but didnt drag, then edit */
				if (data->multi_data.init == BUTTON_MULTI_INIT_SETUP) {
					click = 1;
				}
				else
#endif
				{
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
			}
			else {
				click = 1;
			}
		}
		else if (event->type == MOUSEMOVE) {
#ifdef USE_DRAG_MULTINUM
			data->multi_data.drag_dir[0] += abs(data->draglastx - mx);
			data->multi_data.drag_dir[1] += abs(data->draglasty - my);
#endif
			if (ui_numedit_but_SLI(but, data, mx, true, event->ctrl != 0, event->shift != 0))
				ui_numedit_apply(C, block, but, data);

#ifdef USE_DRAG_MULTINUM
			else if (data->multi_data.has_mbuts) {
				if (data->multi_data.init == BUTTON_MULTI_INIT_ENABLE) {
					ui_multibut_states_apply(C, data, block);
				}
			}
#endif
		}
		retval = WM_UI_HANDLER_BREAK;
	}
	else if (data->state == BUTTON_STATE_TEXT_EDITING) {
		ui_do_but_textedit(C, block, but, data, event);
		retval = WM_UI_HANDLER_BREAK;
	}
	else if (data->state == BUTTON_STATE_TEXT_SELECTING) {
		ui_do_but_textedit_select(C, block, but, data, event);
		retval = WM_UI_HANDLER_BREAK;
	}

	if (click) {
		if (click == 2) {
			/* nudge slider to the left or right */
			float f, tempf, softmin, softmax, softrange;
			int temp;
			
			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			
			softmin = but->softmin;
			softmax = but->softmax;
			softrange = softmax - softmin;
			
			tempf = data->value;
			temp = (int)data->value;

#if 0
			if (but->type == SLI) {
				f = (float)(mx - but->rect.xmin) / (BLI_rctf_size_x(&but->rect)); /* same as below */
			}
			else
#endif
			{
				f = (float)(mx - but->rect.xmin) / (BLI_rctf_size_x(&but->rect));
			}
			
			f = softmin + f * softrange;
			
			if (!ui_is_but_float(but)) {
				if (f < temp) temp--;
				else temp++;
				
				if (temp >= softmin && temp <= softmax)
					data->value = temp;
				else
					data->cancel = true;
			}
			else {
				if (f < tempf) tempf -= 0.01f;
				else tempf += 0.01f;
				
				if (tempf >= softmin && tempf <= softmax)
					data->value = tempf;
				else
					data->cancel = true;
			}
			
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			retval = WM_UI_HANDLER_BREAK;
		}
		else {
			/* edit the value directly */
			button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			retval = WM_UI_HANDLER_BREAK;
		}
	}

	data->draglastx = mx;
	data->draglasty = my;
	
	return retval;
}

static int ui_do_but_SCROLL(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my /*, click = 0 */;
	int retval = WM_UI_HANDLER_CONTINUE;
	bool horizontal = (BLI_rctf_size_x(&but->rect) > BLI_rctf_size_y(&but->rect));
	
	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->val == KM_PRESS) {
			if (event->type == LEFTMOUSE) {
				if (horizontal) {
					data->dragstartx = mx;
					data->draglastx = mx;
				}
				else {
					data->dragstartx = my;
					data->draglastx = my;
				}
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
			/* UNUSED - otherwise code is ok, add back if needed */
#if 0
			else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS)
				click = 1;
#endif
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if (event->type == MOUSEMOVE) {
			if (ui_numedit_but_SLI(but, data, (horizontal) ? mx : my, horizontal, false, false))
				ui_numedit_apply(C, block, but, data);
		}

		retval = WM_UI_HANDLER_BREAK;
	}
	
	return retval;
}

static int ui_do_but_GRIP(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my;
	int retval = WM_UI_HANDLER_CONTINUE;
	const bool horizontal = (BLI_rctf_size_x(&but->rect) < BLI_rctf_size_y(&but->rect));

	/* Note: Having to store org point in window space and recompute it to block "space" each time
	 *       is not ideal, but this is a way to hack around behavior of ui_window_to_block(), which
	 *       returns different results when the block is inside a panel or not...
	 *       See T37739.
	 */

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->val == KM_PRESS) {
			if (event->type == LEFTMOUSE) {
				data->dragstartx = event->x;
				data->dragstarty = event->y;
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if (event->type == MOUSEMOVE) {
			int dragstartx = data->dragstartx;
			int dragstarty = data->dragstarty;
			ui_window_to_block(data->region, block, &dragstartx, &dragstarty);
			data->value = data->origvalue + (horizontal ? mx - dragstartx : dragstarty - my);
			ui_numedit_apply(C, block, but, data);
		}

		retval = WM_UI_HANDLER_BREAK;
	}

	return retval;
}

static int ui_do_but_LISTROW(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		/* hack to pass on ctrl+click and double click to overlapping text
		 * editing field for editing list item names
		 */
		if ((ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS && event->ctrl) ||
		    (event->type == LEFTMOUSE && event->val == KM_DBL_CLICK))
		{
			uiBut *labelbut = ui_but_list_row_text_activate(C, but, data, event, BUTTON_ACTIVATE_TEXT_EDITING);
			if (labelbut) {
				/* Nothing else to do. */
				return WM_UI_HANDLER_BREAK;
			}
		}
	}

	return ui_do_but_EXIT(C, but, data, event);
}

static int ui_do_but_BLOCK(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		
		/* first handle click on icondrag type button */
		if (event->type == LEFTMOUSE && but->dragpoin && event->val == KM_PRESS) {
			if (ui_but_mouse_inside_icon(but, data->region, event)) {
				button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
				data->dragstartx = event->x;
				data->dragstarty = event->y;
				return WM_UI_HANDLER_BREAK;
			}
		}
#ifdef USE_DRAG_TOGGLE
		if (event->type == LEFTMOUSE && event->val == KM_PRESS && (ui_is_but_drag_toggle(but))) {
			button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
			data->dragstartx = event->x;
			data->dragstarty = event->y;
			return WM_UI_HANDLER_BREAK;
		}
#endif
		/* regular open menu */
		if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
			return WM_UI_HANDLER_BREAK;
		}
		else if (but->type == MENU) {
			if (ELEM(event->type, WHEELDOWNMOUSE, WHEELUPMOUSE) && event->alt) {
				const int direction = (event->type == WHEELDOWNMOUSE) ? -1 : 1;

				data->value = ui_step_name_menu(but, direction);

				button_activate_state(C, but, BUTTON_STATE_EXIT);
				ui_apply_button(C, but->block, but, data, true);

				/* button's state need to be changed to EXIT so moving mouse away from this mouse wouldn't lead
				 * to cancel changes made to this button, but changing state to EXIT also makes no button active for
				 * a while which leads to triggering operator when doing fast scrolling mouse wheel.
				 * using post activate stuff from button allows to make button be active again after checking for all
				 * all that mouse leave and cancel stuff, so quick scroll wouldn't be an issue anymore.
				 * same goes for scrolling wheel in another direction below (sergey)
				 */
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_OVER;

				/* without this, a new interface that draws as result of the menu change
				 * won't register that the mouse is over it, eg:
				 * Alt+MouseWheel over the render slots, without this,
				 * the slot menu fails to switch a second time.
				 *
				 * The active state of the button could be maintained some other way
				 * and remove this mousemove event.
				 */
				WM_event_add_mousemove(C);

				return WM_UI_HANDLER_BREAK;
			}
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_DRAG) {
		
		/* this function also ends state */
		if (ui_but_start_drag(C, but, data, event)) {
			return WM_UI_HANDLER_BREAK;
		}
		
		/* outside icon quit, not needed if drag activated */
		if (0 == ui_but_mouse_inside_icon(but, data->region, event)) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			data->cancel = true;
			return WM_UI_HANDLER_BREAK;
		}
		
		if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
			button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
			return WM_UI_HANDLER_BREAK;
		}

	}

	return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_NORMAL(uiBut *but, uiHandleButtonData *data,
                                  int mx, int my,
                                  const enum eSnapType snap)
{
	float dx, dy, rad, radsq, mrad, *fp;
	int mdx, mdy;
	bool changed = true;
	
	/* button is presumed square */
	/* if mouse moves outside of sphere, it does negative normal */

	/* note that both data->vec and data->origvec should be normalized
	 * else we'll get a harmless but annoying jump when first clicking */

	fp = data->origvec;
	rad = BLI_rctf_size_x(&but->rect);
	radsq = rad * rad;
	
	if (fp[2] > 0.0f) {
		mdx = (rad * fp[0]);
		mdy = (rad * fp[1]);
	}
	else if (fp[2] > -1.0f) {
		mrad = rad / sqrtf(fp[0] * fp[0] + fp[1] * fp[1]);
		
		mdx = 2.0f * mrad * fp[0] - (rad * fp[0]);
		mdy = 2.0f * mrad * fp[1] - (rad * fp[1]);
	}
	else {
		mdx = mdy = 0;
	}
	
	dx = (float)(mx + mdx - data->dragstartx);
	dy = (float)(my + mdy - data->dragstarty);

	fp = data->vec;
	mrad = dx * dx + dy * dy;
	if (mrad < radsq) { /* inner circle */
		fp[0] = dx;
		fp[1] = dy;
		fp[2] = sqrtf(radsq - dx * dx - dy * dy);
	}
	else {  /* outer circle */
		
		mrad = rad / sqrtf(mrad);  // veclen
		
		dx *= (2.0f * mrad - 1.0f);
		dy *= (2.0f * mrad - 1.0f);
		
		mrad = dx * dx + dy * dy;
		if (mrad < radsq) {
			fp[0] = dx;
			fp[1] = dy;
			fp[2] = -sqrtf(radsq - dx * dx - dy * dy);
		}
	}
	normalize_v3(fp);

	if (snap != SNAP_OFF) {
		const int snap_steps = (snap == SNAP_ON) ? 4 : 12;  /* 45 or 15 degree increments */
		const float snap_steps_angle = M_PI / snap_steps;
		float angle, angle_snap;
		int i;

		/* round each axis of 'fp' to the next increment
		 * do this in "angle" space - this gives increments of same size */
		for (i = 0; i < 3; i++) {
			angle = asinf(fp[i]);
			angle_snap = floorf(0.5f + (angle / snap_steps_angle)) * snap_steps_angle;
			fp[i] = sinf(angle_snap);
		}
		normalize_v3(fp);
		changed = !compare_v3v3(fp, data->origvec, FLT_EPSILON);
	}

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static int ui_do_but_COLOR(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		/* first handle click on icondrag type button */
		if (event->type == LEFTMOUSE && but->dragpoin && event->val == KM_PRESS) {
			if (ui_but_mouse_inside_icon(but, data->region, event)) {
				button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
				data->dragstartx = event->x;
				data->dragstarty = event->y;
				return WM_UI_HANDLER_BREAK;
			}
		}
#ifdef USE_DRAG_TOGGLE
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_WAIT_DRAG);
			data->dragstartx = event->x;
			data->dragstarty = event->y;
			return WM_UI_HANDLER_BREAK;
		}
#endif
		/* regular open menu */
		if (ELEM(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
			return WM_UI_HANDLER_BREAK;
		}
		else if (ELEM(event->type, MOUSEPAN, WHEELDOWNMOUSE, WHEELUPMOUSE) && event->alt) {
			float *hsv = ui_block_hsv_get(but->block);
			float col[3];

			ui_get_but_vectorf(but, col);
			rgb_to_hsv_compat_v(col, hsv);

			if (event->type == WHEELDOWNMOUSE)
				hsv[2] = CLAMPIS(hsv[2] - 0.05f, 0.0f, 1.0f);
			else if (event->type == WHEELUPMOUSE)
				hsv[2] = CLAMPIS(hsv[2] + 0.05f, 0.0f, 1.0f);
			else {
				float fac = 0.005 * (event->y - event->prevy);
				hsv[2] = CLAMPIS(hsv[2] + fac, 0.0f, 1.0f);
			}

			hsv_to_rgb_v(hsv, data->vec);
			ui_set_but_vectorf(but, data->vec);

			button_activate_state(C, but, BUTTON_STATE_EXIT);
			ui_apply_button(C, but->block, but, data, true);
			return WM_UI_HANDLER_BREAK;
		}
		else if ((int)(but->a1) == UI_PALETTE_COLOR &&
		         event->type == DELKEY && event->val == KM_PRESS)
		{
			Scene *scene = CTX_data_scene(C);
			Paint *paint = BKE_paint_get_active(scene);
			Palette *palette = BKE_paint_palette(paint);
			PaletteColor *color = but->rnapoin.data;

			BKE_palette_color_remove(palette, color);

			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_DRAG) {

		/* this function also ends state */
		if (ui_but_start_drag(C, but, data, event)) {
			return WM_UI_HANDLER_BREAK;
		}

		/* outside icon quit, not needed if drag activated */
		if (0 == ui_but_mouse_inside_icon(but, data->region, event)) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			data->cancel = true;
			return WM_UI_HANDLER_BREAK;
		}

		if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
			if ((int)(but->a1) == UI_PALETTE_COLOR) {
				Palette *palette = but->rnapoin.id.data;
				PaletteColor *color = but->rnapoin.data;
				palette->active_color = BLI_findindex(&palette->colors, color);

				if (!event->ctrl) {
					float color[3];
					Scene *scene = CTX_data_scene(C);
					Paint *paint = BKE_paint_get_active(scene);
					Brush *brush = BKE_paint_brush(paint);

					if (brush->flag & BRUSH_USE_GRADIENT) {
						float *target = &brush->gradient->data[brush->gradient->cur].r;

						if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
							RNA_property_float_get_array(&but->rnapoin, but->rnaprop, target);
							ui_block_to_scene_linear_v3(but->block, target);
						}
						else if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
							RNA_property_float_get_array(&but->rnapoin, but->rnaprop, target);
						}
					}
					else {
						if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
							RNA_property_float_get_array(&but->rnapoin, but->rnaprop, color);
							BKE_brush_color_set(scene, brush, color);
						}
						else if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
							RNA_property_float_get_array(&but->rnapoin, but->rnaprop, color);
							ui_block_to_display_space_v3(but->block, color);
							BKE_brush_color_set(scene, brush, color);
						}
					}

					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				else {
					button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
				}
			}
			else {
				button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
			}
			return WM_UI_HANDLER_BREAK;
		}

	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_NORMAL(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my;

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			const enum eSnapType snap = ui_event_to_snap(event);
			data->dragstartx = mx;
			data->dragstarty = my;
			data->draglastx = mx;
			data->draglasty = my;
			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

			/* also do drag the first time */
			if (ui_numedit_but_NORMAL(but, data, mx, my, snap))
				ui_numedit_apply(C, block, but, data);
			
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				const enum eSnapType snap = ui_event_to_snap(event);
				if (ui_numedit_but_NORMAL(but, data, mx, my, snap))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}

		return WM_UI_HANDLER_BREAK;
	}
	
	return WM_UI_HANDLER_CONTINUE;
}

/* scales a vector so no axis exceeds max
 * (could become BLI_math func) */
static void clamp_axis_max_v3(float v[3], const float max)
{
	const float v_max = max_fff(v[0], v[1], v[2]);
	if (v_max > max) {
		mul_v3_fl(v, max / v_max);
		if (v[0] > max) v[0] = max;
		if (v[1] > max) v[1] = max;
		if (v[2] > max) v[2] = max;
	}
}

static void ui_rgb_to_color_picker_HSVCUBE_compat_v(uiBut *but, const float rgb[3], float hsv[3])
{
	if (but->a1 == UI_GRAD_L_ALT)
		rgb_to_hsl_compat_v(rgb, hsv);
	else
		rgb_to_hsv_compat_v(rgb, hsv);
}

static void ui_rgb_to_color_picker_HSVCUBE_v(uiBut *but, const float rgb[3], float hsv[3])
{
	if (but->a1 == UI_GRAD_L_ALT)
		rgb_to_hsl_v(rgb, hsv);
	else
		rgb_to_hsv_v(rgb, hsv);
}

static void ui_color_picker_to_rgb_HSVCUBE_v(uiBut *but, const float hsv[3], float rgb[3])
{
	if (but->a1 == UI_GRAD_L_ALT)
		hsl_to_rgb_v(hsv, rgb);
	else
		hsv_to_rgb_v(hsv, rgb);
}

static bool ui_numedit_but_HSVCUBE(uiBut *but, uiHandleButtonData *data,
                                   int mx, int my,
                                   const enum eSnapType snap, const bool shift)
{
	float rgb[3];
	float *hsv = ui_block_hsv_get(but->block);
	float x, y;
	float mx_fl, my_fl;
	bool changed = true;
	bool use_display_colorspace = ui_color_picker_use_display_colorspace(but);

	ui_mouse_scale_warp(data, mx, my, &mx_fl, &my_fl, shift);

#ifdef USE_CONT_MOUSE_CORRECT
	if (ui_is_a_warp_but(but)) {
		/* OK but can go outside bounds */
		data->ungrab_mval[0] = mx_fl;
		data->ungrab_mval[1] = my_fl;
		BLI_rctf_clamp_pt_v(&but->rect, data->ungrab_mval);
	}
#endif

	ui_get_but_vectorf(but, rgb);

	if (use_display_colorspace)
		ui_block_to_display_space_v3(but->block, rgb);

	ui_rgb_to_color_picker_HSVCUBE_compat_v(but, rgb, hsv);
	
	/* only apply the delta motion, not absolute */
	if (shift) {
		rcti rect_i;
		float xpos, ypos, hsvo[3];
		
		BLI_rcti_rctf_copy(&rect_i, &but->rect);
		
		/* calculate original hsv again */
		copy_v3_v3(rgb, data->origvec);
		if (use_display_colorspace)
			ui_block_to_display_space_v3(but->block, rgb);
		
		copy_v3_v3(hsvo, ui_block_hsv_get(but->block));

		ui_rgb_to_color_picker_HSVCUBE_compat_v(but, rgb, hsvo);
		
		/* and original position */
		ui_hsvcube_pos_from_vals(but, &rect_i, hsvo, &xpos, &ypos);
		
		mx_fl = xpos - (data->dragstartx - mx_fl);
		my_fl = ypos - (data->dragstarty - my_fl);
	}
	
	/* relative position within box */
	x = ((float)mx_fl - but->rect.xmin) / BLI_rctf_size_x(&but->rect);
	y = ((float)my_fl - but->rect.ymin) / BLI_rctf_size_y(&but->rect);
	CLAMP(x, 0.0f, 1.0f);
	CLAMP(y, 0.0f, 1.0f);

	switch ((int)but->a1) {
		case UI_GRAD_SV:
			hsv[2] = x;
			hsv[1] = y;
			break;
		case UI_GRAD_HV:
			hsv[0] = x;
			hsv[2] = y;
			break;
		case UI_GRAD_HS:
			hsv[0] = x;
			hsv[1] = y;
			break;
		case UI_GRAD_H:
			hsv[0] = x;
			break;
		case UI_GRAD_S:
			hsv[1] = x;
			break;
		case UI_GRAD_V:
			hsv[2] = x;
			break;
		case UI_GRAD_L_ALT:
			hsv[2] = y;
			break;
		case UI_GRAD_V_ALT:
			/* vertical 'value' strip */

			/* exception only for value strip - use the range set in but->min/max */
			hsv[2] = y * (but->softmax - but->softmin) + but->softmin;
			break;
		default:
			BLI_assert(0);
			break;
	}

	if (snap != SNAP_OFF) {
		if (ELEM((int)but->a1, UI_GRAD_HV, UI_GRAD_HS, UI_GRAD_H)) {
			ui_color_snap_hue(snap, &hsv[0]);
		}
	}

	ui_color_picker_to_rgb_HSVCUBE_v(but, hsv, rgb);

	if (use_display_colorspace)
		ui_block_to_scene_linear_v3(but->block, rgb);

	/* clamp because with color conversion we can exceed range [#34295] */
	if (but->a1 == UI_GRAD_V_ALT) {
		clamp_axis_max_v3(rgb, but->softmax);
	}

	copy_v3_v3(data->vec, rgb);

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static void ui_ndofedit_but_HSVCUBE(uiBut *but, uiHandleButtonData *data,
                                    const wmNDOFMotionData *ndof,
                                    const enum eSnapType snap, const bool shift)
{
	float *hsv = ui_block_hsv_get(but->block);
	const float hsv_v_max = max_ff(hsv[2], but->softmax);
	float rgb[3];
	float sensitivity = (shift ? 0.15f : 0.3f) * ndof->dt;
	bool use_display_colorspace = ui_color_picker_use_display_colorspace(but);

	ui_get_but_vectorf(but, rgb);

	if (use_display_colorspace)
		ui_block_to_display_space_v3(but->block, rgb);

	ui_rgb_to_color_picker_HSVCUBE_compat_v(but, rgb, hsv);

	switch ((int)but->a1) {
		case UI_GRAD_SV:
			hsv[2] += ndof->rvec[2] * sensitivity;
			hsv[1] += ndof->rvec[0] * sensitivity;
			break;
		case UI_GRAD_HV:
			hsv[0] += ndof->rvec[2] * sensitivity;
			hsv[2] += ndof->rvec[0] * sensitivity;
			break;
		case UI_GRAD_HS:
			hsv[0] += ndof->rvec[2] * sensitivity;
			hsv[1] += ndof->rvec[0] * sensitivity;
			break;
		case UI_GRAD_H:
			hsv[0] += ndof->rvec[2] * sensitivity;
			break;
		case UI_GRAD_S:
			hsv[1] += ndof->rvec[2] * sensitivity;
			break;
		case UI_GRAD_V:
			hsv[2] += ndof->rvec[2] * sensitivity;
			break;
		case UI_GRAD_V_ALT:
		case UI_GRAD_L_ALT:
			/* vertical 'value' strip */
			
			/* exception only for value strip - use the range set in but->min/max */
			hsv[2] += ndof->rvec[0] * sensitivity;
			
			CLAMP(hsv[2], but->softmin, but->softmax);
			break;
		default:
			assert(!"invalid hsv type");
			break;
	}

	if (snap != SNAP_OFF) {
		if (ELEM((int)but->a1, UI_GRAD_HV, UI_GRAD_HS, UI_GRAD_H)) {
			ui_color_snap_hue(snap, &hsv[0]);
		}
	}

	/* ndof specific: the changes above aren't clamping */
	hsv_clamp_v(hsv, hsv_v_max);

	ui_color_picker_to_rgb_HSVCUBE_v(but, hsv, rgb);

	if (use_display_colorspace)
		ui_block_to_scene_linear_v3(but->block, rgb);

	copy_v3_v3(data->vec, rgb);
	ui_set_but_vectorf(but, data->vec);
}

static int ui_do_but_HSVCUBE(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my;

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			const enum eSnapType snap = ui_event_to_snap(event);

			data->dragstartx = mx;
			data->dragstarty = my;
			data->draglastx = mx;
			data->draglasty = my;
			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

			/* also do drag the first time */
			if (ui_numedit_but_HSVCUBE(but, data, mx, my, snap, event->shift != 0))
				ui_numedit_apply(C, block, but, data);
			
			return WM_UI_HANDLER_BREAK;
		}
		else if (event->type == NDOF_MOTION) {
			const wmNDOFMotionData *ndof = event->customdata;
			const enum eSnapType snap = ui_event_to_snap(event);
			
			ui_ndofedit_but_HSVCUBE(but, data, ndof, snap, event->shift != 0);
			
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			ui_apply_button(C, but->block, but, data, true);
			
			return WM_UI_HANDLER_BREAK;
		}
		/* XXX hardcoded keymap check.... */
		else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
			if (ELEM(but->a1, UI_GRAD_V_ALT, UI_GRAD_L_ALT)) {
				int len;
				
				/* reset only value */
				
				len = RNA_property_array_length(&but->rnapoin, but->rnaprop);
				if (ELEM(len, 3, 4)) {
					float rgb[3], def_hsv[3];
					float def[4];
					float *hsv = ui_block_hsv_get(but->block);
					
					RNA_property_float_get_default_array(&but->rnapoin, but->rnaprop, def);
					ui_rgb_to_color_picker_HSVCUBE_v(but, def, def_hsv);

					ui_get_but_vectorf(but, rgb);
					ui_rgb_to_color_picker_HSVCUBE_compat_v(but, rgb, hsv);

					def_hsv[0] = hsv[0];
					def_hsv[1] = hsv[1];
					
					ui_color_picker_to_rgb_HSVCUBE_v(but, def_hsv, rgb);
					ui_set_but_vectorf(but, rgb);
					
					RNA_property_update(C, &but->rnapoin, but->rnaprop);
					return WM_UI_HANDLER_BREAK;
				}
			}
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY || event->type == RIGHTMOUSE) {
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
		else if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				const enum eSnapType snap = ui_event_to_snap(event);

				if (ui_numedit_but_HSVCUBE(but, data, mx, my, snap, event->shift != 0))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		
		return WM_UI_HANDLER_BREAK;
	}

	return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_HSVCIRCLE(uiBut *but, uiHandleButtonData *data,
                                     float mx, float my,
                                     const enum eSnapType snap, const bool shift)
{
	rcti rect;
	bool changed = true;
	float mx_fl, my_fl;
	float rgb[3];
	float *hsv = ui_block_hsv_get(but->block);
	bool use_display_colorspace = ui_color_picker_use_display_colorspace(but);

	ui_mouse_scale_warp(data, mx, my, &mx_fl, &my_fl, shift);
	
#ifdef USE_CONT_MOUSE_CORRECT
	if (ui_is_a_warp_but(but)) {
		/* OK but can go outside bounds */
		data->ungrab_mval[0] = mx_fl;
		data->ungrab_mval[1] = my_fl;
		{	/* clamp */
			const float radius = min_ff(BLI_rctf_size_x(&but->rect), BLI_rctf_size_y(&but->rect)) / 2.0f;
			const float cent[2] = {BLI_rctf_cent_x(&but->rect), BLI_rctf_cent_y(&but->rect)};
			const float len = len_v2v2(cent, data->ungrab_mval);
			if (len > radius) {
				dist_ensure_v2_v2fl(data->ungrab_mval, cent, radius);
			}
		}
	}
#endif

	BLI_rcti_rctf_copy(&rect, &but->rect);

	ui_get_but_vectorf(but, rgb);
	if (use_display_colorspace)
		ui_block_to_display_space_v3(but->block, rgb);

	ui_rgb_to_color_picker_compat_v(rgb, hsv);

	/* exception, when using color wheel in 'locked' value state:
	 * allow choosing a hue for black values, by giving a tiny increment */
	if (but->flag & UI_BUT_COLOR_LOCK) {
		if (U.color_picker_type == USER_CP_CIRCLE_HSV) { // lock
			if (hsv[2] == 0.f) hsv[2] = 0.0001f;
		}
		else {
			if (hsv[2] == 0.f) hsv[2] = 0.0001f;
			if (hsv[2] == 1.f) hsv[2] = 0.9999f;
		}
	}

	/* only apply the delta motion, not absolute */
	if (shift) {
		float xpos, ypos, hsvo[3], rgbo[3];
		
		/* calculate original hsv again */
		copy_v3_v3(hsvo, ui_block_hsv_get(but->block));
		copy_v3_v3(rgbo, data->origvec);
		if (use_display_colorspace)
			ui_block_to_display_space_v3(but->block, rgbo);

		ui_rgb_to_color_picker_compat_v(rgbo, hsvo);

		/* and original position */
		ui_hsvcircle_pos_from_vals(but, &rect, hsvo, &xpos, &ypos);

		mx_fl = xpos - (data->dragstartx - mx_fl);
		my_fl = ypos - (data->dragstarty - my_fl);
		
	}
	
	ui_hsvcircle_vals_from_pos(hsv, hsv + 1, &rect, mx_fl, my_fl);

	if ((but->flag & UI_BUT_COLOR_CUBIC) && (U.color_picker_type == USER_CP_CIRCLE_HSV))
		hsv[1] = 1.0f - sqrt3f(1.0f - hsv[1]);

	if (snap != SNAP_OFF) {
		ui_color_snap_hue(snap, &hsv[0]);
	}

	ui_color_picker_to_rgb_v(hsv, rgb);

	if ((but->flag & UI_BUT_VEC_SIZE_LOCK) && (rgb[0] || rgb[1] || rgb[2])) {
		normalize_v3(rgb);
		mul_v3_fl(rgb, but->a2);
	}

	if (use_display_colorspace)
		ui_block_to_scene_linear_v3(but->block, rgb);

	ui_set_but_vectorf(but, rgb);
	
	data->draglastx = mx;
	data->draglasty = my;
	
	return changed;
}

static void ui_ndofedit_but_HSVCIRCLE(uiBut *but, uiHandleButtonData *data,
                                      const wmNDOFMotionData *ndof,
                                      const enum eSnapType snap, const bool shift)
{
	float *hsv = ui_block_hsv_get(but->block);
	bool use_display_colorspace = ui_color_picker_use_display_colorspace(but);
	float rgb[3];
	float phi, r /*, sqr */ /* UNUSED */, v[2];
	float sensitivity = (shift ? 0.06f : 0.3f) * ndof->dt;
	
	ui_get_but_vectorf(but, rgb);
	if (use_display_colorspace)
		ui_block_to_display_space_v3(but->block, rgb);
	ui_rgb_to_color_picker_compat_v(rgb, hsv);
	
	/* Convert current color on hue/sat disc to circular coordinates phi, r */
	phi = fmodf(hsv[0] + 0.25f, 1.0f) * -2.0f * (float)M_PI;
	r = hsv[1];
	/* sqr = r > 0.0f ? sqrtf(r) : 1; */ /* UNUSED */
	
	/* Convert to 2d vectors */
	v[0] = r * cosf(phi);
	v[1] = r * sinf(phi);
	
	/* Use ndof device y and x rotation to move the vector in 2d space */
	v[0] += ndof->rvec[2] * sensitivity;
	v[1] += ndof->rvec[0] * sensitivity;

	/* convert back to polar coords on circle */
	phi = atan2f(v[0], v[1]) / (2.0f * (float)M_PI) + 0.5f;
	
	/* use ndof Y rotation to additionally rotate hue */
	phi += ndof->rvec[1] * sensitivity * 0.5f;
	r = len_v2(v);

	/* convert back to hsv values, in range [0,1] */
	hsv[0] = phi;
	hsv[1] = r;

	/* exception, when using color wheel in 'locked' value state:
	 * allow choosing a hue for black values, by giving a tiny increment */
	if (but->flag & UI_BUT_COLOR_LOCK) {
		if (U.color_picker_type == USER_CP_CIRCLE_HSV) { // lock
			if (hsv[2] == 0.f) hsv[2] = 0.0001f;
		}
		else {
			if (hsv[2] == 0.f) hsv[2] = 0.0001f;
			if (hsv[2] == 1.f) hsv[2] = 0.9999f;
		}
	}

	if (snap != SNAP_OFF) {
		ui_color_snap_hue(snap, &hsv[0]);
	}

	hsv_clamp_v(hsv, FLT_MAX);

	ui_color_picker_to_rgb_v(hsv, data->vec);
	
	if ((but->flag & UI_BUT_VEC_SIZE_LOCK) && (data->vec[0] || data->vec[1] || data->vec[2])) {
		normalize_v3(data->vec);
		mul_v3_fl(data->vec, but->a2);
	}

	if (use_display_colorspace)
		ui_block_to_scene_linear_v3(but->block, data->vec);
	
	ui_set_but_vectorf(but, data->vec);
}


static int ui_do_but_HSVCIRCLE(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my;
	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);
	
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			const enum eSnapType snap = ui_event_to_snap(event);
			data->dragstartx = mx;
			data->dragstarty = my;
			data->draglastx = mx;
			data->draglasty = my;
			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			
			/* also do drag the first time */
			if (ui_numedit_but_HSVCIRCLE(but, data, mx, my, snap, event->shift != 0))
				ui_numedit_apply(C, block, but, data);
			
			return WM_UI_HANDLER_BREAK;
		}
		else if (event->type == NDOF_MOTION) {
			const enum eSnapType snap = ui_event_to_snap(event);
			const wmNDOFMotionData *ndof = event->customdata;
			
			ui_ndofedit_but_HSVCIRCLE(but, data, ndof, snap, event->shift != 0);

			button_activate_state(C, but, BUTTON_STATE_EXIT);
			ui_apply_button(C, but->block, but, data, true);
			
			return WM_UI_HANDLER_BREAK;
		}
		/* XXX hardcoded keymap check.... */
		else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
			int len;
			
			/* reset only saturation */
			
			len = RNA_property_array_length(&but->rnapoin, but->rnaprop);
			if (len >= 3) {
				float rgb[3], def_hsv[3];
				float *def;
				float *hsv = ui_block_hsv_get(but->block);
				def = MEM_callocN(sizeof(float) * len, "reset_defaults - float");
				
				RNA_property_float_get_default_array(&but->rnapoin, but->rnaprop, def);
				ui_color_picker_to_rgb_v(def, def_hsv);
				
				ui_get_but_vectorf(but, rgb);
				ui_rgb_to_color_picker_compat_v(rgb, hsv);
				
				def_hsv[0] = hsv[0];
				def_hsv[2] = hsv[2];

				hsv_to_rgb_v(def_hsv, rgb);
				ui_set_but_vectorf(but, rgb);
				
				RNA_property_update(C, &but->rnapoin, but->rnaprop);
				
				MEM_freeN(def);
			}
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY || event->type == RIGHTMOUSE) {
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
		/* XXX hardcoded keymap check.... */
		else if (event->type == WHEELDOWNMOUSE) {
			float *hsv = ui_block_hsv_get(but->block);
			hsv[2] = CLAMPIS(hsv[2] - 0.05f, 0.0f, 1.0f);
			ui_set_but_hsv(but);    /* converts to rgb */
			ui_numedit_apply(C, block, but, data);
		}
		else if (event->type == WHEELUPMOUSE) {
			float *hsv = ui_block_hsv_get(but->block);
			hsv[2] = CLAMPIS(hsv[2] + 0.05f, 0.0f, 1.0f);
			ui_set_but_hsv(but);    /* converts to rgb */
			ui_numedit_apply(C, block, but, data);
		}
		else if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				const enum eSnapType snap = ui_event_to_snap(event);

				if (ui_numedit_but_HSVCIRCLE(but, data, mx, my, snap, event->shift != 0)) {
					ui_numedit_apply(C, block, but, data);
				}
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		return WM_UI_HANDLER_BREAK;
	}
	
	return WM_UI_HANDLER_CONTINUE;
}


static bool ui_numedit_but_COLORBAND(uiBut *but, uiHandleButtonData *data, int mx)
{
	float dx;
	bool changed = false;

	if (data->draglastx == mx)
		return changed;

	dx = ((float)(mx - data->draglastx)) / BLI_rctf_size_x(&but->rect);
	data->dragcbd->pos += dx;
	CLAMP(data->dragcbd->pos, 0.0f, 1.0f);
	
	colorband_update_sort(data->coba);
	data->dragcbd = data->coba->data + data->coba->cur;  /* because qsort */
	
	data->draglastx = mx;
	changed = true;

	return changed;
}

static int ui_do_but_COLORBAND(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	ColorBand *coba;
	CBData *cbd;
	int mx, my, a, xco, mindist = 12;

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			coba = (ColorBand *)but->poin;

			if (event->ctrl) {
				/* insert new key on mouse location */
				float pos = ((float)(mx - but->rect.xmin)) / BLI_rctf_size_x(&but->rect);
				colorband_element_add(coba, pos);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else {
				data->dragstartx = mx;
				data->dragstarty = my;
				data->draglastx = mx;
				data->draglasty = my;

				/* activate new key when mouse is close */
				for (a = 0, cbd = coba->data; a < coba->tot; a++, cbd++) {
					xco = but->rect.xmin + (cbd->pos * BLI_rctf_size_x(&but->rect));
					xco = ABS(xco - mx);
					if (a == coba->cur) xco += 5;  // selected one disadvantage
					if (xco < mindist) {
						coba->cur = a;
						mindist = xco;
					}
				}
		
				data->dragcbd = coba->data + coba->cur;
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			}

			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				if (ui_numedit_but_COLORBAND(but, data, mx))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		
		return WM_UI_HANDLER_BREAK;
	}

	return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_CURVE(uiBlock *block, uiBut *but, uiHandleButtonData *data,
                                 int evtx, int evty,
                                 bool snap, const bool shift)
{
	CurveMapping *cumap = (CurveMapping *)but->poin;
	CurveMap *cuma = cumap->cm + cumap->cur;
	CurveMapPoint *cmp = cuma->curve;
	float fx, fy, zoomx, zoomy;
	int mx, my, dragx, dragy;
	int a;
	bool changed = false;

	/* evtx evty and drag coords are absolute mousecoords, prevents errors when editing when layout changes */
	mx = evtx;
	my = evty;
	ui_window_to_block(data->region, block, &mx, &my);
	dragx = data->draglastx;
	dragy = data->draglasty;
	ui_window_to_block(data->region, block, &dragx, &dragy);
	
	zoomx = BLI_rctf_size_x(&but->rect) / BLI_rctf_size_x(&cumap->curr);
	zoomy = BLI_rctf_size_y(&but->rect) / BLI_rctf_size_y(&cumap->curr);
	
	if (snap) {
		float d[2];

		d[0] = mx - data->dragstartx;
		d[1] = my - data->dragstarty;

		if (len_squared_v2(d) < (3.0f * 3.0f))
			snap = false;
	}

	if (data->dragsel != -1) {
		CurveMapPoint *cmp_last = NULL;
		const float mval_factor = ui_mouse_scale_warp_factor(shift);
		bool moved_point = false;  /* for ctrl grid, can't use orig coords because of sorting */

		fx = (mx - dragx) / zoomx;
		fy = (my - dragy) / zoomy;

		fx *= mval_factor;
		fy *= mval_factor;

		for (a = 0; a < cuma->totpoint; a++) {
			if (cmp[a].flag & CUMA_SELECT) {
				float origx = cmp[a].x, origy = cmp[a].y;
				cmp[a].x += fx;
				cmp[a].y += fy;
				if (snap) {
					cmp[a].x = 0.125f * floorf(0.5f + 8.0f * cmp[a].x);
					cmp[a].y = 0.125f * floorf(0.5f + 8.0f * cmp[a].y);
				}
				if (cmp[a].x != origx || cmp[a].y != origy)
					moved_point = true;

				cmp_last = &cmp[a];
			}
		}

		curvemapping_changed(cumap, false);
		
		if (moved_point) {
			data->draglastx = evtx;
			data->draglasty = evty;
			changed = true;

#ifdef USE_CONT_MOUSE_CORRECT
			/* note: using 'cmp_last' is weak since there may be multiple points selected,
			 * but in practice this isnt really an issue */
			if (ui_is_a_warp_but(but)) {
				/* OK but can go outside bounds */
				data->ungrab_mval[0] = but->rect.xmin + ((cmp_last->x - cumap->curr.xmin) * zoomx);
				data->ungrab_mval[1] = but->rect.ymin + ((cmp_last->y - cumap->curr.ymin) * zoomy);
				BLI_rctf_clamp_pt_v(&but->rect, data->ungrab_mval);
			}
#endif

		}

		data->dragchange = true;  /* mark for selection */
	}
	else {
		fx = (mx - dragx) / zoomx;
		fy = (my - dragy) / zoomy;
		
		/* clamp for clip */
		if (cumap->flag & CUMA_DO_CLIP) {
			if (cumap->curr.xmin - fx < cumap->clipr.xmin)
				fx = cumap->curr.xmin - cumap->clipr.xmin;
			else if (cumap->curr.xmax - fx > cumap->clipr.xmax)
				fx = cumap->curr.xmax - cumap->clipr.xmax;
			if (cumap->curr.ymin - fy < cumap->clipr.ymin)
				fy = cumap->curr.ymin - cumap->clipr.ymin;
			else if (cumap->curr.ymax - fy > cumap->clipr.ymax)
				fy = cumap->curr.ymax - cumap->clipr.ymax;
		}

		cumap->curr.xmin -= fx;
		cumap->curr.ymin -= fy;
		cumap->curr.xmax -= fx;
		cumap->curr.ymax -= fy;
		
		data->draglastx = evtx;
		data->draglasty = evty;

		changed = true;
	}

	return changed;
}

static int ui_do_but_CURVE(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my, a;
	bool changed = false;
	Scene *scene = CTX_data_scene(C);

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);
	
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			CurveMapping *cumap = (CurveMapping *)but->poin;
			CurveMap *cuma = cumap->cm + cumap->cur;
			CurveMapPoint *cmp;
			float fx, fy, zoomx, zoomy, offsx, offsy;
			float dist, mindist = 200.0f; // 14 pixels radius
			int sel = -1;

			zoomx = BLI_rctf_size_x(&but->rect) / BLI_rctf_size_x(&cumap->curr);
			zoomy = BLI_rctf_size_y(&but->rect) / BLI_rctf_size_y(&cumap->curr);
			offsx = cumap->curr.xmin;
			offsy = cumap->curr.ymin;

			if (event->ctrl) {
				fx = ((float)mx - but->rect.xmin) / zoomx + offsx;
				fy = ((float)my - but->rect.ymin) / zoomy + offsy;
				
				curvemap_insert(cuma, fx, fy);
				curvemapping_changed(cumap, false);
				changed = true;
			}

			/* check for selecting of a point */
			cmp = cuma->curve;   /* ctrl adds point, new malloc */
			for (a = 0; a < cuma->totpoint; a++) {
				fx = but->rect.xmin + zoomx * (cmp[a].x - offsx);
				fy = but->rect.ymin + zoomy * (cmp[a].y - offsy);
				dist = (fx - mx) * (fx - mx) + (fy - my) * (fy - my);
				if (dist < mindist) {
					sel = a;
					mindist = dist;
				}
			}

			if (sel == -1) {
				int i;

				/* if the click didn't select anything, check if it's clicked on the 
				 * curve itself, and if so, add a point */
				fx = ((float)mx - but->rect.xmin) / zoomx + offsx;
				fy = ((float)my - but->rect.ymin) / zoomy + offsy;
				
				cmp = cuma->table;

				/* loop through the curve segment table and find what's near the mouse.
				 * 0.05 is kinda arbitrary, but seems to be what works nicely. */
				for (i = 0; i <= CM_TABLE; i++) {
					if ((fabsf(fx - cmp[i].x) < 0.05f) &&
					    (fabsf(fy - cmp[i].y) < 0.05f))
					{
					
						curvemap_insert(cuma, fx, fy);
						curvemapping_changed(cumap, false);

						changed = true;
						
						/* reset cmp back to the curve points again, rather than drawing segments */
						cmp = cuma->curve;
						
						/* find newly added point and make it 'sel' */
						for (a = 0; a < cuma->totpoint; a++)
							if (cmp[a].x == fx)
								sel = a;
							
						break;
					}
				}
			}

			if (sel != -1) {
				/* ok, we move a point */
				/* deselect all if this one is deselect. except if we hold shift */
				if (!event->shift) {
					for (a = 0; a < cuma->totpoint; a++) {
						cmp[a].flag &= ~CUMA_SELECT;
					}
					cmp[sel].flag |= CUMA_SELECT;
				}
				else {
					cmp[sel].flag ^= CUMA_SELECT;
				}
			}
			else {
				/* move the view */
				data->cancel = true;
			}

			data->dragsel = sel;
			
			data->dragstartx = event->x;
			data->dragstarty = event->y;
			data->draglastx = event->x;
			data->draglasty = event->y;

			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == MOUSEMOVE) {
			if (event->x != data->draglastx || event->y != data->draglasty) {
				
				if (ui_numedit_but_CURVE(block, but, data, event->x, event->y, event->ctrl != 0, event->shift != 0))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			if (data->dragsel != -1) {
				CurveMapping *cumap = (CurveMapping *)but->poin;
				CurveMap *cuma = cumap->cm + cumap->cur;
				CurveMapPoint *cmp = cuma->curve;

				if (data->dragchange == false) {
					/* deselect all, select one */
					if (!event->shift) {
						for (a = 0; a < cuma->totpoint; a++)
							cmp[a].flag &= ~CUMA_SELECT;
						cmp[data->dragsel].flag |= CUMA_SELECT;
					}
				}
				else {
					curvemapping_changed(cumap, true);  /* remove doubles */
					BKE_paint_invalidate_cursor_overlay(scene, cumap);
				}
			}

			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}

		return WM_UI_HANDLER_BREAK;
	}

	/* UNUSED but keep for now */
	(void)changed;

	return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_HISTOGRAM(uiBut *but, uiHandleButtonData *data, int mx, int my)
{
	Histogram *hist = (Histogram *)but->poin;
	bool changed = true;
	float dy = my - data->draglasty;

	/* scale histogram values (dy / 10 for better control) */
	const float yfac = min_ff(powf(hist->ymax, 2.0f), 1.0f) * 0.5f;
	hist->ymax += (dy * 0.1f) * yfac;

	/* 0.1 allows us to see HDR colors up to 10 */
	CLAMP(hist->ymax, 0.1f, 100.f);

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static int ui_do_but_HISTOGRAM(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my;
	
	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);
	
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			data->dragstartx = mx;
			data->dragstarty = my;
			data->draglastx = mx;
			data->draglasty = my;
			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			
			/* also do drag the first time */
			if (ui_numedit_but_HISTOGRAM(but, data, mx, my))
				ui_numedit_apply(C, block, but, data);
			
			return WM_UI_HANDLER_BREAK;
		}
		/* XXX hardcoded keymap check.... */
		else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
			Histogram *hist = (Histogram *)but->poin;
			hist->ymax = 1.f;
			
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
		else if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				if (ui_numedit_but_HISTOGRAM(but, data, mx, my))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		return WM_UI_HANDLER_BREAK;
	}
	
	return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_WAVEFORM(uiBut *but, uiHandleButtonData *data, int mx, int my)
{
	Scopes *scopes = (Scopes *)but->poin;
	bool changed = true;
	float dy;

	dy = my - data->draglasty;

	/* scale waveform values */
	scopes->wavefrm_yfac += dy / 200.0f;

	CLAMP(scopes->wavefrm_yfac, 0.5f, 2.0f);

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static int ui_do_but_WAVEFORM(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my;

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			data->dragstartx = mx;
			data->dragstarty = my;
			data->draglastx = mx;
			data->draglasty = my;
			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

			/* also do drag the first time */
			if (ui_numedit_but_WAVEFORM(but, data, mx, my))
				ui_numedit_apply(C, block, but, data);

			return WM_UI_HANDLER_BREAK;
		}
		/* XXX hardcoded keymap check.... */
		else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
			Scopes *scopes = (Scopes *)but->poin;
			scopes->wavefrm_yfac = 1.f;

			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
		else if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				if (ui_numedit_but_WAVEFORM(but, data, mx, my))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		return WM_UI_HANDLER_BREAK;
	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_LINK(bContext *C, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{	
	VECCOPY2D(but->linkto, event->mval);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_WAIT_RELEASE);
			return WM_UI_HANDLER_BREAK;
		}
		else if (event->type == LEFTMOUSE && but->block->handle) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_RELEASE) {
		
		if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			if (!(but->flag & UI_SELECT))
				data->cancel = true;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	
	return WM_UI_HANDLER_CONTINUE;
}

static bool ui_numedit_but_TRACKPREVIEW(bContext *C, uiBut *but, uiHandleButtonData *data,
                                        int mx, int my,
                                        const bool shift)
{
	MovieClipScopes *scopes = (MovieClipScopes *)but->poin;
	bool changed = true;
	float dx, dy;

	dx = mx - data->draglastx;
	dy = my - data->draglasty;

	if (shift) {
		dx /= 5.0f;
		dy /= 5.0f;
	}

	if (!scopes->track_locked) {
		if (scopes->marker->framenr != scopes->framenr)
			scopes->marker = BKE_tracking_marker_ensure(scopes->track, scopes->framenr);

		scopes->marker->flag &= ~(MARKER_DISABLED | MARKER_TRACKED);
		scopes->marker->pos[0] += -dx * scopes->slide_scale[0] / BLI_rctf_size_x(&but->block->rect);
		scopes->marker->pos[1] += -dy * scopes->slide_scale[1] / BLI_rctf_size_y(&but->block->rect);

		WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);
	}

	scopes->ok = 0;

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static int ui_do_but_TRACKPREVIEW(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, const wmEvent *event)
{
	int mx, my;

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			data->dragstartx = mx;
			data->dragstarty = my;
			data->draglastx = mx;
			data->draglasty = my;
			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

			/* also do drag the first time */
			if (ui_numedit_but_TRACKPREVIEW(C, but, data, mx, my, event->shift != 0))
				ui_numedit_apply(C, block, but, data);

			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			if (event->val == KM_PRESS) {
				data->cancel = true;
				data->escapecancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
		else if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				if (ui_numedit_but_TRACKPREVIEW(C, but, data, mx, my, event->shift != 0))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		return WM_UI_HANDLER_BREAK;
	}

	return WM_UI_HANDLER_CONTINUE;
}

static void but_shortcut_name_func(bContext *C, void *arg1, int UNUSED(event))
{
	uiBut *but = (uiBut *)arg1;

	if (but->optype) {
		char shortcut_str[128];

		IDProperty *prop = (but->opptr) ? but->opptr->data : NULL;
		
		/* complex code to change name of button */
		if (WM_key_event_operator_string(C, but->optype->idname, but->opcontext, prop, true,
		                                 shortcut_str, sizeof(shortcut_str)))
		{
			ui_but_add_shortcut(but, shortcut_str, true);
		}
		else {
			/* simply strip the shortcut */
			ui_but_add_shortcut(but, NULL, true);
		}
	}
}

static uiBlock *menu_change_shortcut(bContext *C, ARegion *ar, void *arg)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	uiBlock *block;
	uiBut *but = (uiBut *)arg;
	wmKeyMap *km;
	wmKeyMapItem *kmi;
	PointerRNA ptr;
	uiLayout *layout;
	uiStyle *style = UI_GetStyleDraw();
	IDProperty *prop = (but->opptr) ? but->opptr->data : NULL;
	int kmi_id = WM_key_event_operator_id(C, but->optype->idname, but->opcontext, prop, true, &km);

	kmi = WM_keymap_item_find_id(km, kmi_id);
	
	RNA_pointer_create(&wm->id, &RNA_KeyMapItem, kmi, &ptr);
	
	block = uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetHandleFunc(block, but_shortcut_name_func, but);
	uiBlockSetFlag(block, UI_BLOCK_MOVEMOUSE_QUIT);
	uiBlockSetDirection(block, UI_CENTER);
	
	layout = uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, 200, 20, 0, style);
	
	uiItemR(layout, &ptr, "type", UI_ITEM_R_FULL_EVENT | UI_ITEM_R_IMMEDIATE, "", ICON_NONE);
	
	uiPopupBoundsBlock(block, 6, -50, 26);
	
	return block;
}

static uiBlock *menu_add_shortcut(bContext *C, ARegion *ar, void *arg)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	uiBlock *block;
	uiBut *but = (uiBut *)arg;
	wmKeyMap *km;
	wmKeyMapItem *kmi;
	PointerRNA ptr;
	uiLayout *layout;
	uiStyle *style = UI_GetStyleDraw();
	IDProperty *prop = (but->opptr) ? but->opptr->data : NULL;
	int kmi_id;
	
	/* XXX this guess_opname can potentially return a different keymap than being found on adding later... */
	km = WM_keymap_guess_opname(C, but->optype->idname);
	kmi = WM_keymap_add_item(km, but->optype->idname, AKEY, KM_PRESS, 0, 0);
	kmi_id = kmi->id;

	/* copy properties, prop can be NULL for reset */
	if (prop)
		prop = IDP_CopyProperty(prop);
	WM_keymap_properties_reset(kmi, prop);

	/* update and get pointers again */
	WM_keyconfig_update(wm);

	km = WM_keymap_guess_opname(C, but->optype->idname);
	kmi = WM_keymap_item_find_id(km, kmi_id);

	RNA_pointer_create(&wm->id, &RNA_KeyMapItem, kmi, &ptr);

	block = uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetHandleFunc(block, but_shortcut_name_func, but);
	uiBlockSetDirection(block, UI_CENTER);

	layout = uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, 200, 20, 0, style);

	uiItemR(layout, &ptr, "type", UI_ITEM_R_FULL_EVENT | UI_ITEM_R_IMMEDIATE, "", ICON_NONE);
	
	uiPopupBoundsBlock(block, 6, -50, 26);
	
	return block;
}

static void menu_add_shortcut_cancel(struct bContext *C, void *arg1)
{
	uiBut *but = (uiBut *)arg1;
	wmKeyMap *km;
	wmKeyMapItem *kmi;
	IDProperty *prop = (but->opptr) ? but->opptr->data : NULL;
	int kmi_id = WM_key_event_operator_id(C, but->optype->idname, but->opcontext, prop, true, &km);
	
	kmi = WM_keymap_item_find_id(km, kmi_id);
	WM_keymap_remove_item(km, kmi);
}

static void popup_change_shortcut_func(bContext *C, void *arg1, void *UNUSED(arg2))
{
	uiBut *but = (uiBut *)arg1;
	button_timers_tooltip_remove(C, but);
	uiPupBlock(C, menu_change_shortcut, but);
}

static void remove_shortcut_func(bContext *C, void *arg1, void *UNUSED(arg2))
{
	uiBut *but = (uiBut *)arg1;
	wmKeyMap *km;
	wmKeyMapItem *kmi;
	IDProperty *prop = (but->opptr) ? but->opptr->data : NULL;
	int kmi_id = WM_key_event_operator_id(C, but->optype->idname, but->opcontext, prop, true, &km);
	
	kmi = WM_keymap_item_find_id(km, kmi_id);
	WM_keymap_remove_item(km, kmi);
	
	but_shortcut_name_func(C, but, 0);
}

static void popup_add_shortcut_func(bContext *C, void *arg1, void *UNUSED(arg2))
{
	uiBut *but = (uiBut *)arg1;
	button_timers_tooltip_remove(C, but);
	uiPupBlockEx(C, menu_add_shortcut, NULL, menu_add_shortcut_cancel, but);
}

/**
 * menu to chow when right clicking on the panel header
 */
void ui_panel_menu(bContext *C, ARegion *ar, Panel *pa)
{
	bScreen *sc = CTX_wm_screen(C);
	PointerRNA ptr;
	uiPopupMenu *pup;
	uiLayout *layout;

	RNA_pointer_create(&sc->id, &RNA_Panel, pa, &ptr);

	pup = uiPupMenuBegin(C, IFACE_("Panel"), ICON_NONE);
	layout = uiPupMenuLayout(pup);
	if (UI_panel_category_is_visible(ar)) {
		char tmpstr[80];
		BLI_snprintf(tmpstr, sizeof(tmpstr), "%s" UI_SEP_CHAR_S "%s", IFACE_("Pin"), IFACE_("Shift+Left Mouse"));
		uiItemR(layout, &ptr, "use_pin", 0, tmpstr, ICON_NONE);

		/* evil, force shortcut flag */
		{
			uiBlock *block = uiLayoutGetBlock(layout);
			uiBut *but = block->buttons.last;
			but->flag |= UI_BUT_HAS_SEP_CHAR;
		}

	}
	uiPupMenuEnd(C, pup);
}

static bool ui_but_menu(bContext *C, uiBut *but)
{
	uiPopupMenu *pup;
	uiLayout *layout;
	bool is_array, is_array_component;
	uiStringInfo label = {BUT_GET_LABEL, NULL};

/*	if ((but->rnapoin.data && but->rnaprop) == 0 && but->optype == NULL)*/
/*		return 0;*/

	/* having this menu for some buttons makes no sense */
	if (but->type == BUT_IMAGE) {
		return false;
	}
	
	button_timers_tooltip_remove(C, but);

	/* highly unlikely getting the label ever fails */
	uiButGetStrInfo(C, but, &label, NULL);

	pup = uiPupMenuBegin(C, label.strinfo ? label.strinfo : "", ICON_NONE);
	layout = uiPupMenuLayout(pup);
	if (label.strinfo)
		MEM_freeN(label.strinfo);

	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

	if (but->rnapoin.data && but->rnaprop) {
		PointerRNA *ptr = &but->rnapoin;
		PropertyRNA *prop = but->rnaprop;
		bool is_anim = RNA_property_animateable(ptr, prop);
		bool is_editable = RNA_property_editable(ptr, prop);
		/*bool is_idprop = RNA_property_is_idprop(prop);*/ /* XXX does not work as expected, not strictly needed */
		bool is_set = RNA_property_is_set(ptr, prop);

		/* second slower test, saved people finding keyframe items in menus when its not possible */
		if (is_anim)
			is_anim = RNA_property_path_from_ID_check(&but->rnapoin, but->rnaprop);

		/* determine if we can key a single component of an array */
		is_array = RNA_property_array_length(&but->rnapoin, but->rnaprop) != 0;
		is_array_component = (is_array && but->rnaindex != -1);
		
		/* Keyframes */
		if (but->flag & UI_BUT_ANIMATED_KEY) {
			/* replace/delete keyfraemes */
			if (is_array_component) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Replace Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Replace Single Keyframe"),
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 0);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_delete_button", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Single Keyframe"),
				               ICON_NONE, "ANIM_OT_keyframe_delete_button", "all", 0);
			}
			else {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Replace Keyframe"),
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Keyframe"),
				               ICON_NONE, "ANIM_OT_keyframe_delete_button", "all", 1);
			}
			
			/* keyframe settings */
			uiItemS(layout);
			
			
		}
		else if (but->flag & UI_BUT_DRIVEN) {
			/* pass */
		}
		else if (is_anim) {
			if (is_array_component) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Insert Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Insert Single Keyframe"),
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 0);
			}
			else {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Insert Keyframe"),
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 1);
			}
		}
		
		if (but->flag & UI_BUT_ANIMATED) {
			if (is_array_component) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_clear_button", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Single Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_clear_button", "all", 0);
			}
			else {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_clear_button", "all", 1);
			}
		}

		/* Drivers */
		if (but->flag & UI_BUT_DRIVEN) {
			uiItemS(layout);

			if (is_array_component) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Drivers"),
				               ICON_NONE, "ANIM_OT_driver_button_remove", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Single Driver"),
				               ICON_NONE, "ANIM_OT_driver_button_remove", "all", 0);
			}
			else {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Driver"),
				               ICON_NONE, "ANIM_OT_driver_button_remove", "all", 1);
			}

			uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Copy Driver"),
			        ICON_NONE, "ANIM_OT_copy_driver_button");
			if (ANIM_driver_can_paste()) {
				uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Paste Driver"),
				        ICON_NONE, "ANIM_OT_paste_driver_button");
			}
		}
		else if (but->flag & (UI_BUT_ANIMATED_KEY | UI_BUT_ANIMATED)) {
			/* pass */
		}
		else if (is_anim) {
			uiItemS(layout);

			if (is_array_component) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add Drivers"),
				               ICON_NONE, "ANIM_OT_driver_button_add", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add Single Driver"),
				               ICON_NONE, "ANIM_OT_driver_button_add", "all", 0);
			}
			else {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add Driver"),
				               ICON_NONE, "ANIM_OT_driver_button_add", "all", 1);
			}

			if (ANIM_driver_can_paste()) {
				uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Paste Driver"),
				        ICON_NONE, "ANIM_OT_paste_driver_button");
			}
		}
		
		/* Keying Sets */
		/* TODO: check on modifyability of Keying Set when doing this */
		if (is_anim) {
			uiItemS(layout);

			if (is_array_component) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add All to Keying Set"),
				               ICON_NONE, "ANIM_OT_keyingset_button_add", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add Single to Keying Set"),
				               ICON_NONE, "ANIM_OT_keyingset_button_add", "all", 0);
				uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Remove from Keying Set"),
				        ICON_NONE, "ANIM_OT_keyingset_button_remove");
			}
			else {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add to Keying Set"),
				               ICON_NONE, "ANIM_OT_keyingset_button_add", "all", 1);
				uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Remove from Keying Set"),
				        ICON_NONE, "ANIM_OT_keyingset_button_remove");
			}
		}
		
		uiItemS(layout);
		
		/* Property Operators */
		
		/* Copy Property Value
		 * Paste Property Value */
		
		if (is_array_component) {
			uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Reset All to Default Values"),
			               ICON_NONE, "UI_OT_reset_default_button", "all", 1);
			uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Reset Single to Default Value"),
			               ICON_NONE, "UI_OT_reset_default_button", "all", 0);
		}
		else {
			uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Reset to Default Value"),
			        ICON_NONE, "UI_OT_reset_default_button", "all", 1);
		}
		if (is_editable /*&& is_idprop*/ && is_set) {
			uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Unset"),
			        ICON_NONE, "UI_OT_unset_property_button");
		}
		
		uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Copy Data Path"),
		        ICON_NONE, "UI_OT_copy_data_path_button");
		uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Copy To Selected"),
		        ICON_NONE, "UI_OT_copy_to_selected_button");

		uiItemS(layout);
	}

	/* Operator buttons */
	if (but->optype) {
		uiBlock *block = uiLayoutGetBlock(layout);
		uiBut *but2;
		IDProperty *prop = (but->opptr) ? but->opptr->data : NULL;
		int w = uiLayoutGetWidth(layout);
		wmKeyMap *km;
		wmKeyMapItem *kmi = NULL;
		int kmi_id = WM_key_event_operator_id(C, but->optype->idname, but->opcontext, prop, true, &km);

		if (kmi_id)
			kmi = WM_keymap_item_find_id(km, kmi_id);

		/* keyboard shortcuts */
		if ((kmi) && ISKEYBOARD(kmi->type)) {

			/* would rather use a block but, but gets weirdly positioned... */
			//uiDefBlockBut(block, menu_change_shortcut, but, "Change Shortcut", 0, 0, uiLayoutGetWidth(layout), UI_UNIT_Y, "");
			
			but2 = uiDefIconTextBut(block, BUT, 0, 0, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Change Shortcut"),
			                        0, 0, w, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
			uiButSetFunc(but2, popup_change_shortcut_func, but, NULL);

			but2 = uiDefIconTextBut(block, BUT, 0, 0, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Remove Shortcut"),
			                        0, 0, w, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
			uiButSetFunc(but2, remove_shortcut_func, but, NULL);
		}
		/* only show 'add' if there's a suitable key map for it to go in */
		else if (WM_keymap_guess_opname(C, but->optype->idname)) {
			but2 = uiDefIconTextBut(block, BUT, 0, 0, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add Shortcut"),
			                        0, 0, w, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
			uiButSetFunc(but2, popup_add_shortcut_func, but, NULL);
		}
		
		uiItemS(layout);
	}

	/* Show header tools for header buttons. */
	{
		ARegion *ar = CTX_wm_region(C);
		if (ar && (ar->regiontype == RGN_TYPE_HEADER)) {
			uiItemMenuF(layout, IFACE_("Header"), ICON_NONE, ED_screens_header_tools_menu_create, NULL);
			uiItemS(layout);
		}
	}

	{   /* Docs */
		char buf[512];
		PointerRNA ptr_props;

		if (but->rnapoin.data && but->rnaprop) {
			BLI_snprintf(buf, sizeof(buf), "%s.%s",
			             RNA_struct_identifier(but->rnapoin.type), RNA_property_identifier(but->rnaprop));

			WM_operator_properties_create(&ptr_props, "WM_OT_doc_view_manual");
			RNA_string_set(&ptr_props, "doc_id", buf);
			uiItemFullO(layout, "WM_OT_doc_view_manual", CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Online Manual"),
			            ICON_NONE, ptr_props.data, WM_OP_EXEC_DEFAULT, 0);

			WM_operator_properties_create(&ptr_props, "WM_OT_doc_view");
			RNA_string_set(&ptr_props, "doc_id", buf);
			uiItemFullO(layout, "WM_OT_doc_view", CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Online Python Reference"),
			            ICON_NONE, ptr_props.data, WM_OP_EXEC_DEFAULT, 0);

			/* XXX inactive option, not for public! */
#if 0
			WM_operator_properties_create(&ptr_props, "WM_OT_doc_edit");
			RNA_string_set(&ptr_props, "doc_id", buf);
			RNA_string_set(&ptr_props, "doc_new", RNA_property_description(but->rnaprop));

			uiItemFullO(layout, "WM_OT_doc_edit", "Submit Description", ICON_NONE, ptr_props.data, WM_OP_INVOKE_DEFAULT, 0);
#endif
		}
		else if (but->optype) {
			WM_operator_py_idname(buf, but->optype->idname);


			WM_operator_properties_create(&ptr_props, "WM_OT_doc_view_manual");
			RNA_string_set(&ptr_props, "doc_id", buf);
			uiItemFullO(layout, "WM_OT_doc_view_manual", CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Online Manual"),
			            ICON_NONE, ptr_props.data, WM_OP_EXEC_DEFAULT, 0);

			WM_operator_properties_create(&ptr_props, "WM_OT_doc_view");
			RNA_string_set(&ptr_props, "doc_id", buf);
			uiItemFullO(layout, "WM_OT_doc_view", CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Online Python Reference"),
			            ICON_NONE, ptr_props.data, WM_OP_EXEC_DEFAULT, 0);

			/* XXX inactive option, not for public! */
#if 0
			WM_operator_properties_create(&ptr_props, "WM_OT_doc_edit");
			RNA_string_set(&ptr_props, "doc_id", buf);
			RNA_string_set(&ptr_props, "doc_new", but->optype->description);

			uiItemFullO(layout, "WM_OT_doc_edit", CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Submit Description"),
			            ICON_NONE, ptr_props.data, WM_OP_INVOKE_DEFAULT, 0);
#endif
		}
	}

	/* perhaps we should move this into (G.debug & G_DEBUG) - campbell */
	if (ui_block_is_menu(but->block) == false) {
		uiItemFullO(layout, "UI_OT_editsource", NULL, ICON_NONE, NULL, WM_OP_INVOKE_DEFAULT, 0);
	}
	uiItemFullO(layout, "UI_OT_edittranslation_init", NULL, ICON_NONE, NULL, WM_OP_INVOKE_DEFAULT, 0);

	uiPupMenuEnd(C, pup);

	return true;
}

static int ui_do_button(bContext *C, uiBlock *block, uiBut *but, const wmEvent *event)
{
	uiHandleButtonData *data;
	int retval;

	data = but->active;
	retval = WM_UI_HANDLER_CONTINUE;

	if (but->flag & UI_BUT_DISABLED)
		return WM_UI_HANDLER_CONTINUE;

	if ((data->state == BUTTON_STATE_HIGHLIGHT) || (event->type == EVT_DROP)) {
		/* handle copy-paste */
		if (ELEM(event->type, CKEY, VKEY) && event->val == KM_PRESS && (event->ctrl || event->oskey)) {
			/* Specific handling for listrows, we try to find their overlapping tex button. */
			if (but->type == LISTROW) {
				uiBut *labelbut = ui_but_list_row_text_activate(C, but, data, event, BUTTON_ACTIVATE_OVER);
				if (labelbut) {
					but = labelbut;
					data = but->active;
				}
			}

			ui_but_copy_paste(C, but, data, (event->type == CKEY) ? 'c' : 'v');
			return WM_UI_HANDLER_BREAK;
		}
		/* handle drop */
		else if (event->type == EVT_DROP) {
			ui_but_drop(C, event, but, data);
		}
		/* handle eyedropper */
		else if ((event->type == EKEY) && (event->val == KM_PRESS)) {
			if (event->alt || event->shift || event->ctrl || event->oskey) {
				/* pass */
			}
			else {
				if (but->type == COLOR) {
					WM_operator_name_call(C, "UI_OT_eyedropper_color", WM_OP_INVOKE_DEFAULT, NULL);
					return WM_UI_HANDLER_BREAK;
				}
				else if (but->type == SEARCH_MENU_UNLINK) {
					if (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_POINTER) {
						StructRNA *type = RNA_property_pointer_type(&but->rnapoin, but->rnaprop);
						const short idcode = RNA_type_to_ID_code(type);
						if ((idcode == ID_OB) || OB_DATA_SUPPORT_ID(idcode)) {
							WM_operator_name_call(C, "UI_OT_eyedropper_id", WM_OP_INVOKE_DEFAULT, NULL);
							return WM_UI_HANDLER_BREAK;
						}
					}
				}
			}
		}
		/* handle keyframing */
		else if ((event->type == IKEY) &&
		         !ELEM(KM_MOD_FIRST, event->ctrl, event->oskey) &&
		         (event->val == KM_PRESS))
		{
			if (event->alt) {
				if (event->shift) {
					ui_but_anim_clear_keyframe(C);
				}
				else {
					ui_but_anim_delete_keyframe(C);
				}
			}
			else {
				ui_but_anim_insert_keyframe(C);
			}
			
			ED_region_tag_redraw(data->region);
			
			return WM_UI_HANDLER_BREAK;
		}
		/* handle drivers */
		else if ((event->type == DKEY) &&
		         !ELEM(KM_MOD_FIRST, event->ctrl, event->oskey, event->shift) &&
		         (event->val == KM_PRESS))
		{
			if (event->alt)
				ui_but_anim_remove_driver(C);
			else
				ui_but_anim_add_driver(C);
				
			ED_region_tag_redraw(data->region);
			
			return WM_UI_HANDLER_BREAK;
		}
		/* handle keyingsets */
		else if ((event->type == KKEY) &&
		         !ELEM(KM_MOD_FIRST, event->ctrl, event->oskey, event->shift) &&
		         (event->val == KM_PRESS))
		{
			if (event->alt)
				ui_but_anim_remove_keyingset(C);
			else
				ui_but_anim_add_keyingset(C);
				
			ED_region_tag_redraw(data->region);
			
			return WM_UI_HANDLER_BREAK;
		}
		/* handle menu */
		else if (event->type == RIGHTMOUSE && event->val == KM_PRESS) {
			/* RMB has two options now */
			if (ui_but_menu(C, but)) {
				return WM_UI_HANDLER_BREAK;
			}
		}
	}

	/* verify if we can edit this button */
	if (ELEM(event->type, LEFTMOUSE, RETKEY)) {
		/* this should become disabled button .. */
		if (but->lock == true) {
			if (but->lockstr) {
				WM_report(C, RPT_INFO, but->lockstr);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				return WM_UI_HANDLER_BREAK;
			}
		}
		else if (but->pointype && but->poin == NULL) {
			/* there's a pointer needed */
			BKE_reportf(NULL, RPT_WARNING, "DoButton pointer error: %s", but->str);
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}

	switch (but->type) {
		case BUT:
			retval = ui_do_but_BUT(C, but, data, event);
			break;
		case KEYEVT:
			retval = ui_do_but_KEYEVT(C, but, data, event);
			break;
		case HOTKEYEVT:
			retval = ui_do_but_HOTKEYEVT(C, but, data, event);
			break;
		case TOGBUT:
		case TOG:
		case ICONTOG:
		case ICONTOGN:
		case TOGN:
		case OPTION:
		case OPTIONN:
			retval = ui_do_but_TOG(C, but, data, event);
			break;
		case SCROLL:
			retval = ui_do_but_SCROLL(C, block, but, data, event);
			break;
		case GRIP:
			retval = ui_do_but_GRIP(C, block, but, data, event);
			break;
		case NUM:
			retval = ui_do_but_NUM(C, block, but, data, event);
			break;
		case NUMSLI:
			retval = ui_do_but_SLI(C, block, but, data, event);
			break;
		case LISTBOX:
			/* Nothing to do! */
			break;
		case LISTROW:
			retval = ui_do_but_LISTROW(C, but, data, event);
			break;
		case ROUNDBOX:
		case LABEL:
		case ROW:
		case BUT_IMAGE:
		case PROGRESSBAR:
		case NODESOCKET:
			retval = ui_do_but_EXIT(C, but, data, event);
			break;
		case HISTOGRAM:
			retval = ui_do_but_HISTOGRAM(C, block, but, data, event);
			break;
		case WAVEFORM:
			retval = ui_do_but_WAVEFORM(C, block, but, data, event);
			break;
		case VECTORSCOPE:
			/* Nothing to do! */
			break;
		case TEX:
		case SEARCH_MENU:
			retval = ui_do_but_TEX(C, block, but, data, event);
			break;
		case SEARCH_MENU_UNLINK:
			retval = ui_do_but_SEARCH_UNLINK(C, block, but, data, event);
			break;
		case MENU:
		case BLOCK:
		case PULLDOWN:
			retval = ui_do_but_BLOCK(C, but, data, event);
			break;
		case BUTM:
			retval = ui_do_but_BUT(C, but, data, event);
			break;
		case COLOR:
			if (but->a1 == -1)  /* signal to prevent calling up color picker */
				retval = ui_do_but_EXIT(C, but, data, event);
			else
				retval = ui_do_but_COLOR(C, but, data, event);
			break;
		case BUT_NORMAL:
			retval = ui_do_but_NORMAL(C, block, but, data, event);
			break;
		case BUT_COLORBAND:
			retval = ui_do_but_COLORBAND(C, block, but, data, event);
			break;
		case BUT_CURVE:
			retval = ui_do_but_CURVE(C, block, but, data, event);
			break;
		case HSVCUBE:
			retval = ui_do_but_HSVCUBE(C, block, but, data, event);
			break;
		case HSVCIRCLE:
			retval = ui_do_but_HSVCIRCLE(C, block, but, data, event);
			break;
		case LINK:
		case INLINK:
			retval = ui_do_but_LINK(C, but, data, event);
			break;
		case TRACKPREVIEW:
			retval = ui_do_but_TRACKPREVIEW(C, block, but, data, event);
			break;

			/* quiet warnings for unhandled types */
		case SEPR:
		case SEPRLINE:
		case BUT_EXTRA:
			break;
	}


	/* reset to default (generic function, only use if not handled by switch above) */
	/* XXX hardcoded keymap check.... */
	data = but->active;
	if (data && data->state == BUTTON_STATE_HIGHLIGHT) {
		if ((retval == WM_UI_HANDLER_CONTINUE) &&
		    (event->type == BACKSPACEKEY && event->val == KM_PRESS))
		{
			/* ctrl+backspace = reset active button; backspace = reset a whole array*/
			ui_set_but_default(C, !event->ctrl, true);
			ED_region_tag_redraw(data->region);
			retval = WM_UI_HANDLER_BREAK;
		}
	}

#ifdef USE_DRAG_MULTINUM
	if (data) {
		if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE) ||
		    /* if we started dragging, progress on any event */
		    (data->multi_data.init == BUTTON_MULTI_INIT_SETUP))
		{
			if (ELEM(but->type, NUM, NUMSLI) &&
			    ELEM(data->state, BUTTON_STATE_TEXT_EDITING, BUTTON_STATE_NUM_EDITING))
			{
				/* initialize! */
				if (data->multi_data.init == BUTTON_MULTI_INIT_UNSET) {
					/* --> (BUTTON_MULTI_INIT_SETUP | BUTTON_MULTI_INIT_DISABLE) */

					const float margin_y = DRAG_MULTINUM_THRESHOLD_DRAG_Y / sqrtf(block->aspect);

					/* check if we have a vertical gesture */
					if (len_squared_v2(data->multi_data.drag_dir) > (margin_y * margin_y)) {
						const float dir_nor_y[2] = {0.0, 1.0f};
						float dir_nor_drag[2];

						normalize_v2_v2(dir_nor_drag, data->multi_data.drag_dir);

						if (fabsf(dot_v2v2(dir_nor_drag, dir_nor_y)) > DRAG_MULTINUM_THRESHOLD_VERTICAL) {
							data->multi_data.init = BUTTON_MULTI_INIT_SETUP;
							data->multi_data.drag_lock_x = event->x;
						}
						else {
							data->multi_data.init = BUTTON_MULTI_INIT_DISABLE;
						}
					}
				}
				else if (data->multi_data.init == BUTTON_MULTI_INIT_SETUP) {
					/* --> (BUTTON_MULTI_INIT_ENABLE) */
					const float margin_x = DRAG_MULTINUM_THRESHOLD_DRAG_X / sqrtf(block->aspect);
					/* check if we're dont setting buttons */
					if ((data->str && ELEM(data->state, BUTTON_STATE_TEXT_EDITING, BUTTON_STATE_NUM_EDITING)) ||
					    ((abs(data->multi_data.drag_lock_x - event->x) > margin_x) &&
					     /* just to be sure, check we're dragging more hoz then virt */
					     abs(event->prevx - event->x) > abs(event->prevy - event->y)))
					{
						ui_multibut_states_create(but, data);
						data->multi_data.init = BUTTON_MULTI_INIT_ENABLE;
					}
				}

				if (data->multi_data.init == BUTTON_MULTI_INIT_SETUP) {
					if (ui_multibut_states_tag(but, data, event)) {
						ED_region_tag_redraw(data->region);
					}
				}
			}
		}
	}
#endif  /* USE_DRAG_MULTINUM */

	return retval;
}

/* ************************ button utilities *********************** */

static bool ui_but_contains_pt(uiBut *but, float mx, float my)
{
	return BLI_rctf_isect_pt(&but->rect, mx, my);
}

uiBut *ui_but_find_activated(ARegion *ar)
{
	uiBlock *block;
	uiBut *but;

	for (block = ar->uiblocks.first; block; block = block->next)
		for (but = block->buttons.first; but; but = but->next)
			if (but->active)
				return but;

	return NULL;
}

bool ui_button_is_active(ARegion *ar)
{
	return (ui_but_find_activated(ar) != NULL);
}

/* is called by notifier */
void uiFreeActiveButtons(const bContext *C, bScreen *screen)
{
	ScrArea *sa = screen->areabase.first;
	
	for (; sa; sa = sa->next) {
		ARegion *ar = sa->regionbase.first;
		for (; ar; ar = ar->next) {
			uiBut *but = ui_but_find_activated(ar);
			if (but) {
				uiHandleButtonData *data = but->active;
				
				if (data->menu == NULL && data->searchbox == NULL)
					if (data->state == BUTTON_STATE_HIGHLIGHT)
						ui_button_active_free(C, but);
			}
		}
	}
}



/* returns true if highlighted button allows drop of names */
/* called in region context */
bool UI_but_active_drop_name(bContext *C)
{
	ARegion *ar = CTX_wm_region(C);
	uiBut *but = ui_but_find_activated(ar);

	if (but) {
		if (ELEM(but->type, TEX, SEARCH_MENU, SEARCH_MENU_UNLINK))
			return 1;
	}
	
	return 0;
}

bool UI_but_active_drop_color(bContext *C)
{
	ARegion *ar = CTX_wm_region(C);

	if (ar) {
		uiBut *but = ui_but_find_activated(ar);

		if (but && but->type == COLOR)
			return true;
	}

	return false;
}

static void ui_blocks_set_tooltips(ARegion *ar, const bool enable)
{
	uiBlock *block;

	if (!ar)
		return;

	/* we disabled buttons when when they were already shown, and
	 * re-enable them on mouse move */
	for (block = ar->uiblocks.first; block; block = block->next)
		block->tooltipdisabled = !enable;
}

static bool ui_mouse_inside_region(ARegion *ar, int x, int y)
{
	uiBlock *block;
	
	/* check if the mouse is in the region */
	if (!BLI_rcti_isect_pt(&ar->winrct, x, y)) {
		for (block = ar->uiblocks.first; block; block = block->next)
			block->auto_open = false;
		
		return false;
	}

	/* also, check that with view2d, that the mouse is not over the scrollbars 
	 * NOTE: care is needed here, since the mask rect may include the scrollbars
	 * even when they are not visible, so we need to make a copy of the mask to
	 * use to check
	 */
	if (ar->v2d.mask.xmin != ar->v2d.mask.xmax) {
		View2D *v2d = &ar->v2d;
		int mx, my;
		
		/* convert window coordinates to region coordinates */
		mx = x;
		my = y;
		ui_window_to_region(ar, &mx, &my);

		/* check if in the rect */
		if (!BLI_rcti_isect_pt(&v2d->mask, mx, my))
			return false;
	}
	
	return true;
}

static bool ui_mouse_inside_button(ARegion *ar, uiBut *but, int x, int y)
{
	float mx, my;
	if (!ui_mouse_inside_region(ar, x, y))
		return false;

	mx = x;
	my = y;

	ui_window_to_block_fl(ar, but->block, &mx, &my);

	if (!ui_but_contains_pt(but, mx, my))
		return false;
	
	return true;
}

/**
 * Can we mouse over the button or is it hidden/disabled/layout.
 * Note: ctrl is kind of a hack currently, so that non-embossed TEX button behaves as a label when ctrl is not pressed.
 */
static bool ui_is_but_interactive(const uiBut *but, const bool labeledit)
{
	/* note, LABEL is included for highlights, this allows drags */
	if ((but->type == LABEL) && but->dragpoin == NULL)
		return false;
	if (ELEM(but->type, ROUNDBOX, SEPR, SEPRLINE, LISTBOX))
		return false;
	if (but->flag & UI_HIDDEN)
		return false;
	if (but->flag & UI_SCROLLED)
		return false;
	if ((but->type == TEX) && (but->dt == UI_EMBOSSN) && !labeledit)
		return false;
	if ((but->type == LISTROW) && labeledit)
		return false;

	return true;
}

bool ui_is_but_search_unlink_visible(const uiBut *but)
{
	BLI_assert(but->type == SEARCH_MENU_UNLINK);
	return ((but->editstr == NULL) &&
	        (but->drawstr[0] != '\0'));
}

/* x and y are only used in case event is NULL... */
static uiBut *ui_but_find_mouse_over_ex(ARegion *ar, const int x, const int y, const bool labeledit)
{
	uiBlock *block;
	uiBut *but, *butover = NULL;
	float mx, my;

//	if (!win->active)
//		return NULL;
	if (!ui_mouse_inside_region(ar, x, y))
		return NULL;

	for (block = ar->uiblocks.first; block; block = block->next) {
		mx = x;
		my = y;
		ui_window_to_block_fl(ar, block, &mx, &my);

		for (but = block->buttons.last; but; but = but->prev) {
			if (ui_is_but_interactive(but, labeledit)) {
				if (ui_but_contains_pt(but, mx, my)) {
					butover = but;
					break;
				}
			}
		}

		/* CLIP_EVENTS prevents the event from reaching other blocks */
		if (block->flag & UI_BLOCK_CLIP_EVENTS) {
			/* check if mouse is inside block */
			if (BLI_rctf_isect_pt(&block->rect, mx, my)) {
				break;
			}
		}
	}

	return butover;
}

static uiBut *ui_but_find_mouse_over(ARegion *ar, const wmEvent *event)
{
	return ui_but_find_mouse_over_ex(ar, event->x, event->y, event->ctrl != 0);
}


static uiBut *ui_list_find_mouse_over(ARegion *ar, int x, int y)
{
	uiBlock *block;
	uiBut *but;
	float mx, my;

	if (!ui_mouse_inside_region(ar, x, y))
		return NULL;

	for (block = ar->uiblocks.first; block; block = block->next) {
		mx = x;
		my = y;
		ui_window_to_block_fl(ar, block, &mx, &my);

		for (but = block->buttons.last; but; but = but->prev) {
			if (but->type == LISTBOX && ui_but_contains_pt(but, mx, my)) {
				return but;
			}
		}
	}

	return NULL;
}

/* ****************** button state handling **************************/

static bool button_modal_state(uiHandleButtonState state)
{
	return ELEM(state,
	            BUTTON_STATE_WAIT_RELEASE,
	            BUTTON_STATE_WAIT_KEY_EVENT,
	            BUTTON_STATE_NUM_EDITING,
	            BUTTON_STATE_TEXT_EDITING,
	            BUTTON_STATE_TEXT_SELECTING,
	            BUTTON_STATE_MENU_OPEN);
}

static void button_timers_tooltip_remove(bContext *C, uiBut *but)
{
	uiHandleButtonData *data;

	data = but->active;
	if (data) {

		if (data->tooltiptimer) {
			WM_event_remove_timer(data->wm, data->window, data->tooltiptimer);
			data->tooltiptimer = NULL;
		}
		if (data->tooltip) {
			ui_tooltip_free(C, data->tooltip);
			data->tooltip = NULL;
		}

		if (data->autoopentimer) {
			WM_event_remove_timer(data->wm, data->window, data->autoopentimer);
			data->autoopentimer = NULL;
		}
	}
}

static void button_tooltip_timer_reset(bContext *C, uiBut *but)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	uiHandleButtonData *data;

	data = but->active;

	if (data->tooltiptimer) {
		WM_event_remove_timer(data->wm, data->window, data->tooltiptimer);
		data->tooltiptimer = NULL;
	}

	if (U.flag & USER_TOOLTIPS)
		if (!but->block->tooltipdisabled)
			if (!wm->drags.first)
				data->tooltiptimer = WM_event_add_timer(data->wm, data->window, TIMER, BUTTON_TOOLTIP_DELAY);
}

static void button_activate_state(bContext *C, uiBut *but, uiHandleButtonState state)
{
	uiHandleButtonData *data;

	data = but->active;
	if (data->state == state)
		return;

	/* highlight has timers for tooltips and auto open */
	if (state == BUTTON_STATE_HIGHLIGHT) {
		but->flag &= ~UI_SELECT;

		button_tooltip_timer_reset(C, but);

		/* automatic open pulldown block timer */
		if (ELEM(but->type, BLOCK, PULLDOWN)) {
			if (data->used_mouse && !data->autoopentimer) {
				int time;

				if (but->block->auto_open == true) {  /* test for toolbox */
					time = 1;
				}
				else if ((but->block->flag & UI_BLOCK_LOOP && but->type != BLOCK) || but->block->auto_open == true) {
					time = 5 * U.menuthreshold2;
				}
				else if (U.uiflag & USER_MENUOPENAUTO) {
					time = 5 * U.menuthreshold1;
				}
				else {
					time = -1;  /* do nothing */
				}

				if (time >= 0) {
					data->autoopentimer = WM_event_add_timer(data->wm, data->window, TIMER, 0.02 * (double)time);
				}
			}
		}
	}
	else {
		but->flag |= UI_SELECT;
		button_timers_tooltip_remove(C, but);
	}
	
	/* text editing */
	if (state == BUTTON_STATE_TEXT_EDITING && data->state != BUTTON_STATE_TEXT_SELECTING)
		ui_textedit_begin(C, but, data);
	else if (data->state == BUTTON_STATE_TEXT_EDITING && state != BUTTON_STATE_TEXT_SELECTING)
		ui_textedit_end(C, but, data);
	else if (data->state == BUTTON_STATE_TEXT_SELECTING && state != BUTTON_STATE_TEXT_EDITING)
		ui_textedit_end(C, but, data);
	
	/* number editing */
	if (state == BUTTON_STATE_NUM_EDITING) {
		if (ui_is_a_warp_but(but))
			WM_cursor_grab_enable(CTX_wm_window(C), true, true, NULL);
		ui_numedit_begin(but, data);
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		ui_numedit_end(but, data);

		if (but->flag & UI_BUT_DRIVEN)
			WM_report(C, RPT_INFO, "Can't edit driven number value, see graph editor for the driver setup.");

		if (ui_is_a_warp_but(but)) {

#ifdef USE_CONT_MOUSE_CORRECT
			if (data->ungrab_mval[0] != FLT_MAX) {
				int mouse_ungrab_xy[2];
				ui_block_to_window_fl(data->region, but->block, &data->ungrab_mval[0], &data->ungrab_mval[1]);
				mouse_ungrab_xy[0] = data->ungrab_mval[0];
				mouse_ungrab_xy[1] = data->ungrab_mval[1];

				WM_cursor_grab_disable(data->window, mouse_ungrab_xy);
			}
			else {
				WM_cursor_grab_disable(data->window, NULL);
			}
#else
			WM_cursor_grab_disable(data->window, NULL);
#endif
		}
	}
	/* menu open */
	if (state == BUTTON_STATE_MENU_OPEN)
		ui_blockopen_begin(C, but, data);
	else if (data->state == BUTTON_STATE_MENU_OPEN)
		ui_blockopen_end(C, but, data);

	/* add a short delay before exiting, to ensure there is some feedback */
	if (state == BUTTON_STATE_WAIT_FLASH) {
		data->flashtimer = WM_event_add_timer(data->wm, data->window, TIMER, BUTTON_FLASH_DELAY);
	}
	else if (data->flashtimer) {
		WM_event_remove_timer(data->wm, data->window, data->flashtimer);
		data->flashtimer = NULL;
	}

	/* add a blocking ui handler at the window handler for blocking, modal states
	 * but not for popups, because we already have a window level handler*/
	if (!(but->block->handle && but->block->handle->popup)) {
		if (button_modal_state(state)) {
			if (!button_modal_state(data->state))
				WM_event_add_ui_handler(C, &data->window->modalhandlers, ui_handler_region_menu, NULL, data);
		}
		else {
			if (button_modal_state(data->state)) {
				/* true = postpone free */
				WM_event_remove_ui_handler(&data->window->modalhandlers, ui_handler_region_menu, NULL, data, true);
			}
		}
	}
	
	/* wait for mousemove to enable drag */
	if (state == BUTTON_STATE_WAIT_DRAG) {
		but->flag &= ~UI_SELECT;
	}

	data->state = state;

	if (state != BUTTON_STATE_EXIT) {
		/* When objects for eg. are removed, running ui_check_but() can access
		 * the removed data - so disable update on exit. Also in case of
		 * highlight when not in a popup menu, we remove because data used in
		 * button below popup might have been removed by action of popup. Needs
		 * a more reliable solution... */
		if (state != BUTTON_STATE_HIGHLIGHT || (but->block->flag & UI_BLOCK_LOOP))
			ui_check_but(but);
	}

	/* redraw */
	ED_region_tag_redraw(data->region);
}

static void button_activate_init(bContext *C, ARegion *ar, uiBut *but, uiButtonActivateType type)
{
	uiHandleButtonData *data;

	/* setup struct */
	data = MEM_callocN(sizeof(uiHandleButtonData), "uiHandleButtonData");
	data->wm = CTX_wm_manager(C);
	data->window = CTX_wm_window(C);
	data->region = ar;

#ifdef USE_CONT_MOUSE_CORRECT
	copy_v2_fl(data->ungrab_mval, FLT_MAX);
#endif

	if (ELEM(but->type, BUT_CURVE, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
		/* XXX curve is temp */
	}
	else {
		data->interactive = true;
	}
	
	data->state = BUTTON_STATE_INIT;

	/* activate button */
	but->flag |= UI_ACTIVE;
	but->active = data;

	/* we disable auto_open in the block after a threshold, because we still
	 * want to allow auto opening adjacent menus even if no button is activated
	 * in between going over to the other button, but only for a short while */
	if (type == BUTTON_ACTIVATE_OVER && but->block->auto_open == true)
		if (but->block->auto_open_last + BUTTON_AUTO_OPEN_THRESH < PIL_check_seconds_timer())
			but->block->auto_open = false;

	if (type == BUTTON_ACTIVATE_OVER) {
		data->used_mouse = true;
	}
	button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);
	
	/* activate right away */
	if (but->flag & UI_BUT_IMMEDIATE) {
		if (but->type == HOTKEYEVT)
			button_activate_state(C, but, BUTTON_STATE_WAIT_KEY_EVENT);
		/* .. more to be added here */
	}
	
	if (type == BUTTON_ACTIVATE_OPEN) {
		button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);

		/* activate first button in submenu */
		if (data->menu && data->menu->region) {
			ARegion *subar = data->menu->region;
			uiBlock *subblock = subar->uiblocks.first;
			uiBut *subbut;
			
			if (subblock) {
				subbut = ui_but_first(subblock);

				if (subbut)
					ui_handle_button_activate(C, subar, subbut, BUTTON_ACTIVATE);
			}
		}
	}
	else if (type == BUTTON_ACTIVATE_TEXT_EDITING)
		button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
	else if (type == BUTTON_ACTIVATE_APPLY)
		button_activate_state(C, but, BUTTON_STATE_WAIT_FLASH);

	if (but->type == GRIP) {
		const bool horizontal = (BLI_rctf_size_x(&but->rect) < BLI_rctf_size_y(&but->rect));
		WM_cursor_modal_set(data->window, horizontal ? CURSOR_X_MOVE : CURSOR_Y_MOVE);
	}
}

static void button_activate_exit(bContext *C, uiBut *but, uiHandleButtonData *data,
                                 const bool mousemove, const bool onfree)
{
	uiBlock *block = but->block;
	uiBut *bt;

	if (but->type == GRIP) {
		WM_cursor_modal_restore(data->window);
	}

	/* ensure we are in the exit state */
	if (data->state != BUTTON_STATE_EXIT)
		button_activate_state(C, but, BUTTON_STATE_EXIT);

	/* apply the button action or value */
	if (!onfree)
		ui_apply_button(C, block, but, data, false);

#ifdef USE_DRAG_MULTINUM
	if (data->multi_data.has_mbuts) {
		for (bt = block->buttons.first; bt; bt = bt->next) {
			if (bt->flag & UI_BUT_DRAG_MULTI) {
				bt->flag &= ~UI_BUT_DRAG_MULTI;

				if (!data->cancel) {
					ui_apply_autokey(C, bt);
				}
			}
		}

		ui_multibut_free(data, block);
	}
#endif

	/* if this button is in a menu, this will set the button return
	 * value to the button value and the menu return value to ok, the
	 * menu return value will be picked up and the menu will close */
	if (block->handle && !(block->flag & UI_BLOCK_KEEP_OPEN)) {
		if (!data->cancel || data->escapecancel) {
			uiPopupBlockHandle *menu;

			menu = block->handle;
			menu->butretval = data->retval;
			menu->menuretval = (data->cancel) ? UI_RETURN_CANCEL : UI_RETURN_OK;
		}
	}

	if (!onfree && !data->cancel) {
		/* autokey & undo push */
		ui_apply_undo(but);
		ui_apply_autokey(C, but);

		/* popup menu memory */
		if (block->flag & UI_BLOCK_POPUP_MEMORY)
			ui_popup_menu_memory_set(block, but);
	}

	/* disable tooltips until mousemove + last active flag */
	for (block = data->region->uiblocks.first; block; block = block->next) {
		for (bt = block->buttons.first; bt; bt = bt->next)
			bt->flag &= ~UI_BUT_LAST_ACTIVE;

		block->tooltipdisabled = 1;
	}

	ui_blocks_set_tooltips(data->region, false);

	/* clean up */
	if (data->str)
		MEM_freeN(data->str);
	if (data->origstr)
		MEM_freeN(data->origstr);

	/* redraw (data is but->active!) */
	ED_region_tag_redraw(data->region);

	/* clean up button */
	if (but->active) {
		MEM_freeN(but->active);
		but->active = NULL;
	}

	but->flag &= ~(UI_ACTIVE | UI_SELECT);
	but->flag |= UI_BUT_LAST_ACTIVE;
	if (!onfree)
		ui_check_but(but);

	/* adds empty mousemove in queue for re-init handler, in case mouse is
	 * still over a button. we cannot just check for this ourselfs because
	 * at this point the mouse may be over a button in another region */
	if (mousemove)
		WM_event_add_mousemove(C);
}

void ui_button_active_free(const bContext *C, uiBut *but)
{
	uiHandleButtonData *data;

	/* this gets called when the button somehow disappears while it is still
	 * active, this is bad for user interaction, but we need to handle this
	 * case cleanly anyway in case it happens */
	if (but->active) {
		data = but->active;
		data->cancel = true;
		button_activate_exit((bContext *)C, but, data, false, true);
	}
}

/* returns the active button with an optional checking function */
static uiBut *ui_context_button_active(const bContext *C, bool (*but_check_cb)(uiBut *))
{
	uiBut *but_found = NULL;

	ARegion *ar = CTX_wm_region(C);

	while (ar) {
		uiBlock *block;
		uiBut *but, *activebut = NULL;

		/* find active button */
		for (block = ar->uiblocks.first; block; block = block->next) {
			for (but = block->buttons.first; but; but = but->next) {
				if (but->active)
					activebut = but;
				else if (!activebut && (but->flag & UI_BUT_LAST_ACTIVE))
					activebut = but;
			}
		}

		if (activebut && (but_check_cb == NULL || but_check_cb(activebut))) {
			uiHandleButtonData *data = activebut->active;

			but_found = activebut;

			/* recurse into opened menu, like colorpicker case */
			if (data && data->menu && (ar != data->menu->region)) {
				ar = data->menu->region;
			}
			else {
				return but_found;
			}
		}
		else {
			/* no active button */
			return but_found;
		}
	}

	return but_found;
}

static bool ui_context_rna_button_active_test(uiBut *but)
{
	return (but->rnapoin.data != NULL);
}
static uiBut *ui_context_rna_button_active(const bContext *C)
{
	return ui_context_button_active(C, ui_context_rna_button_active_test);
}

uiBut *uiContextActiveButton(const struct bContext *C)
{
	return ui_context_button_active(C, NULL);
}

/* helper function for insert keyframe, reset to default, etc operators */
void uiContextActiveProperty(const bContext *C, struct PointerRNA *ptr, struct PropertyRNA **prop, int *index)
{
	uiBut *activebut = ui_context_rna_button_active(C);

	memset(ptr, 0, sizeof(*ptr));

	if (activebut && activebut->rnapoin.data) {
		*ptr = activebut->rnapoin;
		*prop = activebut->rnaprop;
		*index = activebut->rnaindex;
	}
	else {
		*prop = NULL;
		*index = 0;
	}
}

void uiContextActivePropertyHandle(bContext *C)
{
	uiBut *activebut = ui_context_rna_button_active(C);
	if (activebut) {
		/* TODO, look into a better way to handle the button change
		 * currently this is mainly so reset defaults works for the
		 * operator redo panel - campbell */
		uiBlock *block = activebut->block;
		if (block->handle_func) {
			block->handle_func(C, block->handle_func_arg, activebut->retval);
		}
	}
}

wmOperator *uiContextActiveOperator(const struct bContext *C)
{
	ARegion *ar_ctx = CTX_wm_region(C);
	uiBlock *block;

	/* background mode */
	if (ar_ctx == NULL) {
		return NULL;
	}

	/* scan active regions ui */
	for (block = ar_ctx->uiblocks.first; block; block = block->next) {
		if (block->ui_operator) {
			return block->ui_operator;
		}
	}

	/* scan popups */
	{
		bScreen *sc = CTX_wm_screen(C);
		ARegion *ar;

		for (ar = sc->regionbase.first; ar; ar = ar->next) {
			if (ar == ar_ctx) {
				continue;
			}
			for (block = ar->uiblocks.first; block; block = block->next) {
				if (block->ui_operator) {
					return block->ui_operator;
				}
			}
		}
	}

	return NULL;
}

/* helper function for insert keyframe, reset to default, etc operators */
void uiContextAnimUpdate(const bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	uiBlock *block;
	uiBut *but, *activebut;

	while (ar) {
		/* find active button */
		activebut = NULL;

		for (block = ar->uiblocks.first; block; block = block->next) {
			for (but = block->buttons.first; but; but = but->next) {
				ui_but_anim_flag(but, (scene) ? scene->r.cfra : 0.0f);
				ED_region_tag_redraw(ar);
				
				if (but->active) {
					activebut = but;
				}
				else if (!activebut && (but->flag & UI_BUT_LAST_ACTIVE)) {
					activebut = but;
				}
			}
		}

		if (activebut) {
			/* always recurse into opened menu, so all buttons update (like colorpicker) */
			uiHandleButtonData *data = activebut->active;
			if (data && data->menu) {
				ar = data->menu->region;
			}
			else {
				return;
			}
		}
		else {
			/* no active button */
			return;
		}
	}
}

/************** handle activating a button *************/

static uiBut *uit_but_find_open_event(ARegion *ar, const wmEvent *event)
{
	uiBlock *block;
	uiBut *but;
	
	for (block = ar->uiblocks.first; block; block = block->next) {
		for (but = block->buttons.first; but; but = but->next)
			if (but == event->customdata)
				return but;
	}
	return NULL;
}

static int ui_handle_button_over(bContext *C, const wmEvent *event, ARegion *ar)
{
	uiBut *but;

	if (event->type == MOUSEMOVE) {
		but = ui_but_find_mouse_over(ar, event);
		if (but) {
			button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
		}
	}
	else if (event->type == EVT_BUT_OPEN) {
		but = uit_but_find_open_event(ar, event);
		if (but) {
			button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
			ui_do_button(C, but->block, but, event);
		}
	}

	return WM_UI_HANDLER_CONTINUE;
}

/* exported to interface.c: uiButActiveOnly() */
void ui_button_activate_do(bContext *C, ARegion *ar, uiBut *but)
{
	wmWindow *win = CTX_wm_window(C);
	wmEvent event;
	
	button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
	
	wm_event_init_from_window(win, &event);
	event.type = EVT_BUT_OPEN;
	event.val = KM_PRESS;
	event.customdata = but;
	event.customdatafree = false;
	
	ui_do_button(C, but->block, but, &event);
}

/**
 * Simulate moving the mouse over a button (or navigating to it with arrow keys).
 *
 * exported so menus can start with a highlighted button,
 * even if the mouse isnt over it
 */
void ui_button_activate_over(bContext *C, ARegion *ar, uiBut *but)
{
	button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
}

void ui_button_execute_begin(struct bContext *UNUSED(C), struct ARegion *ar, uiBut *but, void **active_back)
{
	/* note: ideally we would not have to change 'but->active' however
	 * some functions we call don't use data (as they should be doing) */
	uiHandleButtonData *data;
	*active_back = but->active;
	data = MEM_callocN(sizeof(uiHandleButtonData), "uiHandleButtonData_Fake");
	but->active = data;
	data->region = ar;
}

void ui_button_execute_end(struct bContext *C, struct ARegion *UNUSED(ar), uiBut *but, void *active_back)
{
	ui_apply_button(C, but->block, but, but->active, true);

	if ((but->flag & UI_BUT_DRAG_MULTI) == 0) {
		ui_apply_autokey(C, but);
	}
	/* use onfree event so undo is handled by caller and apply is already done above */
	button_activate_exit((bContext *)C, but, but->active, false, true);
	but->active = active_back;
}

static void ui_handle_button_activate(bContext *C, ARegion *ar, uiBut *but, uiButtonActivateType type)
{
	uiBut *oldbut;
	uiHandleButtonData *data;

	oldbut = ui_but_find_activated(ar);
	if (oldbut) {
		data = oldbut->active;
		data->cancel = true;
		button_activate_exit(C, oldbut, data, false, false);
	}

	button_activate_init(C, ar, but, type);
}

/************ handle events for an activated button ***********/

static int ui_handle_button_event(bContext *C, const wmEvent *event, uiBut *but)
{
	uiHandleButtonData *data = but->active;
	const uiHandleButtonState state_orig = data->state;
	uiBlock *block;
	ARegion *ar;
	int retval;

	block = but->block;
	ar = data->region;

	retval = WM_UI_HANDLER_CONTINUE;
	
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		switch (event->type) {
			case WINDEACTIVATE:
			case EVT_BUT_CANCEL:
				data->cancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_CONTINUE;
				break;
			case MOUSEMOVE:
			{
				uiBut *but_other = ui_but_find_mouse_over(ar, event);
				bool exit = false;

				if (!ui_block_is_menu(block) &&
				    !ui_mouse_inside_button(ar, but, event->x, event->y))
				{
					exit = true;
				}
				else if (but_other && ui_but_is_editable(but_other) && (but_other != but)) {
					exit = true;
				}

				if (exit) {
					data->cancel = true;
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				else if (event->x != event->prevx || event->y != event->prevy) {
					/* re-enable tooltip on mouse move */
					ui_blocks_set_tooltips(ar, true);
					button_tooltip_timer_reset(C, but);
				}

				break;
			}
			case TIMER:
			{
				/* handle tooltip timer */
				if (event->customdata == data->tooltiptimer) {
					WM_event_remove_timer(data->wm, data->window, data->tooltiptimer);
					data->tooltiptimer = NULL;

					if (!data->tooltip)
						data->tooltip = ui_tooltip_create(C, data->region, but);
				}
				/* handle menu auto open timer */
				else if (event->customdata == data->autoopentimer) {
					WM_event_remove_timer(data->wm, data->window, data->autoopentimer);
					data->autoopentimer = NULL;

					if (ui_mouse_inside_button(ar, but, event->x, event->y))
						button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
				}

				retval = WM_UI_HANDLER_CONTINUE;
				break;
			}
				/* XXX hardcoded keymap check... but anyway, while view changes, tooltips should be removed */
			case WHEELUPMOUSE:
			case WHEELDOWNMOUSE:
			case MIDDLEMOUSE:
			case MOUSEPAN:
				button_timers_tooltip_remove(C, but);
				/* fall-through */
			default:
				/* handle button type specific events */
				retval = ui_do_button(C, block, but, event);
				break;
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_RELEASE) {
		switch (event->type) {
			case WINDEACTIVATE:
				data->cancel = true;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				break;

			case MOUSEMOVE:
				if (ELEM(but->type, LINK, INLINK)) {
					ARegion *ar = data->region;
					but->flag |= UI_SELECT;
					ui_do_button(C, block, but, event);
					ED_region_tag_redraw(ar);
				}
				else {
					/* deselect the button when moving the mouse away */
					/* also de-activate for buttons that only show higlights */
					if (ui_mouse_inside_button(ar, but, event->x, event->y)) {
						if (!(but->flag & UI_SELECT)) {
							but->flag |= (UI_SELECT | UI_ACTIVE);
							data->cancel = false;
							ED_region_tag_redraw(data->region);
						}
					}
					else {
						if (but->flag & UI_SELECT) {
							but->flag &= ~(UI_SELECT | UI_ACTIVE);
							data->cancel = true;
							ED_region_tag_redraw(data->region);
						}
					}
				}
				break;
			default:
				/* otherwise catch mouse release event */
				ui_do_button(C, block, but, event);
				break;
		}

		retval = WM_UI_HANDLER_BREAK;
	}
	else if (data->state == BUTTON_STATE_WAIT_FLASH) {
		switch (event->type) {
			case TIMER:
			{
				if (event->customdata == data->flashtimer) {
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				break;
			}
		}

		retval = WM_UI_HANDLER_CONTINUE;
	}
	else if (data->state == BUTTON_STATE_MENU_OPEN) {
		/* check for exit because of mouse-over another button */
		switch (event->type) {
			case MOUSEMOVE:
			{
				uiBut *bt;

				if (data->menu && data->menu->region) {
					if (ui_mouse_inside_region(data->menu->region, event->x, event->y)) {
						break;
					}
				}

				bt = ui_but_find_mouse_over(ar, event);
				
				if (bt && bt->active != data) {
					if (but->type != COLOR) {  /* exception */
						data->cancel = true;
					}
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				break;
			}
		}

		ui_do_button(C, block, but, event);
		retval = WM_UI_HANDLER_CONTINUE;
	}
	else {
		retval = ui_do_button(C, block, but, event);
		// retval = WM_UI_HANDLER_BREAK; XXX why ?
	}

	/* may have been re-allocated above (eyedropper for eg) */
	data = but->active;
	if (data && data->state == BUTTON_STATE_EXIT) {
		uiBut *post_but = data->postbut;
		uiButtonActivateType post_type = data->posttype;

		button_activate_exit(C, but, data, (post_but == NULL), false);

		/* for jumping to the next button with tab while text editing */
		if (post_but) {
			button_activate_init(C, ar, post_but, post_type);
		}
		else {
			/* XXX issue is because WM_event_add_mousemove(C) is a bad hack and not reliable,
			 * if that gets coded better this bypass can go away too.
			 *
			 * This is needed to make sure if a button was active,
			 * it stays active while the mouse is over it.
			 * This avoids adding mousemoves, see: [#33466] */
			if (ELEM(state_orig, BUTTON_STATE_INIT, BUTTON_STATE_HIGHLIGHT)) {
				if (ui_but_find_mouse_over(ar, event) == but) {
					button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
				}
			}
		}
	}

	return retval;
}

static int ui_handle_list_event(bContext *C, const wmEvent *event, ARegion *ar)
{
	uiBut *but;
	uiList *ui_list;
	uiListDyn *dyn_data;
	int retval = WM_UI_HANDLER_CONTINUE;
	int type = event->type, val = event->val;
	int mx, my;

	but = ui_list_find_mouse_over(ar, event->x, event->y);
	if (!but) {
		return retval;
	}

	ui_list = but->custom_data;
	if (!ui_list || !ui_list->dyn_data) {
		return retval;
	}
	dyn_data = ui_list->dyn_data;

	mx = event->x;
	my = event->y;
	ui_window_to_block(ar, but->block, &mx, &my);

	/* convert pan to scrollwheel */
	if (type == MOUSEPAN) {
		ui_pan_to_scroll(event, &type, &val);

		/* if type still is mousepan, we call it handled, since delta-y accumulate */
		/* also see wm_event_system.c do_wheel_ui hack */
		if (type == MOUSEPAN)
			retval = WM_UI_HANDLER_BREAK;
	}

	if (val == KM_PRESS) {
		if (ELEM(type, UPARROWKEY, DOWNARROWKEY) ||
		    ((ELEM(type, WHEELUPMOUSE, WHEELDOWNMOUSE) && event->alt)))
		{
			const int value_orig = RNA_property_int_get(&but->rnapoin, but->rnaprop);
			int value, min, max, inc;

			/* activate up/down the list */
			value = value_orig;
			if ((ui_list->filter_sort_flag & UILST_FLT_SORT_REVERSE) != 0) {
				inc = ELEM(type, UPARROWKEY, WHEELUPMOUSE) ? 1 : -1;
			}
			else {
				inc = ELEM(type, UPARROWKEY, WHEELUPMOUSE) ? -1 : 1;
			}

			if (dyn_data->items_filter_neworder || dyn_data->items_filter_flags) {
				/* If we have a display order different from collection order, we have some work! */
				int *org_order = MEM_mallocN(dyn_data->items_shown * sizeof(int), __func__);
				const int *new_order = dyn_data->items_filter_neworder;
				int i, org_idx = -1, len = dyn_data->items_len;
				int current_idx = -1;
				int filter_exclude = ui_list->filter_flag & UILST_FLT_EXCLUDE;

				for (i = 0; i < len; i++) {
					if (!dyn_data->items_filter_flags ||
					    ((dyn_data->items_filter_flags[i] & UILST_FLT_ITEM) ^ filter_exclude))
					{
						org_order[new_order ? new_order[++org_idx] : ++org_idx] = i;
						if (i == value) {
							current_idx = new_order ? new_order[org_idx] : org_idx;
						}
					}
					else if (i == value && org_idx >= 0) {
						current_idx = -(new_order ? new_order[org_idx] : org_idx) - 1;
					}
				}
				/* Now, org_order maps displayed indices to real indices,
				 * and current_idx either contains the displayed index of active value (positive),
				 *                 or its more-nearest one (negated).
				 */
				if (current_idx < 0) {
					current_idx = (current_idx * -1) + (inc < 0 ? inc : inc - 1);
				}
				else {
					current_idx += inc;
				}
				CLAMP(current_idx, 0, dyn_data->items_shown - 1);
				value = org_order[current_idx];
				MEM_freeN(org_order);
			}
			else {
				value += inc;
			}

			CLAMP(value, 0, dyn_data->items_len - 1);

			RNA_property_int_range(&but->rnapoin, but->rnaprop, &min, &max);
			CLAMP(value, min, max);

			if (value != value_orig) {
				RNA_property_int_set(&but->rnapoin, but->rnaprop, value);
				RNA_property_update(C, &but->rnapoin, but->rnaprop);

				ui_apply_undo(but);

				ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;
				ED_region_tag_redraw(ar);
			}
			retval = WM_UI_HANDLER_BREAK;
		}
		else if (ELEM(type, WHEELUPMOUSE, WHEELDOWNMOUSE) && event->shift) {
			/* We now have proper grip, but keep this anyway! */
			if (ui_list->list_grip < (dyn_data->visual_height_min - UI_LIST_AUTO_SIZE_THRESHOLD)) {
				ui_list->list_grip = dyn_data->visual_height;
			}
			ui_list->list_grip += (type == WHEELUPMOUSE) ? -1 : 1;

			ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;
			ED_region_tag_redraw(ar);

			retval = WM_UI_HANDLER_BREAK;
		}
		else if (ELEM(type, WHEELUPMOUSE, WHEELDOWNMOUSE)) {
			if (dyn_data->height > dyn_data->visual_height) {
				/* list template will clamp */
				ui_list->list_scroll += (type == WHEELUPMOUSE) ? -1 : 1;

				ED_region_tag_redraw(ar);

				retval = WM_UI_HANDLER_BREAK;
			}
		}
	}

	return retval;
}

static void ui_handle_button_return_submenu(bContext *C, const wmEvent *event, uiBut *but)
{
	uiHandleButtonData *data;
	uiPopupBlockHandle *menu;

	data = but->active;
	menu = data->menu;

	/* copy over return values from the closing menu */
	if ((menu->menuretval & UI_RETURN_OK) || (menu->menuretval & UI_RETURN_UPDATE)) {
		if (but->type == COLOR)
			copy_v3_v3(data->vec, menu->retvec);
		else if (but->type == MENU)
			data->value = menu->retvalue;
	}

	if (menu->menuretval & UI_RETURN_UPDATE) {
		if (data->interactive) {
			ui_apply_button(C, but->block, but, data, true);
		}
		else {
			ui_check_but(but);
		}

		menu->menuretval = 0;
	}
	
	/* now change button state or exit, which will close the submenu */
	if ((menu->menuretval & UI_RETURN_OK) || (menu->menuretval & UI_RETURN_CANCEL)) {
		if (menu->menuretval != UI_RETURN_OK)
			data->cancel = true;

		button_activate_exit(C, but, data, true, false);
	}
	else if (menu->menuretval & UI_RETURN_OUT) {
		if (event->type == MOUSEMOVE && ui_mouse_inside_button(data->region, but, event->x, event->y)) {
			button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);
		}
		else {
			if (ISKEYBOARD(event->type)) {
				/* keyboard menu hierarchy navigation, going back to previous level */
				but->active->used_mouse = false;
				button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);
			}
			else {
				data->cancel = true;
				button_activate_exit(C, but, data, true, false);
			}
		}
	}
}

/* ************************* menu handling *******************************/

/* function used to prevent loosing the open menu when using nested pulldowns,
 * when moving mouse towards the pulldown menu over other buttons that could
 * steal the highlight from the current button, only checks:
 *
 * - while mouse moves in triangular area defined old mouse position and
 *   left/right side of new menu
 * - only for 1 second
 */

static void ui_mouse_motion_towards_init_ex(uiPopupBlockHandle *menu, const int xy[2], const bool force)
{
	BLI_assert(((uiBlock *)menu->region->uiblocks.first)->flag & UI_BLOCK_MOVEMOUSE_QUIT);

	if (!menu->dotowards || force) {
		menu->dotowards = true;
		menu->towards_xy[0] = xy[0];
		menu->towards_xy[1] = xy[1];

		if (force)
			menu->towardstime = DBL_MAX;  /* unlimited time */
		else
			menu->towardstime = PIL_check_seconds_timer();
	}
}

static void ui_mouse_motion_towards_init(uiPopupBlockHandle *menu, const int xy[2])
{
	ui_mouse_motion_towards_init_ex(menu, xy, false);
}

static void ui_mouse_motion_towards_reinit(uiPopupBlockHandle *menu, const int xy[2])
{
	ui_mouse_motion_towards_init_ex(menu, xy, true);
}

static bool ui_mouse_motion_towards_check(uiBlock *block, uiPopupBlockHandle *menu, const int xy[2],
                                          const bool use_wiggle_room)
{
	float p1[2], p2[2], p3[2], p4[2];
	float oldp[2] = {menu->towards_xy[0], menu->towards_xy[1]};
	const float newp[2] = {xy[0], xy[1]};
	bool closer;
	const float margin = MENU_TOWARDS_MARGIN;
	rctf rect_px;

	BLI_assert(block->flag & UI_BLOCK_MOVEMOUSE_QUIT);


	/* annoying fix for [#36269], this is a bit odd but in fact works quite well
	 * don't mouse-out of a menu if another menu has been created after it.
	 * if this causes problems we could remove it and check on a different fix - campbell */
	if (menu->region->next) {
		/* am I the last menu (test) */
		ARegion *ar = menu->region->next;
		do {
			uiBlock *block_iter = ar->uiblocks.first;
			if (block_iter && ui_block_is_menu(block_iter)) {
				return true;
			}
		} while ((ar = ar->next));
	}
	/* annoying fix end! */


	if (!menu->dotowards) {
		return false;
	}

	if (len_squared_v2v2(oldp, newp) < (4.0f * 4.0f))
		return menu->dotowards;

	/* verify that we are moving towards one of the edges of the
	 * menu block, in other words, in the triangle formed by the
	 * initial mouse location and two edge points. */
	ui_block_to_window_rctf(menu->region, block, &rect_px, &block->rect);

	p1[0] = rect_px.xmin - margin;
	p1[1] = rect_px.ymin - margin;

	p2[0] = rect_px.xmax + margin;
	p2[1] = rect_px.ymin - margin;
	
	p3[0] = rect_px.xmax + margin;
	p3[1] = rect_px.ymax + margin;

	p4[0] = rect_px.xmin - margin;
	p4[1] = rect_px.ymax + margin;

	/* allow for some wiggle room, if the user moves a few pixels away,
	 * don't immediately quit (only for top level menus) */
	if (use_wiggle_room) {
		const float cent[2] = {
		    BLI_rctf_cent_x(&rect_px),
		    BLI_rctf_cent_y(&rect_px)};
		float delta[2];

		sub_v2_v2v2(delta, oldp, cent);
		normalize_v2(delta);
		mul_v2_fl(delta, MENU_TOWARDS_WIGGLE_ROOM);
		add_v2_v2(oldp, delta);
	}

	closer = (isect_point_tri_v2(newp, oldp, p1, p2) ||
	          isect_point_tri_v2(newp, oldp, p2, p3) ||
	          isect_point_tri_v2(newp, oldp, p3, p4) ||
	          isect_point_tri_v2(newp, oldp, p4, p1));

	if (!closer)
		menu->dotowards = false;

	/* 1 second timer */
	if (PIL_check_seconds_timer() - menu->towardstime > BUTTON_MOUSE_TOWARDS_THRESH)
		menu->dotowards = false;

	return menu->dotowards;
}

#ifdef USE_KEYNAV_LIMIT
static void ui_mouse_motion_keynav_init(struct uiKeyNavLock *keynav, const wmEvent *event)
{
	keynav->is_keynav = true;
	copy_v2_v2_int(keynav->event_xy, &event->x);
}
/**
 * Return true if keyinput isn't blocking mouse-motion,
 * or if the mouse-motion is enough to disable keyinput.
 */
static bool ui_mouse_motion_keynav_test(struct uiKeyNavLock *keynav, const wmEvent *event)
{
	if (keynav->is_keynav && (len_manhattan_v2v2_int(keynav->event_xy, &event->x) > BUTTON_KEYNAV_PX_LIMIT)) {
		keynav->is_keynav = false;
	}

	return keynav->is_keynav;
}
#endif  /* USE_KEYNAV_LIMIT */

static char ui_menu_scroll_test(uiBlock *block, int my)
{
	if (block->flag & (UI_BLOCK_CLIPTOP | UI_BLOCK_CLIPBOTTOM)) {
		if (block->flag & UI_BLOCK_CLIPTOP) 
			if (my > block->rect.ymax - UI_MENU_SCROLL_MOUSE)
				return 't';
		if (block->flag & UI_BLOCK_CLIPBOTTOM)
			if (my < block->rect.ymin + UI_MENU_SCROLL_MOUSE)
				return 'b';
	}
	return 0;
}

static int ui_menu_scroll(ARegion *ar, uiBlock *block, int my, uiBut *to_bt)
{
	uiBut *bt;
	float dy = 0.0f;

	if (to_bt) {
		/* scroll to activated button */
		if (block->flag & UI_BLOCK_CLIPTOP) {
			if (to_bt->rect.ymax > block->rect.ymax - UI_MENU_SCROLL_ARROW)
				dy = block->rect.ymax - to_bt->rect.ymax - UI_MENU_SCROLL_ARROW;
		}
		if (block->flag & UI_BLOCK_CLIPBOTTOM) {
			if (to_bt->rect.ymin < block->rect.ymin + UI_MENU_SCROLL_ARROW)
				dy = block->rect.ymin - to_bt->rect.ymin + UI_MENU_SCROLL_ARROW;
		}
	}
	else {
		/* scroll when mouse over arrow buttons */
		char test = ui_menu_scroll_test(block, my);

		if (test == 't')
			dy = -UI_UNIT_Y; /* scroll to the top */
		else if (test == 'b')
			dy = UI_UNIT_Y; /* scroll to the bottom */
	}

	if (dy != 0.0f) {
		if (dy < 0.0f) {
			/* stop at top item, extra 0.5 unit Y makes it snap nicer */
			float ymax = -FLT_MAX;

			for (bt = block->buttons.first; bt; bt = bt->next)
				ymax = max_ff(ymax, bt->rect.ymax);

			if (ymax + dy - UI_UNIT_Y * 0.5f < block->rect.ymax - UI_MENU_SCROLL_PAD)
				dy = block->rect.ymax - ymax - UI_MENU_SCROLL_PAD;
		}
		else {
			/* stop at bottom item, extra 0.5 unit Y makes it snap nicer */
			float ymin = FLT_MAX;

			for (bt = block->buttons.first; bt; bt = bt->next)
				ymin = min_ff(ymin, bt->rect.ymin);

			if (ymin + dy + UI_UNIT_Y * 0.5f > block->rect.ymin + UI_MENU_SCROLL_PAD)
				dy = block->rect.ymin - ymin + UI_MENU_SCROLL_PAD;
		}

		/* apply scroll offset */
		for (bt = block->buttons.first; bt; bt = bt->next) {
			bt->rect.ymin += dy;
			bt->rect.ymax += dy;
		}

		/* set flags again */
		ui_popup_block_scrolltest(block);
		
		ED_region_tag_redraw(ar);
		
		return 1;
	}
	
	return 0;
}

/**
 * Special function to handle nested menus.
 * let the parent menu get the event.
 *
 * This allows a menu to be open,
 * but send key events to the parent if theres no active buttons.
 *
 * Without this keyboard navigation from menu's wont work.
 */
static bool ui_menu_pass_event_to_parent_if_nonactive(uiPopupBlockHandle *menu, const uiBut *but,
                                                      const int level, const int retval)
{
	if ((level != 0) && (but == NULL)) {
		menu->menuretval = UI_RETURN_OUT | UI_RETURN_OUT_PARENT;
		(void) retval;  /* so release builds with strict flags are happy as well */
		BLI_assert(retval == WM_UI_HANDLER_CONTINUE);
		return true;
	}
	else {
		return false;
	}
}

static int ui_handle_menu_button(bContext *C, const wmEvent *event, uiPopupBlockHandle *menu)
{
	ARegion *ar = menu->region;
	uiBut *but = ui_but_find_activated(ar);
	int retval;

	if (but) {
		/* Its possible there is an active menu item NOT under the mouse,
		 * in this case ignore mouse clicks outside the button (but Enter etc is accepted) */
		if (event->val == KM_RELEASE) {
			/* pass, needed so we can exit active menu-items when click-dragging out of them */
		}
		else if (!ui_mouse_inside_region(but->active->region, event->x, event->y)) {
			/* pass, needed to click-exit outside of non-flaoting menus */
		}
		else if ((event->type != MOUSEMOVE) && ISMOUSE(event->type)) {
			if (!ui_mouse_inside_button(but->active->region, but, event->x, event->y)) {
				but = NULL;
			}
		}
	}

	if (but) {
		ScrArea *ctx_area = CTX_wm_area(C);
		ARegion *ctx_region = CTX_wm_region(C);

		if (menu->ctx_area) CTX_wm_area_set(C, menu->ctx_area);
		if (menu->ctx_region) CTX_wm_region_set(C, menu->ctx_region);

		retval = ui_handle_button_event(C, event, but);

		if (menu->ctx_area) CTX_wm_area_set(C, ctx_area);
		if (menu->ctx_region) CTX_wm_region_set(C, ctx_region);
	}
	else {
		retval = ui_handle_button_over(C, event, ar);
	}

	return retval;
}

static int ui_handle_menu_event(
        bContext *C, const wmEvent *event, uiPopupBlockHandle *menu,
        int level, const bool is_parent_inside, const bool is_parent_menu, const bool is_floating)
{
	ARegion *ar;
	uiBlock *block;
	uiBut *but, *bt;
	int mx, my, retval;
	bool inside;
	bool inside_title;  /* check for title dragging */

	ar = menu->region;
	block = ar->uiblocks.first;

	retval = WM_UI_HANDLER_CONTINUE;

	mx = event->x;
	my = event->y;
	ui_window_to_block(ar, block, &mx, &my);

	/* check if mouse is inside block */
	inside = BLI_rctf_isect_pt(&block->rect, mx, my);
	inside_title = inside && ((my + (UI_UNIT_Y * 1.5f)) > block->rect.ymax);

	/* if there's an active modal button, don't check events or outside, except for search menu */
	but = ui_but_find_activated(ar);

#ifdef USE_DRAG_POPUP
	if (menu->is_grab) {
		if (event->type == LEFTMOUSE) {
			menu->is_grab = false;
			retval = WM_UI_HANDLER_BREAK;
		}
		else {
			if (event->type == MOUSEMOVE) {
				int mdiff[2];

				sub_v2_v2v2_int(mdiff, &event->x, menu->grab_xy_prev);
				copy_v2_v2_int(menu->grab_xy_prev, &event->x);

				add_v2_v2v2_int(menu->popup_create_vars.event_xy, menu->popup_create_vars.event_xy, mdiff);

				ui_popup_translate(C, ar, mdiff);
			}

			return retval;
		}
	}
#endif

	if (but && button_modal_state(but->active->state)) {
		if (block->flag & UI_BLOCK_MOVEMOUSE_QUIT) {
			/* if a button is activated modal, always reset the start mouse
			 * position of the towards mechanism to avoid loosing focus,
			 * and don't handle events */
			ui_mouse_motion_towards_reinit(menu, &event->x);
		}
	}
	else if (event->type == TIMER) {
		if (event->customdata == menu->scrolltimer)
			ui_menu_scroll(ar, block, my, NULL);
	}
	else {
		/* for ui_mouse_motion_towards_block */
		if (event->type == MOUSEMOVE) {
			if (block->flag & UI_BLOCK_MOVEMOUSE_QUIT) {
				ui_mouse_motion_towards_init(menu, &event->x);
			}
			
			/* add menu scroll timer, if needed */
			if (ui_menu_scroll_test(block, my))
				if (menu->scrolltimer == NULL)
					menu->scrolltimer =
					    WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, MENU_SCROLL_INTERVAL);
		}
		
		/* first block own event func */
		if (block->block_event_func && block->block_event_func(C, block, event)) {
			/* pass */
		}   /* events not for active search menu button */
		else {
			int act = 0;

			switch (event->type) {

				/* closing sublevels of pulldowns */
				case LEFTARROWKEY:
					if (event->val == KM_PRESS && (block->flag & UI_BLOCK_LOOP))
						if (block->saferct.first)
							menu->menuretval = UI_RETURN_OUT;

					retval = WM_UI_HANDLER_BREAK;
					break;

				/* opening sublevels of pulldowns */
				case RIGHTARROWKEY:
					if (event->val == KM_PRESS && (block->flag & UI_BLOCK_LOOP)) {

						if (ui_menu_pass_event_to_parent_if_nonactive(menu, but, level, retval))
							break;

						but = ui_but_find_activated(ar);

						if (!but) {
							/* no item active, we make first active */
							if (block->direction & UI_TOP) but = ui_but_last(block);
							else but = ui_but_first(block);
						}

						if (but && ELEM(but->type, BLOCK, PULLDOWN))
							ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE_OPEN);
					}

					retval = WM_UI_HANDLER_BREAK;
					break;
				
				case UPARROWKEY:
				case DOWNARROWKEY:
				case WHEELUPMOUSE:
				case WHEELDOWNMOUSE:
				case MOUSEPAN:
					/* arrowkeys: only handle for block_loop blocks */
					if (event->alt || event->shift || event->ctrl || event->oskey) {
						/* pass */
					}
					else if (inside || (block->flag & UI_BLOCK_LOOP)) {
						int type = event->type;
						int val = event->val;
						
						/* convert pan to scrollwheel */
						if (type == MOUSEPAN)
							ui_pan_to_scroll(event, &type, &val);
						
						if (val == KM_PRESS) {
							const eButType type_flip = BUT | ROW;

							if (ui_menu_pass_event_to_parent_if_nonactive(menu, but, level, retval))
								break;

#ifdef USE_KEYNAV_LIMIT
							ui_mouse_motion_keynav_init(&menu->keynav_state, event);
#endif

							but = ui_but_find_activated(ar);
							if (but) {
								/* is there a situation where UI_LEFT or UI_RIGHT would also change navigation direction? */
								if (((ELEM(type, DOWNARROWKEY, WHEELDOWNMOUSE)) && (block->direction & UI_DOWN)) ||
								    ((ELEM(type, DOWNARROWKEY, WHEELDOWNMOUSE)) && (block->direction & UI_RIGHT)) ||
								    ((ELEM(type, UPARROWKEY, WHEELUPMOUSE)) && (block->direction & UI_TOP)))
								{
									/* the following is just a hack - uiBut->type set to BUT and BUTM have there menus built 
									 * opposite ways - this should be changed so that all popup-menus use the same uiBlock->direction */
									if (but->type & type_flip)
										but = ui_but_next(but);
									else
										but = ui_but_prev(but);
								}
								else {
									if (but->type & type_flip)
										but = ui_but_prev(but);
									else
										but = ui_but_next(but);
								}

								if (but) {
									ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE);
									ui_menu_scroll(ar, block, my, but);
								}
							}

							if (!but) {
								if (((ELEM(type, UPARROWKEY, WHEELUPMOUSE)) && (block->direction & UI_DOWN)) ||
								    ((ELEM(type, UPARROWKEY, WHEELUPMOUSE)) && (block->direction & UI_RIGHT)) ||
								    ((ELEM(type, DOWNARROWKEY, WHEELDOWNMOUSE)) && (block->direction & UI_TOP)))
								{
									if ((bt = ui_but_first(block)) && (bt->type & type_flip)) {
										bt = ui_but_last(block);
									}
									else {
										/* keep ui_but_first() */
									}
								}
								else {
									if ((bt = ui_but_first(block)) && (bt->type & type_flip)) {
										/* keep ui_but_first() */
									}
									else {
										bt = ui_but_last(block);
									}
								}

								if (bt) {
									ui_handle_button_activate(C, ar, bt, BUTTON_ACTIVATE);
									ui_menu_scroll(ar, block, my, bt);
								}
							}
						}

						retval = WM_UI_HANDLER_BREAK;
					}

					break;

				case ONEKEY:    case PAD1:
					act = 1;
				case TWOKEY:    case PAD2:
					if (act == 0) act = 2;
				case THREEKEY:  case PAD3:
					if (act == 0) act = 3;
				case FOURKEY:   case PAD4:
					if (act == 0) act = 4;
				case FIVEKEY:   case PAD5:
					if (act == 0) act = 5;
				case SIXKEY:    case PAD6:
					if (act == 0) act = 6;
				case SEVENKEY:  case PAD7:
					if (act == 0) act = 7;
				case EIGHTKEY:  case PAD8:
					if (act == 0) act = 8;
				case NINEKEY:   case PAD9:
					if (act == 0) act = 9;
				case ZEROKEY:   case PAD0:
					if (act == 0) act = 10;

					if ((block->flag & UI_BLOCK_NUMSELECT) && event->val == KM_PRESS) {
						int count;

						if (ui_menu_pass_event_to_parent_if_nonactive(menu, but, level, retval))
							break;

						if (event->alt) act += 10;

						count = 0;
						for (but = block->buttons.first; but; but = but->next) {
							bool doit = false;
							
							if (!ELEM(but->type, LABEL, SEPR, SEPRLINE))
								count++;
							
							/* exception for rna layer buts */
							if (but->rnapoin.data && but->rnaprop &&
							    ELEM(RNA_property_subtype(but->rnaprop), PROP_LAYER, PROP_LAYER_MEMBER))
							{
								if (but->rnaindex == act - 1) {
									doit = true;
								}
							}
							else if (count == act) {
								doit = true;
							}

							if (doit) {
								/* activate buttons but open menu's */
								uiButtonActivateType activate;
								if (but->type == PULLDOWN) {
									activate = BUTTON_ACTIVATE_OPEN;
								}
								else {
									activate = BUTTON_ACTIVATE_APPLY;
								}

								ui_handle_button_activate(C, ar, but, activate);
								break;
							}
						}

						retval = WM_UI_HANDLER_BREAK;
					}
					break;

				/* Handle keystrokes on menu items */
				case AKEY:
				case BKEY:
				case CKEY:
				case DKEY:
				case EKEY:
				case FKEY:
				case GKEY:
				case HKEY:
				case IKEY:
				case JKEY:
				case KKEY:
				case LKEY:
				case MKEY:
				case NKEY:
				case OKEY:
				case PKEY:
				case QKEY:
				case RKEY:
				case SKEY:
				case TKEY:
				case UKEY:
				case VKEY:
				case WKEY:
				case XKEY:
				case YKEY:
				case ZKEY:
				{
					if ((event->val  == KM_PRESS || event->val == KM_DBL_CLICK) &&
					    (event->shift == 0) &&
					    (event->ctrl  == 0) &&
					    (event->oskey == 0))
					{
						if (ui_menu_pass_event_to_parent_if_nonactive(menu, but, level, retval))
							break;

						for (but = block->buttons.first; but; but = but->next) {

							if (but->menu_key == event->type) {
								if (ELEM(but->type, BUT, BUTM)) {
									/* mainly for operator buttons */
									ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE_APPLY);
								}
								else if (ELEM(but->type, BLOCK, PULLDOWN)) {
									/* open submenus (like right arrow key) */
									ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE_OPEN);
								}
								else if (but->type == MENU) {
									/* activate menu items */
									ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE);
								}
								else {
									printf("%s: error, but->menu_key type: %d\n", __func__, but->type);
								}

								break;
							}
						}

						retval = WM_UI_HANDLER_BREAK;
					}
					break;
				}
			}
		}
		
		/* here we check return conditions for menus */
		if (block->flag & UI_BLOCK_LOOP) {
			/* if we click outside the block, verify if we clicked on the
			 * button that opened us, otherwise we need to close,
			 *
			 * note that there is an exception for root level menus and
			 * popups which you can click again to close.
			 */
			if (inside == 0) {
				uiSafetyRct *saferct = block->saferct.first;

				if (ELEM(event->type, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE) &&
				    ELEM(event->val, KM_PRESS, KM_DBL_CLICK))
				{
					if ((is_parent_menu == false) && (U.uiflag & USER_MENUOPENAUTO) == 0) {
						/* for root menus, allow clicking to close */
						if (block->flag & (UI_BLOCK_OUT_1))
							menu->menuretval = UI_RETURN_OK;
						else
							menu->menuretval = UI_RETURN_OUT;
					}
					else if (saferct && !BLI_rctf_isect_pt(&saferct->parent, event->x, event->y)) {
						if (block->flag & (UI_BLOCK_OUT_1))
							menu->menuretval = UI_RETURN_OK;
						else
							menu->menuretval = UI_RETURN_OUT;
					}
				}
			}

			if (menu->menuretval) {
				/* pass */
			}
#ifdef USE_KEYNAV_LIMIT
			else if ((event->type == MOUSEMOVE) && ui_mouse_motion_keynav_test(&menu->keynav_state, event)) {
				/* don't handle the mousemove if we're using key-navigation */
				retval = WM_UI_HANDLER_BREAK;
			}
#endif
			else if (event->type == ESCKEY && event->val == KM_PRESS) {
				/* esc cancels this and all preceding menus */
				menu->menuretval = UI_RETURN_CANCEL;
			}
			else if (ELEM(event->type, RETKEY, PADENTER) && event->val == KM_PRESS) {
				/* enter will always close this block, we let the event
				 * get handled by the button if it is activated, otherwise we cancel */
				if (!ui_but_find_activated(ar))
					menu->menuretval = UI_RETURN_CANCEL | UI_RETURN_POPUP_OK;
			}
#ifdef USE_DRAG_POPUP
			else if ((event->type == LEFTMOUSE) && (event->val == KM_PRESS) &&
			         (inside && is_floating && inside_title))
			{
				if (!but || !ui_mouse_inside_button(ar, but, event->x, event->y)) {
					if (but) {
						button_timers_tooltip_remove(C, but);
					}

					menu->is_grab = true;
					copy_v2_v2_int(menu->grab_xy_prev, &event->x);
					retval = WM_UI_HANDLER_BREAK;
				}
			}
#endif
			else {

				/* check mouse moving outside of the menu */
				if (inside == 0 && (block->flag & UI_BLOCK_MOVEMOUSE_QUIT)) {
					uiSafetyRct *saferct;

					ui_mouse_motion_towards_check(block, menu, &event->x, is_parent_inside == false);
					
					/* check for all parent rects, enables arrowkeys to be used */
					for (saferct = block->saferct.first; saferct; saferct = saferct->next) {
						/* for mouse move we only check our own rect, for other
						 * events we check all preceding block rects too to make
						 * arrow keys navigation work */
						if (event->type != MOUSEMOVE || saferct == block->saferct.first) {
							if (BLI_rctf_isect_pt(&saferct->parent, (float)event->x, (float)event->y))
								break;
							if (BLI_rctf_isect_pt(&saferct->safety, (float)event->x, (float)event->y))
								break;
						}
					}

					/* strict check, and include the parent rect */
					if (!menu->dotowards && !saferct) {
						if (block->flag & (UI_BLOCK_OUT_1))
							menu->menuretval = UI_RETURN_OK;
						else
							menu->menuretval = UI_RETURN_OUT;
					}
					else if (menu->dotowards && event->type == MOUSEMOVE)
						retval = WM_UI_HANDLER_BREAK;
				}
			}

			/* end switch */
		}
	}

	/* if we are didn't handle the event yet, lets pass it on to
	 * buttons inside this region. disabled inside check .. not sure
	 * anymore why it was there? but it meant enter didn't work
	 * for example when mouse was not over submenu */
	if ((event->type == TIMER) ||
	    (/*inside &&*/ (!menu->menuretval || (menu->menuretval & UI_RETURN_UPDATE)) && retval == WM_UI_HANDLER_CONTINUE))
	{
		retval = ui_handle_menu_button(C, event, menu);
	}

	/* if we set a menu return value, ensure we continue passing this on to
	 * lower menus and buttons, so always set continue then, and if we are
	 * inside the region otherwise, ensure we swallow the event */
	if (menu->menuretval)
		return WM_UI_HANDLER_CONTINUE;
	else if (inside)
		return WM_UI_HANDLER_BREAK;
	else
		return retval;
}

static int ui_handle_menu_return_submenu(bContext *C, const wmEvent *event, uiPopupBlockHandle *menu)
{
	ARegion *ar;
	uiBut *but;
	uiBlock *block;
	uiHandleButtonData *data;
	uiPopupBlockHandle *submenu;

	ar = menu->region;
	block = ar->uiblocks.first;

	but = ui_but_find_activated(ar);
	data = but->active;
	submenu = data->menu;

	if (submenu->menuretval) {
		bool update;

		/* first decide if we want to close our own menu cascading, if
		 * so pass on the sub menu return value to our own menu handle */
		if ((submenu->menuretval & UI_RETURN_OK) || (submenu->menuretval & UI_RETURN_CANCEL)) {
			if (!(block->flag & UI_BLOCK_KEEP_OPEN)) {
				menu->menuretval = submenu->menuretval;
				menu->butretval = data->retval;
			}
		}

		update = (submenu->menuretval & UI_RETURN_UPDATE) != 0;

		/* now let activated button in this menu exit, which
		 * will actually close the submenu too */
		ui_handle_button_return_submenu(C, event, but);

		if (update)
			submenu->menuretval = 0;
	}

	if (block->flag & UI_BLOCK_MOVEMOUSE_QUIT) {
		/* for cases where close does not cascade, allow the user to
		 * move the mouse back towards the menu without closing */
		ui_mouse_motion_towards_reinit(menu, &event->x);
	}

	if (menu->menuretval)
		return WM_UI_HANDLER_CONTINUE;
	else
		return WM_UI_HANDLER_BREAK;
}

static int ui_handle_menus_recursive(
        bContext *C, const wmEvent *event, uiPopupBlockHandle *menu,
        int level, const bool is_parent_inside, const bool is_parent_menu, const bool is_floating)
{
	uiBut *but;
	uiHandleButtonData *data;
	uiPopupBlockHandle *submenu;
	int retval = WM_UI_HANDLER_CONTINUE;
	bool do_towards_reinit = false;

	/* check if we have a submenu, and handle events for it first */
	but = ui_but_find_activated(menu->region);
	data = (but) ? but->active : NULL;
	submenu = (data) ? data->menu : NULL;

	if (submenu) {
		uiBlock *block = menu->region->uiblocks.first;
		const bool is_menu = ui_block_is_menu(block);
		bool inside = false;

		if (is_parent_inside == false) {
			int mx, my;

			mx = event->x;
			my = event->y;
			ui_window_to_block(menu->region, block, &mx, &my);
			inside = BLI_rctf_isect_pt(&block->rect, mx, my);
		}

		retval = ui_handle_menus_recursive(C, event, submenu, level + 1, is_parent_inside || inside, is_menu, false);
	}

	/* now handle events for our own menu */
	if (retval == WM_UI_HANDLER_CONTINUE || event->type == TIMER) {
		const bool do_but_search = (but && ELEM(but->type, SEARCH_MENU, SEARCH_MENU_UNLINK));
		if (submenu && submenu->menuretval) {
			const bool do_ret_out_parent = (submenu->menuretval & UI_RETURN_OUT_PARENT) != 0;
			retval = ui_handle_menu_return_submenu(C, event, menu);
			submenu = NULL;  /* hint not to use this, it may be freed by call above */
			(void)submenu;
			/* we may want to quit the submenu and handle the even in this menu,
			 * if its important to use it, check 'data->menu' first */
			if (((retval == WM_UI_HANDLER_BREAK) && do_ret_out_parent) == 0) {
				/* skip applying the event */
				return retval;
			}
		}

		if (do_but_search) {
			uiBlock *block = menu->region->uiblocks.first;

			retval = ui_handle_menu_button(C, event, menu);

			if (block->flag & UI_BLOCK_MOVEMOUSE_QUIT) {
				/* when there is a active search button and we close it,
				 * we need to reinit the mouse coords [#35346] */
				if (ui_but_find_activated(menu->region) != but) {
					do_towards_reinit = true;
				}
			}
		}
		else {
			retval = ui_handle_menu_event(C, event, menu, level, is_parent_inside, is_parent_menu, is_floating);
		}
	}

	if (do_towards_reinit) {
		ui_mouse_motion_towards_reinit(menu, &event->x);
	}

	return retval;
}

/* *************** UI event handlers **************** */

static int ui_handler_region(bContext *C, const wmEvent *event, void *UNUSED(userdata))
{
	ARegion *ar;
	uiBut *but;
	int retval;

	/* here we handle buttons at the region level, non-modal */
	ar = CTX_wm_region(C);
	retval = WM_UI_HANDLER_CONTINUE;

	if (ar == NULL || BLI_listbase_is_empty(&ar->uiblocks)) {
		return retval;
	}

	/* either handle events for already activated button or try to activate */
	but = ui_but_find_activated(ar);

	retval = ui_handler_panel_region(C, event, ar);

	if (retval == WM_UI_HANDLER_CONTINUE)
		retval = ui_handle_list_event(C, event, ar);

	if (retval == WM_UI_HANDLER_CONTINUE) {
		if (but)
			retval = ui_handle_button_event(C, event, but);
		else
			retval = ui_handle_button_over(C, event, ar);
	}

	/* re-enable tooltips */
	if (event->type == MOUSEMOVE && (event->x != event->prevx || event->y != event->prevy))
		ui_blocks_set_tooltips(ar, true);
	
	/* delayed apply callbacks */
	ui_apply_but_funcs_after(C);

	return retval;
}

static void ui_handler_remove_region(bContext *C, void *UNUSED(userdata))
{
	bScreen *sc;
	ARegion *ar;

	ar = CTX_wm_region(C);
	if (ar == NULL) return;

	uiFreeBlocks(C, &ar->uiblocks);
	
	sc = CTX_wm_screen(C);
	if (sc == NULL) return;

	/* delayed apply callbacks, but not for screen level regions, those
	 * we rather do at the very end after closing them all, which will
	 * be done in ui_handler_region/window */
	if (BLI_findindex(&sc->regionbase, ar) == -1)
		ui_apply_but_funcs_after(C);
}

static int ui_handler_region_menu(bContext *C, const wmEvent *event, void *UNUSED(userdata))
{
	ARegion *ar;
	uiBut *but;

	/* here we handle buttons at the window level, modal, for example
	 * while number sliding, text editing, or when a menu block is open */
	ar = CTX_wm_menu(C);
	if (!ar)
		ar = CTX_wm_region(C);

	but = ui_but_find_activated(ar);

	if (but) {
		uiBut *but_other;
		uiHandleButtonData *data;

		/* handle activated button events */
		data = but->active;

		if ((data->state == BUTTON_STATE_MENU_OPEN) &&
		    (but->type == PULLDOWN) &&
		    (but_other = ui_but_find_mouse_over(ar, event)) &&
		    (but != but_other) &&
		    (but->type == but_other->type))
		{
			/* if mouse moves to a different root-level menu button,
			 * open it to replace the current menu */
			ui_handle_button_activate(C, ar, but_other, BUTTON_ACTIVATE_OVER);
			button_activate_state(C, but_other, BUTTON_STATE_MENU_OPEN);
		}
		else if (data->state == BUTTON_STATE_MENU_OPEN) {
			int retval;

			/* handle events for menus and their buttons recursively,
			 * this will handle events from the top to the bottom menu */
			if (data->menu)
				retval = ui_handle_menus_recursive(C, event, data->menu, 0, false, false, false);

			/* handle events for the activated button */
			if ((data->menu && (retval == WM_UI_HANDLER_CONTINUE)) ||
			    (event->type == TIMER))
			{
				if (data->menu && data->menu->menuretval)
					ui_handle_button_return_submenu(C, event, but);
				else
					ui_handle_button_event(C, event, but);
			}
		}
		else {
			/* handle events for the activated button */
			ui_handle_button_event(C, event, but);
		}
	}

	/* re-enable tooltips */
	if (event->type == MOUSEMOVE && (event->x != event->prevx || event->y != event->prevy))
		ui_blocks_set_tooltips(ar, true);

	/* delayed apply callbacks */
	ui_apply_but_funcs_after(C);

	/* we block all events, this is modal interaction */
	return WM_UI_HANDLER_BREAK;
}

/* two types of popups, one with operator + enum, other with regular callbacks */
static int ui_handler_popup(bContext *C, const wmEvent *event, void *userdata)
{
	uiPopupBlockHandle *menu = userdata;
	struct ARegion *menu_region;
	/* we block all events, this is modal interaction, except for drop events which is described below */
	int retval = WM_UI_HANDLER_BREAK;

	menu_region = CTX_wm_menu(C);
	CTX_wm_menu_set(C, menu->region);

	if (event->type == EVT_DROP) {
		/* if we're handling drop event we'll want it to be handled by popup callee as well,
		 * so it'll be possible to perform such operations as opening .blend files by dropping
		 * them into blender even if there's opened popup like splash screen (sergey)
		 */

		retval = WM_UI_HANDLER_CONTINUE;
	}

	ui_handle_menus_recursive(C, event, menu, 0, false, false, true);

	/* free if done, does not free handle itself */
	if (menu->menuretval) {
		wmWindow *win = CTX_wm_window(C);
		/* copy values, we have to free first (closes region) */
		uiPopupBlockHandle temp = *menu;
		
		ui_popup_block_free(C, menu);
		UI_remove_popup_handlers(&win->modalhandlers, menu);

#ifdef USE_DRAG_TOGGLE
		{
			WM_event_free_ui_handler_all(C, &win->modalhandlers,
			                             ui_handler_region_drag_toggle, ui_handler_region_drag_toggle_remove);
		}
#endif

		if ((temp.menuretval & UI_RETURN_OK) || (temp.menuretval & UI_RETURN_POPUP_OK)) {
			if (temp.popup_func)
				temp.popup_func(C, temp.popup_arg, temp.retvalue);
			if (temp.optype)
				WM_operator_name_call_ptr(C, temp.optype, temp.opcontext, NULL);
		}
		else if (temp.cancel_func)
			temp.cancel_func(C, temp.popup_arg);

		WM_event_add_mousemove(C);
	}
	else {
		/* re-enable tooltips */
		if (event->type == MOUSEMOVE && (event->x != event->prevx || event->y != event->prevy))
			ui_blocks_set_tooltips(menu->region, true);
	}

	/* delayed apply callbacks */
	ui_apply_but_funcs_after(C);

	CTX_wm_region_set(C, menu_region);

	return retval;
}

static void ui_handler_remove_popup(bContext *C, void *userdata)
{
	uiPopupBlockHandle *menu = userdata;

	/* free menu block if window is closed for some reason */
	ui_popup_block_free(C, menu);

	/* delayed apply callbacks */
	ui_apply_but_funcs_after(C);
}

void UI_add_region_handlers(ListBase *handlers)
{
	WM_event_remove_ui_handler(handlers, ui_handler_region, ui_handler_remove_region, NULL, false);
	WM_event_add_ui_handler(NULL, handlers, ui_handler_region, ui_handler_remove_region, NULL);
}

void UI_add_popup_handlers(bContext *C, ListBase *handlers, uiPopupBlockHandle *popup)
{
	WM_event_add_ui_handler(C, handlers, ui_handler_popup, ui_handler_remove_popup, popup);
}

void UI_remove_popup_handlers(ListBase *handlers, uiPopupBlockHandle *popup)
{
	WM_event_remove_ui_handler(handlers, ui_handler_popup, ui_handler_remove_popup, popup, false);
}

void UI_remove_popup_handlers_all(bContext *C, ListBase *handlers)
{
	WM_event_free_ui_handler_all(C, handlers, ui_handler_popup, ui_handler_remove_popup);
}

bool UI_textbutton_activate_rna(const bContext *C, ARegion *ar,
                                const void *rna_poin_data, const char *rna_prop_id)
{
	uiBlock *block;
	uiBut *but = NULL;
	
	for (block = ar->uiblocks.first; block; block = block->next) {
		for (but = block->buttons.first; but; but = but->next) {
			if (but->type == TEX) {
				if (but->rnaprop && but->rnapoin.data == rna_poin_data) {
					if (STREQ(RNA_property_identifier(but->rnaprop), rna_prop_id)) {
						break;
					}
				}
			}
		}
		if (but)
			break;
	}
	
	if (but) {
		uiButActiveOnly(C, ar, block, but);
		return true;
	}
	else {
		return false;
	}
}

bool UI_textbutton_activate_but(const bContext *C, uiBut *actbut)
{
	ARegion *ar = CTX_wm_region(C);
	uiBlock *block;
	uiBut *but = NULL;
	
	for (block = ar->uiblocks.first; block; block = block->next) {
		for (but = block->buttons.first; but; but = but->next)
			if (but == actbut && but->type == TEX)
				break;

		if (but)
			break;
	}
	
	if (but) {
		uiButActiveOnly(C, ar, block, but);
		return true;
	}
	else {
		return false;
	}
}


void ui_button_clipboard_free(void)
{
	curvemapping_free_data(&but_copypaste_curve);
}
