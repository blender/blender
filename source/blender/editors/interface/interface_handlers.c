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

#include "DNA_sensor_types.h"
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_string_cursor_utf8.h"

#include "BLF_translation.h"

#include "PIL_time.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_report.h"
#include "BKE_texture.h"
#include "BKE_tracking.h"
#include "BKE_unit.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_keyframing.h"

#include "UI_interface.h"

#include "BLF_api.h"

#include "interface_intern.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

/* proto */
static void ui_add_smart_controller(bContext *C, uiBut *from, uiBut *to);
static void ui_add_link(bContext *C, uiBut *from, uiBut *to);

/***************** structs and defines ****************/

#define BUTTON_TOOLTIP_DELAY        0.500
#define BUTTON_FLASH_DELAY          0.020
#define MENU_SCROLL_INTERVAL        0.1
#define BUTTON_AUTO_OPEN_THRESH     0.3
#define BUTTON_MOUSE_TOWARDS_THRESH 1.0

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

typedef struct uiHandleButtonData {
	wmWindowManager *wm;
	wmWindow *window;
	ARegion *region;

	int interactive;

	/* overall state */
	uiHandleButtonState state;
	int cancel, escapecancel, retval;
	int applied, appliedinteractive;
	wmTimer *flashtimer;

	/* edited value */
	char *str, *origstr;
	double value, origvalue, startvalue;
	float vec[3], origvec[3];
	int togdual, togonly;
	ColorBand *coba;

	/* tooltip */
	ARegion *tooltip;
	wmTimer *tooltiptimer;
	
	/* auto open */
	int used_mouse;
	wmTimer *autoopentimer;

	/* text selection/editing */
	int maxlen, selextend, selstartx;

	/* number editing / dragging */
	int draglastx, draglasty;
	int dragstartx, dragstarty;
	int dragchange, draglock, dragsel;
	float dragf, dragfstart;
	CBData *dragcbd;

	/* menu open (watch uiFreeActiveButtons) */
	uiPopupBlockHandle *menu;
	int menuretval;
	
	/* search box (watch uiFreeActiveButtons) */
	ARegion *searchbox;

	/* post activate */
	uiButtonActivateType posttype;
	uiBut *postbut;
} uiHandleButtonData;

typedef struct uiAfterFunc {
	struct uiAfterFunc *next, *prev;

	uiButHandleFunc func;
	void *func_arg1;
	void *func_arg2;
	void *func_arg3;
	
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

	char undostr[512];

	int autokey;
} uiAfterFunc;

static int ui_but_contains_pt(uiBut *but, int mx, int my);
static int ui_mouse_inside_button(ARegion *ar, uiBut *but, int x, int y);
static void button_activate_state(bContext *C, uiBut *but, uiHandleButtonState state);
static int ui_handler_region_menu(bContext *C, wmEvent *event, void *userdata);
static void ui_handle_button_activate(bContext *C, ARegion *ar, uiBut *but, uiButtonActivateType type);
static void button_timers_tooltip_remove(bContext *C, uiBut *but);

/* ******************** menu navigation helpers ************** */

static int ui_but_editable(uiBut *but)
{
	return ELEM5(but->type, LABEL, SEPR, ROUNDBOX, LISTBOX, PROGRESSBAR);
}

static uiBut *ui_but_prev(uiBut *but)
{
	while (but->prev) {
		but = but->prev;
		if (!ui_but_editable(but)) return but;
	}
	return NULL;
}

static uiBut *ui_but_next(uiBut *but)
{
	while (but->next) {
		but = but->next;
		if (!ui_but_editable(but)) return but;
	}
	return NULL;
}

static uiBut *ui_but_first(uiBlock *block)
{
	uiBut *but;
	
	but = block->buttons.first;
	while (but) {
		if (!ui_but_editable(but)) return but;
		but = but->next;
	}
	return NULL;
}

static uiBut *ui_but_last(uiBlock *block)
{
	uiBut *but;
	
	but = block->buttons.last;
	while (but) {
		if (!ui_but_editable(but)) return but;
		but = but->prev;
	}
	return NULL;
}

static int ui_is_a_warp_but(uiBut *but)
{
	if (U.uiflag & USER_CONTINUOUS_MOUSE) {
		if (ELEM6(but->type, NUM, NUMABS, HSVCIRCLE, TRACKPREVIEW, HSVCUBE, BUT_CURVE)) {
			return TRUE;
		}
	}

	return FALSE;
}

static float ui_mouse_scale_warp_factor(const short shift)
{
	if (U.uiflag & USER_CONTINUOUS_MOUSE) {
		return shift ? 0.05f : 1.0f;
	}
	else {
		return 1.0f;
	}
}

static void ui_mouse_scale_warp(uiHandleButtonData *data,
                                const float mx, const float my,
                                float *r_mx, float *r_my,
                                const short shift)
{
	if (U.uiflag & USER_CONTINUOUS_MOUSE) {
		const float fac = ui_mouse_scale_warp_factor(shift);
		/* slow down the mouse, this is fairly picky */
		*r_mx = (data->dragstartx * (1.0f - fac) + mx * fac);
		*r_my = (data->dragstarty * (1.0f - fac) + my * fac);
	}
	else {
		*r_mx = mx;
		*r_my = my;
	}
}

/* file selectors are exempt from utf-8 checks */
int ui_is_but_utf8(uiBut *but)
{
	if (but->rnaprop) {
		const int subtype = RNA_property_subtype(but->rnaprop);
		return !(ELEM4(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME, PROP_BYTESTRING));
	}
	else {
		return !(but->flag & UI_BUT_NO_UTF8);
	}
}

/* ********************** button apply/revert ************************/

static ListBase UIAfterFuncs = {NULL, NULL};

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
		after = MEM_callocN(sizeof(uiAfterFunc), "uiAfterFunc");

		if (but->func && ELEM(but, but->func_arg1, but->func_arg2)) {
			/* exception, this will crash due to removed button otherwise */
			but->func(C, but->func_arg1, but->func_arg2);
		}
		else
			after->func = but->func;

		after->func_arg1 = but->func_arg1;
		after->func_arg2 = but->func_arg2;
		after->func_arg3 = but->func_arg3;

		after->funcN = but->funcN;
		after->func_argN = MEM_dupallocN(but->func_argN);

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

		BLI_addtail(&UIAfterFuncs, after);
	}
}

static void ui_apply_autokey_undo(bContext *C, uiBut *but)
{
	Scene *scene = CTX_data_scene(C);
	uiAfterFunc *after;

	if (but->flag & UI_BUT_UNDO) {
		const char *str = NULL;

		/* define which string to use for undo */
		if (ELEM(but->type, LINK, INLINK)) str = "Add button link";
		else if (ELEM(but->type, MENU, ICONTEXTROW)) str = but->drawstr;
		else if (but->drawstr[0]) str = but->drawstr;
		else str = but->tip;

		/* fallback, else we don't get an undo! */
		if (str == NULL || str[0] == '\0') {
			str = "Unknown Action";
		}

		/* delayed, after all other funcs run, popups are closed, etc */
		after = MEM_callocN(sizeof(uiAfterFunc), "uiAfterFunc");
		BLI_strncpy(after->undostr, str, sizeof(after->undostr));
		BLI_addtail(&UIAfterFuncs, after);
	}

	/* try autokey */
	ui_but_anim_autokey(C, but, scene, scene->r.cfra);
}

static void ui_apply_but_funcs_after(bContext *C)
{
	uiAfterFunc *afterf, after;
	PointerRNA opptr;
	ListBase funcs;

	/* copy to avoid recursive calls */
	funcs = UIAfterFuncs;
	UIAfterFuncs.first = UIAfterFuncs.last = NULL;

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
			WM_operator_name_call(C, after.optype->idname, after.opcontext, (after.opptr) ? &opptr : NULL);

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
	data->applied = 1;
}

static void ui_apply_but_BUTM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_set_but_val(but, but->hardmin);
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_BLOCK(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	if (ELEM3(but->type, MENU, ICONROW, ICONTEXTROW))
		ui_set_but_val(but, data->value);

	ui_check_but(but);
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_TOG(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	double value;
	int w, lvalue, push;
	
	/* local hack... */
	if (but->type == BUT_TOGDUAL && data->togdual) {
		if (but->pointype == SHO)
			but->poin += 2;
		else if (but->pointype == INT)
			but->poin += 4;
	}
	
	value = ui_get_but_val(but);
	lvalue = (int)value;
	
	if (but->bit) {
		w = BTST(lvalue, but->bitnr);
		if (w) lvalue = BCLR(lvalue, but->bitnr);
		else lvalue = BSET(lvalue, but->bitnr);
		
		if (but->type == TOGR) {
			if (!data->togonly) {
				lvalue = 1 << (but->bitnr);
	
				ui_set_but_val(but, (double)lvalue);
			}
			else {
				if (lvalue == 0) lvalue = 1 << (but->bitnr);
			}
		}
		
		ui_set_but_val(but, (double)lvalue);
		if (but->type == ICONTOG || but->type == ICONTOGN) ui_check_but(but);
	}
	else {
		
		if (value == 0.0) push = 1;
		else push = 0;
		
		if (ELEM3(but->type, TOGN, ICONTOGN, OPTIONN)) push = !push;
		ui_set_but_val(but, (double)push);
		if (but->type == ICONTOG || but->type == ICONTOGN) ui_check_but(but);
	}
	
	/* end local hack... */
	if (but->type == BUT_TOGDUAL && data->togdual) {
		if (but->pointype == SHO)
			but->poin -= 2;
		else if (but->pointype == INT)
			but->poin -= 4;
	}
	
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_ROW(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data)
{
	uiBut *bt;
	
	ui_set_but_val(but, but->hardmax);
	
	/* states of other row buttons */
	for (bt = block->buttons.first; bt; bt = bt->next)
		if (bt != but && bt->poin == but->poin && ELEM(bt->type, ROW, LISTROW))
			ui_check_but(bt);
	
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = 1;
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
	data->applied = 1;
}

static void ui_apply_but_NUM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	if (data->str) {
		if (ui_set_but_string(C, but, data->str)) {
			data->value = ui_get_but_val(but);
		}
		else {
			data->cancel = 1;
			return;
		}
	}
	else
		ui_set_but_val(but, data->value);

	ui_check_but(but);
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_TOG3(bContext *C, uiBut *but, uiHandleButtonData *data)
{ 
	if (but->pointype == SHO) {
		short *sp = (short *)but->poin;
		
		if (BTST(sp[1], but->bitnr)) {
			sp[1] = BCLR(sp[1], but->bitnr);
			sp[0] = BCLR(sp[0], but->bitnr);
		}
		else if (BTST(sp[0], but->bitnr)) {
			sp[1] = BSET(sp[1], but->bitnr);
		}
		else {
			sp[0] = BSET(sp[0], but->bitnr);
		}
	}
	else {
		if (BTST(*(but->poin + 2), but->bitnr)) {
			*(but->poin + 2) = BCLR(*(but->poin + 2), but->bitnr);
			*(but->poin) = BCLR(*(but->poin), but->bitnr);
		}
		else if (BTST(*(but->poin), but->bitnr)) {
			*(but->poin + 2) = BSET(*(but->poin + 2), but->bitnr);
		}
		else {
			*(but->poin) = BSET(*(but->poin), but->bitnr);
		}
	}
	
	ui_check_but(but);
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_VEC(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_set_but_vectorf(but, data->vec);
	ui_check_but(but);
	ui_apply_but_func(C, but);

	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_COLORBAND(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_CURVE(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_IDPOIN(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_set_but_string(C, but, data->str);
	ui_check_but(but);
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}

#ifdef WITH_INTERNATIONAL
static void ui_apply_but_CHARTAB(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}
#endif

/* ****************** drag drop code *********************** */

static int ui_but_mouse_inside_icon(uiBut *but, ARegion *ar, wmEvent *event)
{
	rcti rect;
	int x = event->x, y = event->y;
	
	ui_window_to_block(ar, but->block, &x, &y);
	
	rect.xmin = but->x1; rect.xmax = but->x2;
	rect.ymin = but->y1; rect.ymax = but->y2;
	
	if (but->imb) ;  /* use button size itself */
	else if (but->flag & UI_ICON_LEFT) {
		rect.xmax = rect.xmin + (rect.ymax - rect.ymin);
	}
	else {
		int delta = (rect.xmax - rect.xmin) - (rect.ymax - rect.ymin);
		rect.xmin += delta / 2;
		rect.xmax -= delta / 2;
	}
	
	return BLI_in_rcti(&rect, x, y);
}

static int ui_but_start_drag(bContext *C, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	/* prevent other WM gestures to start while we try to drag */
	WM_gestures_remove(C);

	if (ABS(data->dragstartx - event->x) + ABS(data->dragstarty - event->y) > U.dragthreshold) {
		wmDrag *drag;
		
		button_activate_state(C, but, BUTTON_STATE_EXIT);
		data->cancel = 1;
		
		drag = WM_event_start_drag(C, but->icon, but->dragtype, but->dragpoin, ui_get_but_val(but));
		if (but->imb)
			WM_event_drag_image(drag, but->imb, but->imb_scale, but->x2 - but->x1, but->y2 - but->y1);
		return 1;
	}
	
	return 0;
}

/* ********************** linklines *********************** */

static void ui_delete_active_linkline(uiBlock *block)
{
	uiBut *but;
	uiLink *link;
	uiLinkLine *line, *nline;
	int a, b;
	
	but = block->buttons.first;
	while (but) {
		if (but->type == LINK && but->link) {
			line = but->link->lines.first;
			while (line) {
				
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
								
								if ( (*(link->ppoin))[a] != line->to->poin) {
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
				line = nline;
			}
		}
		but = but->next;
	}
}


static uiLinkLine *ui_is_a_link(uiBut *from, uiBut *to)
{
	uiLinkLine *line;
	uiLink *link;
	
	link = from->link;
	if (link) {
		line = link->lines.first;
		while (line) {
			if (line->from == from && line->to == to) return line;
			line = line->next;
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

	/* (3) add a new controller */
	if (WM_operator_name_call(C, "LOGIC_OT_controller_add", WM_OP_EXEC_DEFAULT, NULL) & OPERATOR_FINISHED) {
		cont = (bController *)ob->controllers.last;

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
}

static void ui_add_link(bContext *C, uiBut *from, uiBut *to)
{
	/* in 'from' we have to add a link to 'to' */
	uiLink *link;
	uiLinkLine *line;
	void **oldppoin;
	int a;
	
	if ( (line = ui_is_a_link(from, to)) ) {
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
	data->applied = 1;
}

static void ui_apply_but_IMAGE(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_HISTOGRAM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_WAVEFORM(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}

static void ui_apply_but_TRACKPREVIEW(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	ui_apply_but_func(C, but);
	data->retval = but->retval;
	data->applied = 1;
}


static void ui_apply_button(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, int interactive)
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
		if (interactive)
			data->appliedinteractive = 1;
		else if (data->appliedinteractive)
			return;
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
		case SEARCH_MENU:
			ui_apply_but_TEX(C, but, data);
			break;
		case TOGBUT: 
		case TOG: 
		case TOGR: 
		case ICONTOG:
		case ICONTOGN:
		case TOGN:
		case BUT_TOGDUAL:
		case OPTION:
		case OPTIONN:
			ui_apply_but_TOG(C, but, data);
			break;
		case ROW:
		case LISTROW:
			ui_apply_but_ROW(C, block, but, data);
			break;
		case SCROLL:
		case NUM:
		case NUMABS:
		case SLI:
		case NUMSLI:
			ui_apply_but_NUM(C, but, data);
			break;
		case HSVSLI:
			break;
		case TOG3:	
			ui_apply_but_TOG3(C, but, data);
			break;
		case MENU:
		case ICONROW:
		case ICONTEXTROW:
		case BLOCK:
		case PULLDOWN:
			ui_apply_but_BLOCK(C, but, data);
			break;
		case COL:
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
		case IDPOIN:
			ui_apply_but_IDPOIN(C, but, data);
			break;
#ifdef WITH_INTERNATIONAL
		case CHARTAB:
			ui_apply_but_CHARTAB(C, but, data);
			break;
#endif
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

	but->editstr = editstr;
	but->editval = editval;
	but->editvec = editvec;
	but->editcoba = editcoba;
	but->editcumap = editcumap;
}

/* ******************* drop event ********************  */

/* only call if event type is EVT_DROP */
static void ui_but_drop(bContext *C, wmEvent *event, uiBut *but, uiHandleButtonData *data)
{
	wmDrag *wmd;
	ListBase *drags = event->customdata; /* drop event type has listbase customdata by default */
	
	for (wmd = drags->first; wmd; wmd = wmd->next) {
		if (wmd->type == WM_DRAG_ID) {
			/* align these types with UI_but_active_drop_name */
			if (ELEM3(but->type, TEX, IDPOIN, SEARCH_MENU)) {
				ID *id = (ID *)wmd->poin;
				
				if (but->poin == NULL && but->rnapoin.data == NULL) {}
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
				BLI_strncpy(data->str, id->name + 2, data->maxlen);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}
	
}

/* ******************* copy and paste ********************  */

/* c = copy, v = paste */
static void ui_but_copy_paste(bContext *C, uiBut *but, uiHandleButtonData *data, char mode)
{
	static ColorBand but_copypaste_coba = {0};
	char buf[UI_MAX_DRAW_STR + 1] = {0};

	if (mode == 'v' && but->lock)
		return;

	if (mode == 'v') {
		/* extract first line from clipboard in case of multi-line copies */
		char *p, *pbuf = WM_clipboard_text_get(0);
		p = pbuf;
		if (p) {
			int i = 0;
			while (*p && *p != '\r' && *p != '\n' && i < UI_MAX_DRAW_STR) {
				buf[i++] = *p;
				p++;
			}
			buf[i] = 0;
			MEM_freeN(pbuf);
		}
	}
	
	/* numeric value */
	if (ELEM4(but->type, NUM, NUMABS, NUMSLI, HSVSLI)) {
		
		if (but->poin == NULL && but->rnapoin.data == NULL) ;
		else if (mode == 'c') {
			ui_get_but_string(but, buf, sizeof(buf));
			WM_clipboard_text_set(buf, 0);
		}
		else {
			double val;

			if (ui_set_but_string_eval_num(C, but, buf, &val)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				data->value = val;
				ui_set_but_string(C, but, buf);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}

	/* RGB triple */
	else if (but->type == COL) {
		float rgb[3];
		
		if (but->poin == NULL && but->rnapoin.data == NULL) ;
		else if (mode == 'c') {

			ui_get_but_vectorf(but, rgb);
			BLI_snprintf(buf, sizeof(buf), "[%f, %f, %f]", rgb[0], rgb[1], rgb[2]);
			WM_clipboard_text_set(buf, 0);
			
		}
		else {
			if (sscanf(buf, "[%f, %f, %f]", &rgb[0], &rgb[1], &rgb[2]) == 3) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				ui_set_but_vectorf(but, rgb);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}
	}

	/* text/string and ID data */
	else if (ELEM3(but->type, TEX, IDPOIN, SEARCH_MENU)) {
		uiHandleButtonData *active_data = but->active;

		if (but->poin == NULL && but->rnapoin.data == NULL) ;
		else if (mode == 'c') {
			button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
			BLI_strncpy(buf, active_data->str, UI_MAX_DRAW_STR);
			WM_clipboard_text_set(active_data->str, 0);
			active_data->cancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else {
			button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);

			if (ui_is_but_utf8(but)) BLI_strncpy_utf8(active_data->str, buf, active_data->maxlen);
			else BLI_strncpy(active_data->str, buf, active_data->maxlen);

			if (but->type == SEARCH_MENU) {
				/* else uiSearchboxData.active member is not updated [#26856] */
				ui_searchbox_update(C, data->searchbox, but, 1);
			}
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}
	/* colorband (not supported by system clipboard) */
	else if (but->type == BUT_COLORBAND) {
		if (mode == 'c') {
			if (but->poin == NULL)
				return;

			memcpy(&but_copypaste_coba, but->poin, sizeof(ColorBand));
		}
		else {
			if (but_copypaste_coba.tot == 0)
				return;

			if (!but->poin)
				but->poin = MEM_callocN(sizeof(ColorBand), "colorband");

			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			memcpy(data->coba, &but_copypaste_coba, sizeof(ColorBand));
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}
	/* operator button (any type) */
	else if (but->optype) {
		if (mode == 'c') {
			PointerRNA *opptr;
			char *str;
			opptr = uiButGetOperatorPtrRNA(but); /* allocated when needed, the button owns it */

			str = WM_operator_pystring(C, but->optype, opptr, 0);

			WM_clipboard_text_set(str, 0);

			MEM_freeN(str);
		}
	}
}

/* ************* in-button text selection/editing ************* */


static int ui_textedit_delete_selection(uiBut *but, uiHandleButtonData *data)
{
	char *str = data->str;
	int len = strlen(str);
	int change = 0;
	if (but->selsta != but->selend && len) {
		memmove(str + but->selsta, str + but->selend, (len - but->selend) + 1);
		change = 1;
	}
	
	but->pos = but->selend = but->selsta;
	return change;
}

/* note, but->block->aspect is used here, when drawing button style is getting scaled too */
static void ui_textedit_set_cursor_pos(uiBut *but, uiHandleButtonData *data, short x)
{
	uiStyle *style = UI_GetStyle();  // XXX pass on as arg
	uiFontStyle *fstyle = &style->widget;
	int startx = but->x1;
	char *origstr;

	uiStyleFontSet(fstyle);

	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	
	origstr = MEM_callocN(sizeof(char) * data->maxlen, "ui_textedit origstr");
	
	BLI_strncpy(origstr, but->drawstr, data->maxlen);
	
	/* XXX solve generic */
	if (but->type == NUM || but->type == NUMSLI)
		startx += (int)(0.5f * (but->y2 - but->y1));
	else if (ELEM(but->type, TEX, SEARCH_MENU)) {
		startx += 5;
		if (but->flag & UI_HAS_ICON)
			startx += UI_DPI_ICON_SIZE;
	}
	
	/* mouse dragged outside the widget to the left */
	if (x < startx && but->ofs > 0) {	
		int i = but->ofs;

		origstr[but->ofs] = 0;
		
		while (i > 0) {
			if (BLI_str_cursor_step_prev_utf8(origstr, but->ofs, &i)) {
				if (BLF_width(fstyle->uifont_id, origstr + i) > (startx - x) * 0.25f) break;  // 0.25 == scale factor for less sensitivity
			}
			else {
				break; /* unlikely but possible */
			}
		}
		but->ofs = i;
		but->pos = but->ofs;
	}
	/* mouse inside the widget */
	else if (x >= startx) {
		int pos_i;

		/* keep track of previous distance from the cursor to the char */
		float cdist, cdist_prev = 0.0f;
		short pos_prev;

		const float aspect_sqrt = sqrtf(but->block->aspect);
		
		but->pos = pos_prev = strlen(origstr) - but->ofs;

		while (TRUE) {
			/* XXX does not take zoom level into account */
			cdist = startx + aspect_sqrt *BLF_width(fstyle->uifont_id, origstr + but->ofs);

			/* check if position is found */
			if (cdist < x) {
				/* check is previous location was in fact closer */
				if (((float)x - cdist) > (cdist_prev - (float)x)) {
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
	
	MEM_freeN(origstr);
}

static void ui_textedit_set_cursor_select(uiBut *but, uiHandleButtonData *data, short x)
{
	if (x > data->selstartx) data->selextend = EXTEND_RIGHT;
	else if (x < data->selstartx) data->selextend = EXTEND_LEFT;

	ui_textedit_set_cursor_pos(but, data, x);
						
	if (data->selextend == EXTEND_RIGHT) but->selend = but->pos;
	if (data->selextend == EXTEND_LEFT) but->selsta = but->pos;

	ui_check_but(but);
}

/* this is used for both utf8 and ascii, its meant to be used for single keys,
 * notice the buffer is either copied or not, so its not suitable for pasting in
 * - campbell */
static int ui_textedit_type_buf(uiBut *but, uiHandleButtonData *data,
                                const char *utf8_buf, int utf8_buf_len)
{
	char *str;
	int len, changed = 0;

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
			changed = 1;
		}
	}

	return changed;
}

static int ui_textedit_type_ascii(uiBut *but, uiHandleButtonData *data, char ascii)
{
	char buf[2] = {ascii, '\0'};

	if (ui_is_but_utf8(but) && (BLI_str_utf8_size(buf) == -1)) {
		printf("%s: entering invalid ascii char into an ascii key (%d)\n",
		       __func__, (int)(unsigned char)ascii);

		return 0;
	}

	/* in some cases we want to allow invalid utf8 chars */
	return ui_textedit_type_buf(but, data, buf, 1);
}

static void ui_textedit_move(uiBut *but, uiHandleButtonData *data, strCursorJumpDirection direction, int select, strCursorJumpType jump)
{
	const char *str = data->str;
	const int len = strlen(str);
	const int pos_prev = but->pos;
	const int has_sel = (but->selend - but->selsta) > 0;

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
		BLI_str_cursor_step_utf8(str, len, &pos_i, direction, jump);
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

static int ui_textedit_delete(uiBut *but, uiHandleButtonData *data, int direction, strCursorJumpType jump)
{
	char *str = data->str;
	const int len = strlen(str);

	int changed = 0;

	if (jump == STRCUR_JUMP_ALL) {
		if (len) changed = 1;
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
			BLI_str_cursor_step_utf8(str, len, &pos, direction, jump);
			step = pos - but->pos;
			memmove(&str[but->pos], &str[but->pos + step], (len + 1) - but->pos);
			changed = 1;
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

				BLI_str_cursor_step_utf8(str, len, &pos, direction, jump);
				step = but->pos - pos;
				memmove(&str[but->pos - step], &str[but->pos], (len + 1) - but->pos);
				but->pos -= step;
				changed = 1;
			}
		} 
	}

	return changed;
}

static int ui_textedit_autocomplete(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	char *str;
	int changed = 1;

	str = data->str;

	if (data->searchbox)
		ui_searchbox_autocomplete(C, data->searchbox, but, data->str);
	else
		but->autocomplete_func(C, str, but->autofunc_arg);

	but->pos = strlen(str);
	but->selsta = but->selend = but->pos;

	return changed;
}

static int ui_textedit_copypaste(uiBut *but, uiHandleButtonData *data, int paste, int copy, int cut)
{
	char buf[UI_MAX_DRAW_STR] = {0};
	char *str, *p, *pbuf;
	int len, x, i, changed = 0;

	str = data->str;
	len = strlen(str);
	
	/* paste */
	if (paste) {
		/* extract the first line from the clipboard */
		p = pbuf = WM_clipboard_text_get(0);

		if (p && p[0]) {
			unsigned int y;
			i = 0;
			while (*p && *p != '\r' && *p != '\n' && i < UI_MAX_DRAW_STR - 1) {
				buf[i++] = *p;
				p++;
			}
			buf[i] = 0;

			/* paste over the current selection */
			if ((but->selend - but->selsta) > 0) {
				ui_textedit_delete_selection(but, data);
				len = strlen(str);
			}
			
			for (y = 0; y < strlen(buf); y++) {
				/* add contents of buffer */
				if (len + 1 < data->maxlen) {
					for (x = data->maxlen; x > but->pos; x--)
						str[x] = str[x - 1];
					str[but->pos] = buf[y];
					but->pos++; 
					len++;
					str[len] = '\0';
				}
			}

			changed = 1;
		}

		if (pbuf) {
			MEM_freeN(pbuf);
		}
	}
	/* cut & copy */
	else if (copy || cut) {
		/* copy the contents to the copypaste buffer */
		for (x = but->selsta; x <= but->selend; x++) {
			if (x == but->selend)
				buf[x] = '\0';
			else
				buf[(x - but->selsta)] = str[x];
		}

		WM_clipboard_text_set(buf, 0);
		
		/* for cut only, delete the selection afterwards */
		if (cut)
			if ((but->selend - but->selsta) > 0)
				changed = ui_textedit_delete_selection(but, data);
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

	/* retrieve string */
	data->maxlen = ui_get_but_string_max_length(but);
	data->str = MEM_callocN(sizeof(char) * data->maxlen + 1, "textedit str");
	ui_get_but_string(but, data->str, data->maxlen);

	if (ELEM3(but->type, NUM, NUMABS, NUMSLI)) {
		ui_convert_to_unit_alt_name(but, data->str, data->maxlen);
	}

	/* won't change from now on */
	len = strlen(data->str);

	data->origstr = BLI_strdupn(data->str, len);
	data->selextend = 0;
	data->selstartx = 0;

	/* set cursor pos to the end of the text */
	but->editstr = data->str;
	but->pos = len;
	but->selsta = 0;
	but->selend = len;

	/* optional searchbox */
	if (but->type == SEARCH_MENU) {
		data->searchbox = ui_searchbox_create(C, data->region, but);
		ui_searchbox_update(C, data->searchbox, but, 1); /* 1= reset */
	}
	
	ui_check_but(but);
	
	WM_cursor_modal(CTX_wm_window(C), BC_TEXTEDITCURSOR);
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
			if (data->cancel == 0)
				ui_searchbox_apply(but, data->searchbox);

			ui_searchbox_free(C, data->searchbox);
			data->searchbox = NULL;
		}
		
		but->editstr = NULL;
		but->pos = -1;
	}
	
	WM_cursor_restore(CTX_wm_window(C));
}

static void ui_textedit_next_but(uiBlock *block, uiBut *actbut, uiHandleButtonData *data)
{
	uiBut *but;

	/* label and roundbox can overlap real buttons (backdrops...) */
	if (ELEM4(actbut->type, LABEL, SEPR, ROUNDBOX, LISTBOX))
		return;

	for (but = actbut->next; but; but = but->next) {
		if (ELEM7(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI, IDPOIN, SEARCH_MENU)) {
			if (!(but->flag & UI_BUT_DISABLED)) {
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
				return;
			}
		}
	}
	for (but = block->buttons.first; but != actbut; but = but->next) {
		if (ELEM7(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI, IDPOIN, SEARCH_MENU)) {
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
	if (ELEM4(actbut->type, LABEL, SEPR, ROUNDBOX, LISTBOX))
		return;

	for (but = actbut->prev; but; but = but->prev) {
		if (ELEM7(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI, IDPOIN, SEARCH_MENU)) {
			if (!(but->flag & UI_BUT_DISABLED)) {
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
				return;
			}
		}
	}
	for (but = block->buttons.last; but != actbut; but = but->prev) {
		if (ELEM7(but->type, TEX, NUM, NUMABS, NUMSLI, HSVSLI, IDPOIN, SEARCH_MENU)) {
			if (!(but->flag & UI_BUT_DISABLED)) {
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_TEXT_EDITING;
				return;
			}
		}
	}
}


static void ui_do_but_textedit(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	int mx, my, changed = 0, inbox = 0, update = 0, retval = WM_UI_HANDLER_CONTINUE;

	switch (event->type) {
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
		case MOUSEMOVE:
			if (data->searchbox)
				ui_searchbox_event(C, data->searchbox, but, event);
			
			break;
		case RIGHTMOUSE:
		case ESCKEY:
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			retval = WM_UI_HANDLER_BREAK;
			break;
		case LEFTMOUSE: {
			
			/* exit on LMB only on RELEASE for searchbox, to mimic other popups, and allow multiple menu levels */
			if (data->searchbox)
				inbox = ui_searchbox_inside(data->searchbox, event->x, event->y);

			if (event->val == KM_PRESS) {
				mx = event->x;
				my = event->y;
				ui_window_to_block(data->region, block, &mx, &my);

				if (ui_but_contains_pt(but, mx, my)) {
					ui_textedit_set_cursor_pos(but, data, mx);
					but->selsta = but->selend = but->pos;
					data->selstartx = mx;

					button_activate_state(C, but, BUTTON_STATE_TEXT_SELECTING);
					retval = WM_UI_HANDLER_BREAK;
				}
				else if (inbox == 0) {
					/* if searchbox, click outside will cancel */
					if (data->searchbox)
						data->cancel = data->escapecancel = 1;
					button_activate_state(C, but, BUTTON_STATE_EXIT);
					retval = WM_UI_HANDLER_BREAK;
				}
			}
			else if (inbox) {
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_BREAK;
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
						changed = ui_textedit_copypaste(but, data, 1, 0, 0);
					else if (event->type == CKEY)
						changed = ui_textedit_copypaste(but, data, 0, 1, 0);
					else if (event->type == XKEY)
						changed = ui_textedit_copypaste(but, data, 0, 0, 1);

					retval = WM_UI_HANDLER_BREAK;
				}
				break;
			case RIGHTARROWKEY:
				ui_textedit_move(but, data, STRCUR_DIR_NEXT, event->shift, event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case LEFTARROWKEY:
				ui_textedit_move(but, data, STRCUR_DIR_PREV, event->shift, event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case DOWNARROWKEY:
				if (data->searchbox) {
					ui_searchbox_event(C, data->searchbox, but, event);
					break;
				}
			/* pass on purposedly */
			case ENDKEY:
				ui_textedit_move(but, data, STRCUR_DIR_NEXT, event->shift, STRCUR_JUMP_ALL);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case UPARROWKEY:
				if (data->searchbox) {
					ui_searchbox_event(C, data->searchbox, but, event);
					break;
				}
			/* pass on purposedly */
			case HOMEKEY:
				ui_textedit_move(but, data, STRCUR_DIR_PREV, event->shift, STRCUR_JUMP_ALL);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case PADENTER:
			case RETKEY:
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_BREAK;
				break;
			case DELKEY:
				changed = ui_textedit_delete(but, data, 1, event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE);
				retval = WM_UI_HANDLER_BREAK;
				break;

			case BACKSPACEKEY:
				changed = ui_textedit_delete(but, data, 0, event->shift ? STRCUR_JUMP_ALL :  (event->ctrl ? STRCUR_JUMP_DELIM : STRCUR_JUMP_NONE));
				retval = WM_UI_HANDLER_BREAK;
				break;
				
			case TABKEY:
				/* there is a key conflict here, we can't tab with autocomplete */
				if (but->autocomplete_func || data->searchbox) {
					changed = ui_textedit_autocomplete(C, but, data);
					update = 1; /* do live update for tab key */
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
			if (ELEM3(but->type, NUM, NUMABS, NUMSLI)) { /* could use data->min*/
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
			update = 1;
	}

	if (changed) {
		/* only update when typing for TAB key */
		if (update && data->interactive) ui_apply_button(C, block, but, data, 1);
		else ui_check_but(but);
		but->changed = TRUE;
		
		if (data->searchbox)
			ui_searchbox_update(C, data->searchbox, but, 1);  /* 1 = reset */
	}

	if (changed || (retval == WM_UI_HANDLER_BREAK))
		ED_region_tag_redraw(data->region);
}

static void ui_do_but_textedit_select(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	int mx, my, retval = WM_UI_HANDLER_CONTINUE;

	switch (event->type) {
		case MOUSEMOVE: {
			mx = event->x;
			my = event->y;
			ui_window_to_block(data->region, block, &mx, &my);

			ui_textedit_set_cursor_select(but, data, mx);
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
	else if (ELEM3(but->type, BUT_NORMAL, HSVCUBE, HSVCIRCLE)) {
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

	data->dragchange = 0;
	data->draglock = 1;
}

static void ui_numedit_end(uiBut *but, uiHandleButtonData *data)
{
	but->editval = NULL;
	but->editvec = NULL;
	but->editcoba = NULL;
	but->editcumap = NULL;

	data->dragstartx = 0;
	data->draglastx = 0;
	data->dragchange = 0;
	data->dragcbd = NULL;
	data->dragsel = 0;
}

static void ui_numedit_apply(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data)
{
	if (data->interactive) ui_apply_button(C, block, but, data, 1);
	else ui_check_but(but);

	ED_region_tag_redraw(data->region);
}

/* ****************** menu opening for various types **************** */

static void ui_blockopen_begin(bContext *C, uiBut *but, uiHandleButtonData *data)
{
	uiBlockCreateFunc func = NULL;
	uiBlockHandleCreateFunc handlefunc = NULL;
	uiMenuCreateFunc menufunc = NULL;
	char *menustr = NULL;
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
			if (but->menu_create_func) {
				menufunc = but->menu_create_func;
				arg = but->poin;
			}
			else {
				data->origvalue = ui_get_but_val(but);
				data->value = data->origvalue;
				but->editval = &data->value;

				menustr = but->str;
			}
			break;
		case ICONROW:
			menufunc = ui_block_func_ICONROW;
			arg = but;
			break;
		case ICONTEXTROW:
			menufunc = ui_block_func_ICONTEXTROW;
			arg = but;
			break;
		case COL:
			ui_get_but_vectorf(but, data->origvec);
			copy_v3_v3(data->vec, data->origvec);
			but->editvec = data->vec;

			handlefunc = ui_block_func_COL;
			arg = but;
			break;
	}

	if (func || handlefunc) {
		data->menu = ui_popup_block_create(C, data->region, but, func, handlefunc, arg);
		if (but->block->handle)
			data->menu->popup = but->block->handle->popup;
	}
	else if (menufunc || menustr) {
		data->menu = ui_popup_menu_create(C, data->region, but, menufunc, arg, menustr);
		if (but->block->handle)
			data->menu->popup = but->block->handle->popup;
	}

	/* this makes adjacent blocks auto open from now on */
	//if (but->block->auto_open==0) but->block->auto_open= 1;
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

/* ***************** events for different button types *************** */

static int ui_do_but_BUT(bContext *C, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_WAIT_RELEASE);
			return WM_UI_HANDLER_BREAK;
		}
		else if (event->type == LEFTMOUSE && but->block->handle) {
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
				data->cancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_HOTKEYEVT(bContext *C, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
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
					data->cancel = 1;
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
					data->cancel = 1;
				
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				return WM_UI_HANDLER_BREAK;
			}
			else if (event->type == ESCKEY) {
				data->cancel = 1;
				data->escapecancel = 1;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			
		}
	}
	
	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_KEYEVT(bContext *C, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
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
				data->cancel = 1;

			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_TEX(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (ELEM(event->type, LEFTMOUSE, EVT_BUT_OPEN) && event->val == KM_PRESS) {
			if (but->dt == UI_EMBOSSN && !event->ctrl) ;
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

static int ui_do_but_TOG(bContext *C, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
			data->togdual = event->ctrl;
			data->togonly = !event->shift;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	return WM_UI_HANDLER_CONTINUE;
}

static int ui_do_but_EXIT(bContext *C, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
		
		if (ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
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
static float ui_numedit_apply_snapf(uiBut *but, float tempf, float softmin, float softmax, float softrange, int snap)
{
	if (tempf == softmin || tempf == softmax || snap == 0) {
		/* pass */
	}
	else {
		float fac = 1.0f;
		
		if (ui_is_but_unit(but)) {
			UnitSettings *unit = but->block->unit;
			int unit_type = RNA_SUBTYPE_UNIT_VALUE(uiButGetUnitType(but));

			if (bUnit_IsValid(unit->system, unit_type)) {
				fac = (float)bUnit_BaseScalar(unit->system, unit_type);
				if (ELEM3(unit_type, B_UNIT_LENGTH, B_UNIT_AREA, B_UNIT_VOLUME)) {
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

		if (snap == 1) {
			if (softrange < 2.10f) tempf = 0.1f * floorf(10.0f * tempf);
			else if (softrange < 21.0f) tempf = floorf(tempf);
			else tempf = 10.0f * floorf(tempf / 10.0f);
		}
		else if (snap == 2) {
			if (softrange < 2.10f) tempf = 0.01f * floorf(100.0f * tempf);
			else if (softrange < 21.0f) tempf = 0.1f * floorf(10.0f * tempf);
			else tempf = floor(tempf);
		}
		
		if (fac != 1.0f)
			tempf *= fac;
	}

	return tempf;
}

static float ui_numedit_apply_snap(int temp, float softmin, float softmax, int snap)
{
	if (temp == softmin || temp == softmax)
		return temp;

	switch (snap) {
		case 0:
			break;
		case 1:
			temp = 10 * (temp / 10);
			break;
		case 2:
			temp = 100 * (temp / 100);
			break;
	}

	return temp;
}

static int ui_numedit_but_NUM(uiBut *but, uiHandleButtonData *data, float fac, int snap, int mx)
{
	float deler, tempf, softmin, softmax, softrange;
	int lvalue, temp, changed = 0;
	
	if (mx == data->draglastx)
		return changed;
	
	/* drag-lock - prevent unwanted scroll adjustments */
	/* change value (now 3) to adjust threshold in pixels */
	if (data->draglock) {
		if (abs(mx - data->dragstartx) <= 3)
			return changed;

		data->draglock = 0;
		data->dragstartx = mx;  /* ignore mouse movement within drag-lock */
	}

	softmin = but->softmin;
	softmax = but->softmax;
	softrange = softmax - softmin;

	if (ui_is_a_warp_but(but)) {
		/* Mouse location isn't screen clamped to the screen so use a linear mapping
		 * 2px == 1-int, or 1px == 1-ClickStep */
		if (ui_is_but_float(but)) {
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
				data->dragchange = 1;
				data->value = tempf;
				changed = 1;
			}
		}
		else {
			if (softrange > 256) fac = 1.0;             /* 1px == 1 */
			else if (softrange > 32) fac = 1.0 / 2.0;   /* 2px == 1 */
			else fac = 1.0 / 16.0;                  /* 16px == 1? */

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
				data->dragchange = 1;
				data->value = temp;
				changed = 1;
			}
		}

		data->draglastx = mx;
	}
	else {
		/* Use a non-linear mapping of the mouse drag especially for large floats (normal behavior) */
		deler = 500;
		if (!ui_is_but_float(but)) {
			/* prevent large ranges from getting too out of control */
			if (softrange > 600) deler = powf(softrange, 0.75);
			
			if (softrange < 100) deler = 200.0;
			if (softrange < 25) deler = 50.0;
		}
		deler /= fac;

		if (softrange > 11) {
			/* non linear change in mouse input- good for high precicsion */
			data->dragf += (((float)(mx - data->draglastx)) / deler) * (fabsf(data->dragstartx - mx) * 0.002f);
		}
		else if (softrange > 129) { /* only scale large int buttons */
			/* non linear change in mouse input- good for high precicsionm ints need less fine tuning */
			data->dragf += (((float)(mx - data->draglastx)) / deler) * (fabsf(data->dragstartx - mx) * 0.004f);
		}
		else {
			/*no scaling */
			data->dragf += ((float)(mx - data->draglastx)) / deler;
		}
	
		CLAMP(data->dragf, 0.0f, 1.0f);
		data->draglastx = mx;
		tempf = (softmin + data->dragf * softrange);


		if (!ui_is_but_float(but)) {
			temp = floorf(tempf + 0.5f);

			temp = ui_numedit_apply_snap(temp, softmin, softmax, snap);

			CLAMP(temp, softmin, softmax);
			lvalue = (int)data->value;
			
			if (temp != lvalue) {
				data->dragchange = 1;
				data->value = (double)temp;
				changed = 1;
			}
		}
		else {
			temp = 0;
			tempf = ui_numedit_apply_snapf(but, tempf, softmin, softmax, softrange, snap);

			CLAMP(tempf, softmin, softmax);

			if (tempf != (float)data->value) {
				data->dragchange = 1;
				data->value = tempf;
				changed = 1;
			}
		}
	}


	return changed;
}

static int ui_do_but_NUM(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	int mx, my; /* mouse location scaled to fit the UI */
	int screen_mx, screen_my; /* mouse location kept at screen pixel coords */
	int click = 0;
	int retval = WM_UI_HANDLER_CONTINUE;
	
	mx = screen_mx = event->x;
	my = screen_my = event->y;

	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		/* XXX hardcoded keymap check.... */
		if (event->type == WHEELDOWNMOUSE && event->alt) {
			mx = but->x1;
			click = 1;
		}
		else if (event->type == WHEELUPMOUSE && event->alt) {
			mx = but->x2;
			click = 1;
		}
		else if (event->val == KM_PRESS) {
			if (ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->ctrl) {
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
			else if (event->type == LEFTMOUSE) {
				data->dragstartx = data->draglastx = ui_is_a_warp_but(but) ? screen_mx : mx;
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
			else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS)
				click = 1;
			else if (event->type == MINUSKEY && event->val == KM_PRESS) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				data->value = -data->value;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_BREAK;
			}
		}
		
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			if (data->dragchange)
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			else
				click = 1;
		}
		else if (event->type == MOUSEMOVE) {
			float fac;
			int snap;

			fac = 1.0f;
			if (event->shift) fac /= 10.0f;
			if (event->alt) fac /= 20.0f;
			
			snap = (event->ctrl) ? (event->shift) ? 2 : 1 : 0;

			if (ui_numedit_but_NUM(but, data, fac, snap, (ui_is_a_warp_but(but) ? screen_mx : mx)))
				ui_numedit_apply(C, block, but, data);
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
		int temp;

		softmin = but->softmin;
		softmax = but->softmax;

		if (!ui_is_but_float(but)) {
			if (mx < (but->x1 + (but->x2 - but->x1) / 3 - 3)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				temp = (int)data->value - 1;
				if (temp >= softmin && temp <= softmax)
					data->value = (double)temp;
				else
					data->cancel = 1;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else if (mx > (but->x1 + (2 * (but->x2 - but->x1) / 3) + 3)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				temp = (int)data->value + 1;
				if (temp >= softmin && temp <= softmax)
					data->value = (double)temp;
				else
					data->cancel = 1;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
		}
		else {
			if (mx < (but->x1 + (but->x2 - but->x1) / 3 - 3)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				tempf = (float)data->value - 0.01f * but->a1;
				if (tempf < softmin) tempf = softmin;
				data->value = tempf;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else if (mx > but->x1 + (2 * ((but->x2 - but->x1) / 3) + 3)) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);

				tempf = (float)data->value + 0.01f * but->a1;
				if (tempf > softmax) tempf = softmax;
				data->value = tempf;

				button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
			else
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
		}

		retval = WM_UI_HANDLER_BREAK;
	}
	
	return retval;
}

static int ui_numedit_but_SLI(uiBut *but, uiHandleButtonData *data, const short shift, const short ctrl, int mx)
{
	float deler, f, tempf, softmin, softmax, softrange;
	int temp, lvalue, changed = 0;

	softmin = but->softmin;
	softmax = but->softmax;
	softrange = softmax - softmin;

	if (but->type == NUMSLI) deler = ((but->x2 - but->x1) - 5.0f * but->aspect);
	else if (but->type == HSVSLI) deler = ((but->x2 - but->x1) / 2.0f - 5.0f * but->aspect);
	else if (but->type == SCROLL) {
		int horizontal = (but->x2 - but->x1 > but->y2 - but->y1);
		float size = (horizontal) ? (but->x2 - but->x1) : -(but->y2 - but->y1);
		deler = size * (but->softmax - but->softmin) / (but->softmax - but->softmin + but->a1);
	}
	else deler = (but->x2 - but->x1 - 5.0f * but->aspect);

	f = (float)(mx - data->dragstartx) / deler + data->dragfstart;
	
	if (shift)
		f = (f - data->dragfstart) / 10.0f + data->dragfstart;

	CLAMP(f, 0.0f, 1.0f);
	tempf = softmin + f * softrange;
	temp = floorf(tempf + 0.5f);

	if (ctrl) {
		if (tempf == softmin || tempf == softmax) ;
		else if (ui_is_but_float(but)) {

			if (shift) {
				if (tempf == softmin || tempf == softmax) ;
				else if (softmax - softmin < 2.10f) tempf = 0.01f * floorf(100.0f * tempf);
				else if (softmax - softmin < 21.0f) tempf = 0.1f * floorf(10.0f * tempf);
				else tempf = floorf(tempf);
			}
			else {
				if (softmax - softmin < 2.10f) tempf = 0.1f * floorf(10.0f * tempf);
				else if (softmax - softmin < 21.0f) tempf = floorf(tempf);
				else tempf = 10.0f * floorf(tempf / 10.0f);
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
			data->dragchange = 1;
			changed = 1;
		}
	}
	else {
		CLAMP(tempf, softmin, softmax);

		if (tempf != (float)data->value) {
			data->value = tempf;
			data->dragchange = 1;
			changed = 1;
		}
	}

	return changed;
}

static int ui_do_but_SLI(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	int mx, my, click = 0;
	int retval = WM_UI_HANDLER_CONTINUE;

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		/* XXX hardcoded keymap check.... */
		if (event->type == WHEELDOWNMOUSE && event->alt) {
			mx = but->x1;
			click = 2;
		}
		else if (event->type == WHEELUPMOUSE && event->alt) {
			mx = but->x2;
			click = 2;
		}
		else if (event->val == KM_PRESS) {
			if (ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->ctrl) {
				button_activate_state(C, but, BUTTON_STATE_TEXT_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
			/* alt-click on sides to get "arrows" like in NUM buttons, and match wheel usage above */
			else if (event->type == LEFTMOUSE && event->alt) {
				int halfpos = (but->x1 + but->x2) / 2;
				click = 2;
				if (mx < halfpos)
					mx = but->x1;
				else
					mx = but->x2;
			}
			else if (event->type == LEFTMOUSE) {
				data->dragstartx = mx;
				data->draglastx = mx;
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				retval = WM_UI_HANDLER_BREAK;
			}
			else if (ELEM(event->type, PADENTER, RETKEY) && event->val == KM_PRESS)
				click = 1;
			else if (event->type == MINUSKEY && event->val == KM_PRESS) {
				button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
				data->value = -data->value;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_BREAK;
			}
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			if (data->dragchange)
				button_activate_state(C, but, BUTTON_STATE_EXIT);
			else
				click = 1;
		}
		else if (event->type == MOUSEMOVE) {
			if (ui_numedit_but_SLI(but, data, event->shift, event->ctrl, mx))
				ui_numedit_apply(C, block, but, data);
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
				f = (float)(mx - but->x1) / (but->x2 - but->x1); /* same as below */
			}
			else
#endif
			{
				f = (float)(mx - but->x1) / (but->x2 - but->x1);
			}
			
			f = softmin + f * softrange;
			
			if (!ui_is_but_float(but)) {
				if (f < temp) temp--;
				else temp++;
				
				if (temp >= softmin && temp <= softmax)
					data->value = temp;
				else
					data->cancel = 1;
			} 
			else {
				if (f < tempf) tempf -= 0.01f;
				else tempf += 0.01f;
				
				if (tempf >= softmin && tempf <= softmax)
					data->value = tempf;
				else
					data->cancel = 1;
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
	
	return retval;
}

static int ui_do_but_SCROLL(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	int mx, my /*, click= 0 */;
	int retval = WM_UI_HANDLER_CONTINUE;
	int horizontal = (but->x2 - but->x1 > but->y2 - but->y1);
	
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
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if (event->type == MOUSEMOVE) {
			if (ui_numedit_but_SLI(but, data, 0, 0, (horizontal) ? mx : my))
				ui_numedit_apply(C, block, but, data);
		}

		retval = WM_UI_HANDLER_BREAK;
	}
	
	return retval;
}


static int ui_do_but_BLOCK(bContext *C, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
		
		/* regular open menu */
		if (ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
			button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
			return WM_UI_HANDLER_BREAK;
		}
		else if (ELEM3(but->type, MENU, ICONROW, ICONTEXTROW)) {
			
			if (event->type == WHEELDOWNMOUSE && event->alt) {
				data->value = ui_step_name_menu(but, -1);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				ui_apply_button(C, but->block, but, data, 1);

				/* button's state need to be changed to EXIT so moving mouse away from this mouse wouldn't lead
				 * to cancel changes made to this button, but changing state to EXIT also makes no button active for
				 * a while which leads to triggering operator when doing fast scrolling mouse wheel.
				 * using post activate stuff from button allows to make button be active again after checking for all
				 * all that mouse leave and cancel stuff, so quick scroll wouldn't be an issue anymore.
				 * same goes for scrolling wheel in another direction below (sergey)
				 */
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_OVER;

				return WM_UI_HANDLER_BREAK;
			}
			else if (event->type == WHEELUPMOUSE && event->alt) {
				data->value = ui_step_name_menu(but, 1);
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				ui_apply_button(C, but->block, but, data, 1);

				/* why this is needed described above */
				data->postbut = but;
				data->posttype = BUTTON_ACTIVATE_OVER;

				return WM_UI_HANDLER_BREAK;
			}
		}
		else if (but->type == COL) {
			if (ELEM(event->type, WHEELDOWNMOUSE, WHEELUPMOUSE) && event->alt) {
				float *hsv = ui_block_hsv_get(but->block);
				float col[3];
				
				ui_get_but_vectorf(but, col);
				rgb_to_hsv_compat_v(col, hsv);

				if (event->type == WHEELDOWNMOUSE)
					hsv[2] = CLAMPIS(hsv[2] - 0.05f, 0.0f, 1.0f);
				else
					hsv[2] = CLAMPIS(hsv[2] + 0.05f, 0.0f, 1.0f);
				
				hsv_to_rgb_v(hsv, data->vec);
				ui_set_but_vectorf(but, data->vec);
				
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				ui_apply_button(C, but->block, but, data, 1);
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
			data->cancel = 1;
			return WM_UI_HANDLER_BREAK;
		}
		
		if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
			button_activate_state(C, but, BUTTON_STATE_MENU_OPEN);
			return WM_UI_HANDLER_BREAK;
		}

	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_numedit_but_NORMAL(uiBut *but, uiHandleButtonData *data, int mx, int my)
{
	float dx, dy, rad, radsq, mrad, *fp;
	int mdx, mdy, changed = 1;
	
	/* button is presumed square */
	/* if mouse moves outside of sphere, it does negative normal */

	/* note that both data->vec and data->origvec should be normalized
	 * else we'll get a harmless but annoying jump when first clicking */

	fp = data->origvec;
	rad = (but->x2 - but->x1);
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
	else mdx = mdy = 0;
	
	dx = (float)(mx + mdx - data->dragstartx);
	dy = (float)(my + mdy - data->dragstarty);

	fp = data->vec;
	mrad = dx * dx + dy * dy;
	if (mrad < radsq) { /* inner circle */
		fp[0] = dx;
		fp[1] = dy;
		fp[2] = sqrt(radsq - dx * dx - dy * dy);
	}
	else {  /* outer circle */
		
		mrad = rad / sqrtf(mrad);  // veclen
		
		dx *= (2.0f * mrad - 1.0f);
		dy *= (2.0f * mrad - 1.0f);
		
		mrad = dx * dx + dy * dy;
		if (mrad < radsq) {
			fp[0] = dx;
			fp[1] = dy;
			fp[2] = -sqrt(radsq - dx * dx - dy * dy);
		}
	}
	normalize_v3(fp);

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static int ui_do_but_NORMAL(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
			if (ui_numedit_but_NORMAL(but, data, mx, my))
				ui_numedit_apply(C, block, but, data);
			
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				if (ui_numedit_but_NORMAL(but, data, mx, my))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS)
			button_activate_state(C, but, BUTTON_STATE_EXIT);

		return WM_UI_HANDLER_BREAK;
	}
	
	return WM_UI_HANDLER_CONTINUE;
}

static int ui_numedit_but_HSVCUBE(uiBut *but, uiHandleButtonData *data, int mx, int my, const short shift)
{
	float rgb[3];
	float *hsv = ui_block_hsv_get(but->block);
	float x, y;
	float mx_fl, my_fl;
	int changed = 1;
	int color_profile = but->block->color_profile;
	
	ui_mouse_scale_warp(data, mx, my, &mx_fl, &my_fl, shift);

	if (but->rnaprop) {
		if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
			color_profile = BLI_PR_NONE;
	}

	ui_get_but_vectorf(but, rgb);

	rgb_to_hsv_compat_v(rgb, hsv);


	/* relative position within box */
	x = ((float)mx_fl - but->x1) / (but->x2 - but->x1);
	y = ((float)my_fl - but->y1) / (but->y2 - but->y1);
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
		case UI_GRAD_V_ALT:
			/* vertical 'value' strip */

			/* exception only for value strip - use the range set in but->min/max */
			hsv[2] = y * (but->softmax - but->softmin) + but->softmin;

			if (color_profile)
				hsv[2] = srgb_to_linearrgb(hsv[2]);

			if (hsv[2] > but->softmax)
				hsv[2] = but->softmax;
			break;
		default:
			assert(!"invalid hsv type");
	}

	hsv_to_rgb_v(hsv, rgb);
	copy_v3_v3(data->vec, rgb);

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static void ui_ndofedit_but_HSVCUBE(uiBut *but, uiHandleButtonData *data, wmNDOFMotionData *ndof, const short shift)
{
	float *hsv = ui_block_hsv_get(but->block);
	float rgb[3];
	float sensitivity = (shift ? 0.15f : 0.3f) * ndof->dt;
	
	int color_profile = but->block->color_profile;
	
	if (but->rnaprop) {
		if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
			color_profile = BLI_PR_NONE;
	}

	ui_get_but_vectorf(but, rgb);
	rgb_to_hsv_compat_v(rgb, hsv);
	
	switch ((int)but->a1) {
		case UI_GRAD_SV:
			hsv[2] += ndof->ry * sensitivity;
			hsv[1] += ndof->rx * sensitivity;
			break;
		case UI_GRAD_HV:
			hsv[0] += ndof->ry * sensitivity;
			hsv[2] += ndof->rx * sensitivity;
			break;
		case UI_GRAD_HS:
			hsv[0] += ndof->ry * sensitivity;
			hsv[1] += ndof->rx * sensitivity;
			break;
		case UI_GRAD_H:
			hsv[0] += ndof->ry * sensitivity;
			break;
		case UI_GRAD_S:
			hsv[1] += ndof->ry * sensitivity;
			break;
		case UI_GRAD_V:
			hsv[2] += ndof->ry * sensitivity;
			break;
		case UI_GRAD_V_ALT:	
			/* vertical 'value' strip */
			
			/* exception only for value strip - use the range set in but->min/max */
			hsv[2] += ndof->rx * sensitivity;
			
			if (color_profile)
				hsv[2] = srgb_to_linearrgb(hsv[2]);
			
			CLAMP(hsv[2], but->softmin, but->softmax);
		default:
			assert(!"invalid hsv type");
	}
	
	hsv_to_rgb_v(hsv, rgb);
	copy_v3_v3(data->vec, rgb);
	ui_set_but_vectorf(but, data->vec);
}

static int ui_do_but_HSVCUBE(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
			if (ui_numedit_but_HSVCUBE(but, data, mx, my, event->shift))
				ui_numedit_apply(C, block, but, data);
			
			return WM_UI_HANDLER_BREAK;
		}
		else if (event->type == NDOF_MOTION) {
			wmNDOFMotionData *ndof = (wmNDOFMotionData *) event->customdata;
			
			ui_ndofedit_but_HSVCUBE(but, data, ndof, event->shift);
			
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			ui_apply_button(C, but->block, but, data, 1);
			
			return WM_UI_HANDLER_BREAK;
		}
		/* XXX hardcoded keymap check.... */
		else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
			if (but->a1 == UI_GRAD_V_ALT) {
				int len;
				
				/* reset only value */
				
				len = RNA_property_array_length(&but->rnapoin, but->rnaprop);
				if (len >= 3) {
					float rgb[3], def_hsv[3];
					float *def;
					float *hsv = ui_block_hsv_get(but->block);
					def = MEM_callocN(sizeof(float) * len, "reset_defaults - float");
					
					RNA_property_float_get_default_array(&but->rnapoin, but->rnaprop, def);
					rgb_to_hsv_v(def, def_hsv);
					
					ui_get_but_vectorf(but, rgb);
					rgb_to_hsv_compat_v(rgb, hsv);

					def_hsv[0] = hsv[0];
					def_hsv[1] = hsv[1];
					
					hsv_to_rgb_v(def_hsv, rgb);
					ui_set_but_vectorf(but, rgb);
					
					RNA_property_update(C, &but->rnapoin, but->rnaprop);
					
					MEM_freeN(def);
				}
				return WM_UI_HANDLER_BREAK;
			}
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				if (ui_numedit_but_HSVCUBE(but, data, mx, my, event->shift))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS)
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		
		return WM_UI_HANDLER_BREAK;
	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_numedit_but_HSVCIRCLE(uiBut *but, uiHandleButtonData *data, float mx, float my, int shift)
{
	rcti rect;
	int changed = 1;
	float mx_fl, my_fl;
	float rgb[3];
	float hsv[3];
	
	ui_mouse_scale_warp(data, mx, my, &mx_fl, &my_fl, shift);

	rect.xmin = but->x1; rect.xmax = but->x2;
	rect.ymin = but->y1; rect.ymax = but->y2;
	
	ui_get_but_vectorf(but, rgb);
	copy_v3_v3(hsv, ui_block_hsv_get(but->block));
	rgb_to_hsv_compat_v(rgb, hsv);
	
	/* exception, when using color wheel in 'locked' value state:
	 * allow choosing a hue for black values, by giving a tiny increment */
	if (but->flag & UI_BUT_COLOR_LOCK) { // lock
		if (hsv[2] == 0.f) hsv[2] = 0.0001f;
	}


	ui_hsvcircle_vals_from_pos(hsv, hsv + 1, &rect, mx_fl, my_fl);

	if (but->flag & UI_BUT_COLOR_CUBIC)
		hsv[1] = 1.0f - sqrt3f(1.0f - hsv[1]);

	hsv_to_rgb_v(hsv, rgb);

	if ((but->flag & UI_BUT_VEC_SIZE_LOCK) && (rgb[0] || rgb[1] || rgb[2])) {
		normalize_v3(rgb);
		mul_v3_fl(rgb, but->a2);
	}

	ui_set_but_vectorf(but, rgb);
	
	data->draglastx = mx;
	data->draglasty = my;
	
	return changed;
}

static void ui_ndofedit_but_HSVCIRCLE(uiBut *but, uiHandleButtonData *data, wmNDOFMotionData *ndof, const short shift)
{
	float *hsv = ui_block_hsv_get(but->block);
	float rgb[3];
	float phi, r /*, sqr */ /* UNUSED */, v[2];
	float sensitivity = (shift ? 0.15f : 0.3f) * ndof->dt;
	
	ui_get_but_vectorf(but, rgb);
	rgb_to_hsv_compat_v(rgb, hsv);
	
	/* Convert current color on hue/sat disc to circular coordinates phi, r */
	phi = fmodf(hsv[0] + 0.25f, 1.0f) * -2.0f * (float)M_PI;
	r = hsv[1];
	/* sqr= r>0.f?sqrtf(r):1; */ /* UNUSED */
	
	/* Convert to 2d vectors */
	v[0] = r * cosf(phi);
	v[1] = r * sinf(phi);
	
	/* Use ndof device y and x rotation to move the vector in 2d space */
	v[0] += ndof->ry * sensitivity;
	v[1] += ndof->rx * sensitivity;

	/* convert back to polar coords on circle */
	phi = atan2f(v[0], v[1]) / (2.0f * (float)M_PI) + 0.5f;
	
	/* use ndof z rotation to additionally rotate hue */
	phi -= ndof->rz * sensitivity * 0.5f;
	
	r = len_v2(v);
	CLAMP(r, 0.0f, 1.0f);
	
	/* convert back to hsv values, in range [0,1] */
	hsv[0] = fmodf(phi, 1.0f);
	hsv[1] = r;

	/* exception, when using color wheel in 'locked' value state:
	 * allow choosing a hue for black values, by giving a tiny increment */
	if (but->flag & UI_BUT_COLOR_LOCK) { // lock
		if (hsv[2] == 0.0f) hsv[2] = 0.0001f;
	}
	
	hsv_to_rgb_v(hsv, data->vec);
	
	if ((but->flag & UI_BUT_VEC_SIZE_LOCK) && (data->vec[0] || data->vec[1] || data->vec[2])) {
		normalize_v3(data->vec);
		mul_v3_fl(data->vec, but->a2);
	}
	
	ui_set_but_vectorf(but, data->vec);
}


static int ui_do_but_HSVCIRCLE(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
			if (ui_numedit_but_HSVCIRCLE(but, data, mx, my, event->shift))
				ui_numedit_apply(C, block, but, data);
			
			return WM_UI_HANDLER_BREAK;
		}
		else if (event->type == NDOF_MOTION) {
			wmNDOFMotionData *ndof = (wmNDOFMotionData *) event->customdata;
			
			ui_ndofedit_but_HSVCIRCLE(but, data, ndof, event->shift);

			button_activate_state(C, but, BUTTON_STATE_EXIT);
			ui_apply_button(C, but->block, but, data, 1);
			
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
				rgb_to_hsv_v(def, def_hsv);
				
				ui_get_but_vectorf(but, rgb);
				rgb_to_hsv_compat_v(rgb, hsv);
				
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
		if (event->type == ESCKEY) {
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
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
				if (ui_numedit_but_HSVCIRCLE(but, data, mx, my, event->shift))
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


static int ui_numedit_but_COLORBAND(uiBut *but, uiHandleButtonData *data, int mx)
{
	float dx;
	int changed = 0;

	if (data->draglastx == mx)
		return changed;

	dx = ((float)(mx - data->draglastx)) / (but->x2 - but->x1);
	data->dragcbd->pos += dx;
	CLAMP(data->dragcbd->pos, 0.0f, 1.0f);
	
	colorband_update_sort(data->coba);
	data->dragcbd = data->coba->data + data->coba->cur;  /* because qsort */
	
	data->draglastx = mx;
	changed = 1;

	return changed;
}

static int ui_do_but_COLORBAND(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
				float pos = ((float)(mx - but->x1)) / (but->x2 - but->x1);
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
					xco = but->x1 + (cbd->pos * (but->x2 - but->x1));
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
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS)
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		
		return WM_UI_HANDLER_BREAK;
	}

	return WM_UI_HANDLER_CONTINUE;
}

static int ui_numedit_but_CURVE(uiBut *but, uiHandleButtonData *data, int snap,
                                float mx, float my, const short shift)
{
	CurveMapping *cumap = (CurveMapping *)but->poin;
	CurveMap *cuma = cumap->cm + cumap->cur;
	CurveMapPoint *cmp = cuma->curve;
	float fx, fy, zoomx, zoomy /*, offsx, offsy */ /* UNUSED */;
	int a, changed = 0;

	zoomx = (but->x2 - but->x1) / (cumap->curr.xmax - cumap->curr.xmin);
	zoomy = (but->y2 - but->y1) / (cumap->curr.ymax - cumap->curr.ymin);
	/* offsx= cumap->curr.xmin; */
	/* offsy= cumap->curr.ymin; */

	if (snap) {
		float d[2];

		d[0] = mx - data->dragstartx;
		d[1] = my - data->dragstarty;

		if (len_v2(d) < 3.0f)
			snap = 0;
	}

	if (data->dragsel != -1) {
		const float mval_factor = ui_mouse_scale_warp_factor(shift);
		int moved_point = 0;     /* for ctrl grid, can't use orig coords because of sorting */
		
		fx = (mx - data->draglastx) / zoomx;
		fy = (my - data->draglasty) / zoomy;

		fx *= mval_factor;
		fy *= mval_factor;

		for (a = 0; a < cuma->totpoint; a++) {
			if (cmp[a].flag & SELECT) {
				float origx = cmp[a].x, origy = cmp[a].y;
				cmp[a].x += fx;
				cmp[a].y += fy;
				if (snap) {
					cmp[a].x = 0.125f * floorf(0.5f + 8.0f * cmp[a].x);
					cmp[a].y = 0.125f * floorf(0.5f + 8.0f * cmp[a].y);
				}
				if (cmp[a].x != origx || cmp[a].y != origy)
					moved_point = 1;
			}
		}

		curvemapping_changed(cumap, 0); /* no remove doubles */
		
		if (moved_point) {
			data->draglastx = mx;
			data->draglasty = my;
			changed = 1;
		}

		data->dragchange = 1; /* mark for selection */
	}
	else {
		fx = (mx - data->draglastx) / zoomx;
		fy = (my - data->draglasty) / zoomy;
		
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
		
		data->draglastx = mx;
		data->draglasty = my;

		changed = 1;
	}

	return changed;
}

static int ui_do_but_CURVE(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
{
	int mx, my, a, changed = 0;

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

			zoomx = (but->x2 - but->x1) / (cumap->curr.xmax - cumap->curr.xmin);
			zoomy = (but->y2 - but->y1) / (cumap->curr.ymax - cumap->curr.ymin);
			offsx = cumap->curr.xmin;
			offsy = cumap->curr.ymin;

			if (event->ctrl) {
				fx = ((float)my - but->x1) / zoomx + offsx;
				fy = ((float)my - but->y1) / zoomy + offsy;
				
				curvemap_insert(cuma, fx, fy);
				curvemapping_changed(cumap, 0);
				changed = 1;
			}

			/* check for selecting of a point */
			cmp = cuma->curve;   /* ctrl adds point, new malloc */
			for (a = 0; a < cuma->totpoint; a++) {
				fx = but->x1 + zoomx * (cmp[a].x - offsx);
				fy = but->y1 + zoomy * (cmp[a].y - offsy);
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
				fx = ((float)mx - but->x1) / zoomx + offsx;
				fy = ((float)my - but->y1) / zoomy + offsy;
				
				cmp = cuma->table;

				/* loop through the curve segment table and find what's near the mouse.
				 * 0.05 is kinda arbitrary, but seems to be what works nicely. */
				for (i = 0; i <= CM_TABLE; i++) {
					if ( (fabsf(fx - cmp[i].x) < 0.05f) &&
					     (fabsf(fy - cmp[i].y) < 0.05f))
					{
					
						curvemap_insert(cuma, fx, fy);
						curvemapping_changed(cumap, 0);

						changed = 1;
						
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
				if (event->shift == FALSE) {
					for (a = 0; a < cuma->totpoint; a++)
						cmp[a].flag &= ~SELECT;
					cmp[sel].flag |= SELECT;
				}
				else
					cmp[sel].flag ^= SELECT;
			}
			else {
				/* move the view */
				data->cancel = 1;
			}

			data->dragsel = sel;

			data->dragstartx = mx;
			data->dragstarty = my;
			data->draglastx = mx;
			data->draglasty = my;

			button_activate_state(C, but, BUTTON_STATE_NUM_EDITING);
			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				if (ui_numedit_but_CURVE(but, data, event->ctrl, mx, my, event->shift))
					ui_numedit_apply(C, block, but, data);
			}
		}
		else if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
			if (data->dragsel != -1) {
				CurveMapping *cumap = (CurveMapping *)but->poin;
				CurveMap *cuma = cumap->cm + cumap->cur;
				CurveMapPoint *cmp = cuma->curve;

				if (!data->dragchange) {
					/* deselect all, select one */
					if (event->shift == FALSE) {
						for (a = 0; a < cuma->totpoint; a++)
							cmp[a].flag &= ~SELECT;
						cmp[data->dragsel].flag |= SELECT;
					}
				}
				else
					curvemapping_changed(cumap, 1);  /* remove doubles */
			}

			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}

		return WM_UI_HANDLER_BREAK;
	}

	/* UNUSED but keep for now */
	(void)changed;

	return WM_UI_HANDLER_CONTINUE;
}

static int in_scope_resize_zone(uiBut *but, int UNUSED(x), int y)
{
	/* bottom corner return (x > but->x2 - SCOPE_RESIZE_PAD) && (y < but->y1 + SCOPE_RESIZE_PAD); */
	return (y < but->y1 + SCOPE_RESIZE_PAD);
}

static int ui_numedit_but_HISTOGRAM(uiBut *but, uiHandleButtonData *data, int mx, int my)
{
	Histogram *hist = (Histogram *)but->poin;
	/* rcti rect; */
	int changed = 1;
	float /* dx, */ dy; /* UNUSED */
	
	/* rect.xmin = but->x1; rect.xmax = but->x2; */
	/* rect.ymin = but->y1; rect.ymax = but->y2; */
	
	/* dx = mx - data->draglastx; */ /* UNUSED */
	dy = my - data->draglasty;

	if (in_scope_resize_zone(but, data->dragstartx, data->dragstarty)) {
		/* resize histogram widget itself */
		hist->height = (but->y2 - but->y1) + (data->dragstarty - my);
	}
	else {
		/* scale histogram values */
		const float yfac = minf(powf(hist->ymax, 2.0f), 1.0f) * 0.5f;
		hist->ymax += dy * yfac;
	
		CLAMP(hist->ymax, 1.f, 100.f);
	}
	
	data->draglastx = mx;
	data->draglasty = my;
	
	return changed;
}

static int ui_do_but_HISTOGRAM(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
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

static int ui_numedit_but_WAVEFORM(uiBut *but, uiHandleButtonData *data, int mx, int my)
{
	Scopes *scopes = (Scopes *)but->poin;
	/* rcti rect; */
	int changed = 1;
	float /* dx, */ dy /* , yfac=1.f */; /* UNUSED */

	/* rect.xmin = but->x1; rect.xmax = but->x2; */
	/* rect.ymin = but->y1; rect.ymax = but->y2; */

	/* dx = mx - data->draglastx; */ /* UNUSED */
	dy = my - data->draglasty;


	if (in_scope_resize_zone(but, data->dragstartx, data->dragstarty)) {
		/* resize waveform widget itself */
		scopes->wavefrm_height = (but->y2 - but->y1) + (data->dragstarty - my);
	}
	else {
		/* scale waveform values */
		/* yfac = scopes->wavefrm_yfac; */ /* UNUSED */
		scopes->wavefrm_yfac += dy / 200.0f;

		CLAMP(scopes->wavefrm_yfac, 0.5f, 2.f);
	}

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static int ui_do_but_WAVEFORM(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
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

static int ui_numedit_but_VECTORSCOPE(uiBut *but, uiHandleButtonData *data, int mx, int my)
{
	Scopes *scopes = (Scopes *)but->poin;
	/* rcti rect; */
	int changed = 1;
	/* float dx, dy; */

	/* rect.xmin = but->x1; rect.xmax = but->x2; */
	/* rect.ymin = but->y1; rect.ymax = but->y2; */

	/* dx = mx - data->draglastx; */
	/* dy = my - data->draglasty; */

	if (in_scope_resize_zone(but, data->dragstartx, data->dragstarty)) {
		/* resize vectorscope widget itself */
		scopes->vecscope_height = (but->y2 - but->y1) + (data->dragstarty - my);
	}

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static int ui_do_but_VECTORSCOPE(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
			if (ui_numedit_but_VECTORSCOPE(but, data, mx, my))
				ui_numedit_apply(C, block, but, data);

			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				if (ui_numedit_but_VECTORSCOPE(but, data, mx, my))
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

#ifdef WITH_INTERNATIONAL
static int ui_do_but_CHARTAB(bContext *UNUSED(C), uiBlock *UNUSED(block), uiBut *UNUSED(but), uiHandleButtonData *UNUSED(data), wmEvent *UNUSED(event))
{
	/* XXX 2.50 bad global and state access */
#if 0
	float sx, sy, ex, ey;
	float width, height;
	float butw, buth;
	int mx, my, x, y, cs, che;

	mx = event->x;
	my = event->y;
	ui_window_to_block(data->region, block, &mx, &my);

	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		if (ELEM3(event->type, LEFTMOUSE, PADENTER, RETKEY) && event->val == KM_PRESS) {
			/* Calculate the size of the button */
			width = abs(but->x2 - but->x1);
			height = abs(but->y2 - but->y1);

			butw = floor(width / 12);
			buth = floor(height / 6);

			/* Initialize variables */
			sx = but->x1;
			ex = but->x1 + butw;
			sy = but->y1 + height - buth;
			ey = but->y1 + height;

			cs = G.charstart;

			/* And the character is */
			x = (int) ((mx / butw) - 0.5);
			y = (int) (6 - ((my / buth) - 0.5));

			che = cs + (y * 12) + x;

			if (che > G.charmax)
				che = 0;

			if (G.obedit) {
				do_textedit(0, 0, che);
			}

			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
		else if (ELEM(event->type, WHEELUPMOUSE, PAGEUPKEY)) {
			for (but = block->buttons.first; but; but = but->next) {
				if (but->type == CHARTAB) {
					G.charstart = G.charstart - (12 * 6);
					if (G.charstart < 0)
						G.charstart = 0;
					if (G.charstart < G.charmin)
						G.charstart = G.charmin;
					ui_draw_but(but);

					//Really nasty... to update the num button from the same butblock
					for (bt = block->buttons.first; bt; bt = bt->next)
					{
						if (ELEM(bt->type, NUM, NUMABS)) {
							ui_check_but(bt);
							ui_draw_but(bt);
						}
					}
					retval = UI_CONT;
					break;
				}
			}

			return WM_UI_HANDLER_BREAK;
		}
		else if (ELEM(event->type, WHEELDOWNMOUSE, PAGEDOWNKEY)) {
			for (but = block->buttons.first; but; but = but->next) {
				if (but->type == CHARTAB) {
					G.charstart = G.charstart + (12 * 6);
					if (G.charstart > (0xffff - 12 * 6))
						G.charstart = 0xffff - (12 * 6);
					if (G.charstart > G.charmax - 12 * 6)
						G.charstart = G.charmax - 12 * 6;
					ui_draw_but(but);

					for (bt = block->buttons.first; bt; bt = bt->next)
					{
						if (ELEM(bt->type, NUM, NUMABS)) {
							ui_check_but(bt);
							ui_draw_but(bt);
						}
					}
					
					but->flag |= UI_ACTIVE;
					retval = UI_RETURN_OK;
					break;
				}
			}

			return WM_UI_HANDLER_BREAK;
		}
	}
#endif

	return WM_UI_HANDLER_CONTINUE;
}
#endif


static int ui_do_but_LINK(bContext *C, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
				data->cancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
			return WM_UI_HANDLER_BREAK;
		}
	}
	
	return WM_UI_HANDLER_CONTINUE;
}

static int ui_numedit_but_TRACKPREVIEW(bContext *C, uiBut *but, uiHandleButtonData *data,
                                       int mx, int my, const short shift)
{
	MovieClipScopes *scopes = (MovieClipScopes *)but->poin;
	int changed = 1;
	float dx, dy;

	dx = mx - data->draglastx;
	dy = my - data->draglasty;

	if (shift) {
		dx /= 5.0f;
		dy /= 5.0f;
	}

	if (in_scope_resize_zone(but, data->dragstartx, data->dragstarty)) {
		/* resize preview widget itself */
		scopes->track_preview_height = (but->y2 - but->y1) + (data->dragstarty - my);
	}
	else {
		if (!scopes->track_locked) {
			if (scopes->marker->framenr != scopes->framenr)
				scopes->marker = BKE_tracking_marker_ensure(scopes->track, scopes->framenr);

			scopes->marker->flag &= ~(MARKER_DISABLED | MARKER_TRACKED);
			scopes->marker->pos[0] += -dx * scopes->slide_scale[0] / (but->block->maxx - but->block->minx);
			scopes->marker->pos[1] += -dy * scopes->slide_scale[1] / (but->block->maxy - but->block->miny);

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);
		}

		scopes->ok = 0;
	}

	data->draglastx = mx;
	data->draglasty = my;

	return changed;
}

static int ui_do_but_TRACKPREVIEW(bContext *C, uiBlock *block, uiBut *but, uiHandleButtonData *data, wmEvent *event)
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
			if (ui_numedit_but_TRACKPREVIEW(C, but, data, mx, my, event->shift))
				ui_numedit_apply(C, block, but, data);

			return WM_UI_HANDLER_BREAK;
		}
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		if (event->type == ESCKEY) {
			data->cancel = 1;
			data->escapecancel = 1;
			button_activate_state(C, but, BUTTON_STATE_EXIT);
		}
		else if (event->type == MOUSEMOVE) {
			if (mx != data->draglastx || my != data->draglasty) {
				if (ui_numedit_but_TRACKPREVIEW(C, but, data, mx, my, event->shift))
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
		if (WM_key_event_operator_string(C, but->optype->idname, but->opcontext, prop, TRUE,
		                                 shortcut_str, sizeof(shortcut_str)))
		{
			ui_but_add_shortcut(but, shortcut_str, TRUE);
		}
		else {
			/* simply strip the shortcut */
			ui_but_add_shortcut(but, NULL, TRUE);
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
	uiStyle *style = UI_GetStyle();
	IDProperty *prop = (but->opptr) ? but->opptr->data : NULL;
	int kmi_id = WM_key_event_operator_id(C, but->optype->idname, but->opcontext, prop, 1, &km);

	kmi = WM_keymap_item_find_id(km, kmi_id);
	
	RNA_pointer_create(&wm->id, &RNA_KeyMapItem, kmi, &ptr);
	
	block = uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetHandleFunc(block, but_shortcut_name_func, but);
	uiBlockSetFlag(block, UI_BLOCK_MOVEMOUSE_QUIT);
	uiBlockSetDirection(block, UI_CENTER);
	
	layout = uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, 200, 20, style);
	
	uiItemR(layout, &ptr, "type", UI_ITEM_R_FULL_EVENT | UI_ITEM_R_IMMEDIATE, "", ICON_NONE);
	
	uiPopupBoundsBlock(block, 6, -50, 26);
	uiEndBlock(C, block);
	
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
	uiStyle *style = UI_GetStyle();
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
	uiBlockSetFlag(block, UI_BLOCK_RET_1);
	uiBlockSetDirection(block, UI_CENTER);

	layout = uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, 200, 20, style);

	uiItemR(layout, &ptr, "type", UI_ITEM_R_FULL_EVENT | UI_ITEM_R_IMMEDIATE, "", ICON_NONE);
	
	uiPopupBoundsBlock(block, 6, -50, 26);
	uiEndBlock(C, block);
	
	return block;
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
	int kmi_id = WM_key_event_operator_id(C, but->optype->idname, but->opcontext, prop, 1, &km);
	
	kmi = WM_keymap_item_find_id(km, kmi_id);
	WM_keymap_remove_item(km, kmi);
	
	but_shortcut_name_func(C, but, 0);
}

static void popup_add_shortcut_func(bContext *C, void *arg1, void *UNUSED(arg2))
{
	uiBut *but = (uiBut *)arg1;
	button_timers_tooltip_remove(C, but);
	uiPupBlock(C, menu_add_shortcut, but);
}


static int ui_but_menu(bContext *C, uiBut *but)
{
	uiPopupMenu *pup;
	uiLayout *layout;
	int length;
	char *name;
	uiStringInfo label = {BUT_GET_LABEL, NULL};

/*	if ((but->rnapoin.data && but->rnaprop) == 0 && but->optype == NULL)*/
/*		return 0;*/
	
	button_timers_tooltip_remove(C, but);

#if 0
	if (but->rnaprop)
		name = RNA_property_ui_name(but->rnaprop);
	else if (but->optype && but->optype->srna)
		name = RNA_struct_ui_name(but->optype->srna);
	else
		name = IFACE_("<needs_name>");  // XXX - should never happen.
#else
	uiButGetStrInfo(C, but, 1, &label);
	name = label.strinfo;
#endif

	pup = uiPupMenuBegin(C, name, ICON_NONE);
	layout = uiPupMenuLayout(pup);

	if (label.strinfo)
		MEM_freeN(label.strinfo);

	uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

	if (but->rnapoin.data && but->rnaprop) {
		short is_anim = RNA_property_animateable(&but->rnapoin, but->rnaprop);

		/* second slower test, saved people finding keyframe items in menus when its not possible */
		if (is_anim)
			is_anim = RNA_property_path_from_ID_check(&but->rnapoin, but->rnaprop);

		length = RNA_property_array_length(&but->rnapoin, but->rnaprop);
		
		/* Keyframes */
		if (but->flag & UI_BUT_ANIMATED_KEY) {
			/* replace/delete keyfraemes */
			if (length) {
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
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 0);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Keyframe"),
				               ICON_NONE, "ANIM_OT_keyframe_delete_button", "all", 0);
			}
			
			/* keyframe settings */
			uiItemS(layout);
			
			
		}
		else if (but->flag & UI_BUT_DRIVEN) ;
		else if (is_anim) {
			if (length) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Insert Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Insert Single Keyframe"),
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 0);
			}
			else
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Insert Keyframe"),
				               ICON_NONE, "ANIM_OT_keyframe_insert_button", "all", 0);
		}
		
		if (but->flag & UI_BUT_ANIMATED) {
			if (length) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_clear_button", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Single Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_clear_button", "all", 0);
			}
			else {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Keyframes"),
				               ICON_NONE, "ANIM_OT_keyframe_clear_button", "all", 0);
			}
		}

		/* Drivers */
		if (but->flag & UI_BUT_DRIVEN) {
			uiItemS(layout);

			if (length) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Drivers"),
				               ICON_NONE, "ANIM_OT_driver_button_remove", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Single Driver"),
				               ICON_NONE, "ANIM_OT_driver_button_remove", "all", 0);
			}
			else
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Delete Driver"),
				               ICON_NONE, "ANIM_OT_driver_button_remove", "all", 0);

			uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Copy Driver"),
			        ICON_NONE, "ANIM_OT_copy_driver_button");
			if (ANIM_driver_can_paste())
				uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Paste Driver"),
				        ICON_NONE, "ANIM_OT_paste_driver_button");
		}
		else if (but->flag & (UI_BUT_ANIMATED_KEY | UI_BUT_ANIMATED)) ;
		else if (is_anim) {
			uiItemS(layout);

			if (length) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add Drivers"),
				               ICON_NONE, "ANIM_OT_driver_button_add", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add Single Driver"),
				               ICON_NONE, "ANIM_OT_driver_button_add", "all", 0);
			}
			else
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add Driver"),
				               ICON_NONE, "ANIM_OT_driver_button_add", "all", 0);

			if (ANIM_driver_can_paste())
				uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Paste Driver"),
				        ICON_NONE, "ANIM_OT_paste_driver_button");
		}
		
		/* Keying Sets */
		/* TODO: check on modifyability of Keying Set when doing this */
		if (is_anim) {
			uiItemS(layout);

			if (length) {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add All to Keying Set"),
				               ICON_NONE, "ANIM_OT_keyingset_button_add", "all", 1);
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add Single to Keying Set"),
				               ICON_NONE, "ANIM_OT_keyingset_button_add", "all", 0);
				uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Remove from Keying Set"),
				        ICON_NONE, "ANIM_OT_keyingset_button_remove");
			}
			else {
				uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Add to Keying Set"),
				               ICON_NONE, "ANIM_OT_keyingset_button_add", "all", 0);
				uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Remove from Keying Set"),
				        ICON_NONE, "ANIM_OT_keyingset_button_remove");
			}
		}
		
		uiItemS(layout);
		
		/* Property Operators */
		
		/*Copy Property Value
		 *Paste Property Value */
		
		if (length) {
			uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Reset All to Default Values"),
			               ICON_NONE, "UI_OT_reset_default_button", "all", 1);
			uiItemBooleanO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Reset Single to Default Value"),
			               ICON_NONE, "UI_OT_reset_default_button", "all", 0);
		}
		else
			uiItemO(layout, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Reset to Default Value"),
			        ICON_NONE, "UI_OT_reset_default_button");
		
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
		int kmi_id = WM_key_event_operator_id(C, but->optype->idname, but->opcontext, prop, 1, &km);

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
	uiItemFullO(layout, "UI_OT_editsource", NULL, ICON_NONE, NULL, WM_OP_INVOKE_DEFAULT, 0);
	uiItemFullO(layout, "UI_OT_edittranslation_init", NULL, ICON_NONE, NULL, WM_OP_INVOKE_DEFAULT, 0);

	uiPupMenuEnd(C, pup);

	return 1;
}

static int ui_do_button(bContext *C, uiBlock *block, uiBut *but, wmEvent *event)
{
//	Scene *scene= CTX_data_scene(C);
	uiHandleButtonData *data;
	int retval;

	data = but->active;
	retval = WM_UI_HANDLER_CONTINUE;

	if (but->flag & UI_BUT_DISABLED)
		return WM_UI_HANDLER_CONTINUE;

	if ((data->state == BUTTON_STATE_HIGHLIGHT) &&
	    /* check prevval because of modal operators [#24016],
	     * modifier check is to allow Ctrl+C for copy.
	     * if this causes other problems, remove this check and suffer the bug :) - campbell */
	    ((event->prevval != KM_PRESS) || (ISKEYMODIFIER(event->prevtype)) || (event->type == EVT_DROP)))
	{
		/* handle copy-paste */
		if (ELEM(event->type, CKEY, VKEY) && event->val == KM_PRESS && (event->ctrl || event->oskey)) {
			ui_but_copy_paste(C, but, data, (event->type == CKEY) ? 'c' : 'v');
			return WM_UI_HANDLER_BREAK;
		}
		/* handle drop */
		else if (event->type == EVT_DROP) {
			ui_but_drop(C, event, but, data);
		}
		/* handle keyframing */
		else if (event->type == IKEY && !ELEM(KM_MOD_FIRST, event->ctrl, event->oskey) && event->val == KM_PRESS) {
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
			
			ED_region_tag_redraw(CTX_wm_region(C));
			
			return WM_UI_HANDLER_BREAK;
		}
		/* handle drivers */
		else if (event->type == DKEY && !ELEM3(KM_MOD_FIRST, event->ctrl, event->oskey, event->shift) && event->val == KM_PRESS) {
			if (event->alt)
				ui_but_anim_remove_driver(C);
			else
				ui_but_anim_add_driver(C);
				
			ED_region_tag_redraw(CTX_wm_region(C));
			
			return WM_UI_HANDLER_BREAK;
		}
		/* handle keyingsets */
		else if (event->type == KKEY && !ELEM3(KM_MOD_FIRST, event->ctrl, event->oskey, event->shift) && event->val == KM_PRESS) {
			if (event->alt)
				ui_but_anim_remove_keyingset(C);
			else
				ui_but_anim_add_keyingset(C);
				
			ED_region_tag_redraw(CTX_wm_region(C));
			
			return WM_UI_HANDLER_BREAK;
		}
		/* reset to default */
		/* XXX hardcoded keymap check.... */
		else if (event->type == BACKSPACEKEY && event->val == KM_PRESS) {
			/* ctrl+backspace = reset active button; backspace = reset a whole array*/
			if (!(ELEM3(but->type, HSVCIRCLE, HSVCUBE, HISTOGRAM)))
				ui_set_but_default(C, !event->ctrl);
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
		if (but->lock) {
			if (but->lockstr) {
				BKE_report(NULL, RPT_WARNING, but->lockstr);
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
		case TOGR:
		case ICONTOG:
		case ICONTOGN:
		case TOGN:
		case BUT_TOGDUAL:
		case OPTION:
		case OPTIONN:
			retval = ui_do_but_TOG(C, but, data, event);
			break;
		case SCROLL:
			retval = ui_do_but_SCROLL(C, block, but, data, event);
			break;
		case NUM:
		case NUMABS:
			retval = ui_do_but_NUM(C, block, but, data, event);
			break;
		case SLI:
		case NUMSLI:
		case HSVSLI:
			retval = ui_do_but_SLI(C, block, but, data, event);
			break;
		case ROUNDBOX:
		case LISTBOX:
		case LABEL:
		case TOG3:
		case ROW:
		case LISTROW:
		case BUT_IMAGE:
		case PROGRESSBAR:
			retval = ui_do_but_EXIT(C, but, data, event);
			break;
		case HISTOGRAM:
			retval = ui_do_but_HISTOGRAM(C, block, but, data, event);
			break;
		case WAVEFORM:
			retval = ui_do_but_WAVEFORM(C, block, but, data, event);
			break;
		case VECTORSCOPE:
			retval = ui_do_but_VECTORSCOPE(C, block, but, data, event);
			break;
		case TEX:
		case IDPOIN:
		case SEARCH_MENU:
			retval = ui_do_but_TEX(C, block, but, data, event);
			break;
		case MENU:
		case ICONROW:
		case ICONTEXTROW:
		case BLOCK:
		case PULLDOWN:
			retval = ui_do_but_BLOCK(C, but, data, event);
			break;
		case BUTM:
			retval = ui_do_but_BUT(C, but, data, event);
			break;
		case COL:
			if (but->a1 == UI_GRAD_V_ALT) // signal to prevent calling up color picker
				retval = ui_do_but_EXIT(C, but, data, event);
			else
				retval = ui_do_but_BLOCK(C, but, data, event);
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
#ifdef WITH_INTERNATIONAL
		case CHARTAB:
			retval = ui_do_but_CHARTAB(C, block, but, data, event);
			break;
#endif

		case LINK:
		case INLINK:
			retval = ui_do_but_LINK(C, but, data, event);
			break;
		case TRACKPREVIEW:
			retval = ui_do_but_TRACKPREVIEW(C, block, but, data, event);
			break;
	}
	
	return retval;
}

/* ************************ button utilities *********************** */

static int ui_but_contains_pt(uiBut *but, int mx, int my)
{
	return ((but->x1 < mx && but->x2 >= mx) && (but->y1 < my && but->y2 >= my));
}

static uiBut *ui_but_find_activated(ARegion *ar)
{
	uiBlock *block;
	uiBut *but;

	for (block = ar->uiblocks.first; block; block = block->next)
		for (but = block->buttons.first; but; but = but->next)
			if (but->active)
				return but;

	return NULL;
}

int ui_button_is_active(ARegion *ar)
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



/* returns TRUE if highlighted button allows drop of names */
/* called in region context */
int UI_but_active_drop_name(bContext *C)
{
	ARegion *ar = CTX_wm_region(C);
	uiBut *but = ui_but_find_activated(ar);

	if (but) {
		if (ELEM3(but->type, TEX, IDPOIN, SEARCH_MENU))
			return 1;
	}
	
	return 0;
}

static void ui_blocks_set_tooltips(ARegion *ar, int enable)
{
	uiBlock *block;

	if (!ar)
		return;

	/* we disabled buttons when when they were already shown, and
	 * re-enable them on mouse move */
	for (block = ar->uiblocks.first; block; block = block->next)
		block->tooltipdisabled = !enable;
}

static int ui_mouse_inside_region(ARegion *ar, int x, int y)
{
	uiBlock *block;
	
	/* check if the mouse is in the region */
	if (!BLI_in_rcti(&ar->winrct, x, y)) {
		for (block = ar->uiblocks.first; block; block = block->next)
			block->auto_open = FALSE;
		
		return 0;
	}

	/* also, check that with view2d, that the mouse is not over the scrollbars 
	 * NOTE: care is needed here, since the mask rect may include the scrollbars
	 * even when they are not visible, so we need to make a copy of the mask to
	 * use to check
	 */
	if (ar->v2d.mask.xmin != ar->v2d.mask.xmax) {
		View2D *v2d = &ar->v2d;
		rcti mask_rct;
		int mx, my;
		
		/* convert window coordinates to region coordinates */
		mx = x;
		my = y;
		ui_window_to_region(ar, &mx, &my);
		
		/* make a copy of the mask rect, and tweak accordingly for hidden scrollbars */
		mask_rct.xmin = v2d->mask.xmin;
		mask_rct.xmax = v2d->mask.xmax;
		mask_rct.ymin = v2d->mask.ymin;
		mask_rct.ymax = v2d->mask.ymax;
		
		if (v2d->scroll & (V2D_SCROLL_VERTICAL_HIDE | V2D_SCROLL_VERTICAL_FULLR)) {
			if (v2d->scroll & V2D_SCROLL_LEFT)
				mask_rct.xmin = v2d->vert.xmin;
			else if (v2d->scroll & V2D_SCROLL_RIGHT)
				mask_rct.xmax = v2d->vert.xmax;
		}
		if (v2d->scroll & (V2D_SCROLL_HORIZONTAL_HIDE | V2D_SCROLL_HORIZONTAL_FULLR)) {
			if (v2d->scroll & (V2D_SCROLL_BOTTOM | V2D_SCROLL_BOTTOM_O))
				mask_rct.ymin = v2d->hor.ymin;
			else if (v2d->scroll & V2D_SCROLL_TOP)
				mask_rct.ymax = v2d->hor.ymax;
		}
		
		/* check if in the rect */
		if (!BLI_in_rcti(&mask_rct, mx, my)) 
			return 0;
	}
	
	return 1;
}

static int ui_mouse_inside_button(ARegion *ar, uiBut *but, int x, int y)
{
	if (!ui_mouse_inside_region(ar, x, y))
		return 0;

	ui_window_to_block(ar, but->block, &x, &y);

	if (!ui_but_contains_pt(but, x, y))
		return 0;
	
	return 1;
}

static uiBut *ui_but_find_mouse_over(ARegion *ar, int x, int y)
{
	uiBlock *block;
	uiBut *but, *butover = NULL;
	int mx, my;

//	if (!win->active)
//		return NULL;
	if (!ui_mouse_inside_region(ar, x, y))
		return NULL;

	for (block = ar->uiblocks.first; block; block = block->next) {
		mx = x;
		my = y;
		ui_window_to_block(ar, block, &mx, &my);

		for (but = block->buttons.first; but; but = but->next) {
			/* note, LABEL is included for highlights, this allows drags */
			if (but->type == LABEL && but->dragpoin == NULL)
				continue;
			if (ELEM3(but->type, ROUNDBOX, SEPR, LISTBOX))
				continue;
			if (but->flag & UI_HIDDEN)
				continue;
			if (but->flag & UI_SCROLLED)
				continue;
			if (ui_but_contains_pt(but, mx, my))
				butover = but;
		}

		/* CLIP_EVENTS prevents the event from reaching other blocks */
		if (block->flag & UI_BLOCK_CLIP_EVENTS) {
			/* check if mouse is inside block */
			if (block->minx <= mx && block->maxx >= mx &&
			    block->miny <= my && block->maxy >= my)
			{
				break;
			}
		}
	}

	return butover;
}

static uiBut *ui_list_find_mouse_over(ARegion *ar, int x, int y)
{
	uiBlock *block;
	uiBut *but;
	int mx, my;

//	if (!win->active)
//		return NULL;
	if (!ui_mouse_inside_region(ar, x, y))
		return NULL;

	for (block = ar->uiblocks.first; block; block = block->next) {
		mx = x;
		my = y;
		ui_window_to_block(ar, block, &mx, &my);

		for (but = block->buttons.last; but; but = but->prev)
			if (but->type == LISTBOX && ui_but_contains_pt(but, mx, my))
				return but;
	}

	return NULL;
}

/* ****************** button state handling **************************/

static int button_modal_state(uiHandleButtonState state)
{
	return ELEM6(state, BUTTON_STATE_WAIT_RELEASE, BUTTON_STATE_WAIT_KEY_EVENT,
	             BUTTON_STATE_NUM_EDITING, BUTTON_STATE_TEXT_EDITING,
	             BUTTON_STATE_TEXT_SELECTING, BUTTON_STATE_MENU_OPEN);
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
		if (ELEM3(but->type, BLOCK, PULLDOWN, ICONTEXTROW)) {
			if (data->used_mouse && !data->autoopentimer) {
				int time;

				if (but->block->auto_open == TRUE) time = 1;  // test for toolbox
				else if ((but->block->flag & UI_BLOCK_LOOP && but->type != BLOCK) || but->block->auto_open == TRUE) time = 5 * U.menuthreshold2;
				else if (U.uiflag & USER_MENUOPENAUTO) time = 5 * U.menuthreshold1;
				else time = -1;

				if (time >= 0)
					data->autoopentimer = WM_event_add_timer(data->wm, data->window, TIMER, 0.02 * (double)time);
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
			WM_cursor_grab(CTX_wm_window(C), TRUE, TRUE, NULL);
		ui_numedit_begin(but, data);
	}
	else if (data->state == BUTTON_STATE_NUM_EDITING) {
		ui_numedit_end(but, data);
		if (ui_is_a_warp_but(but))
			WM_cursor_ungrab(CTX_wm_window(C));
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
				/* TRUE = postpone free */
				WM_event_remove_ui_handler(&data->window->modalhandlers, ui_handler_region_menu, NULL, data, TRUE);
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
	if (ELEM(but->type, BUT_CURVE, SEARCH_MENU) ) ;  // XXX curve is temp
	else data->interactive = 1;
	
	data->state = BUTTON_STATE_INIT;

	/* activate button */
	but->flag |= UI_ACTIVE;
	but->active = data;

	/* we disable auto_open in the block after a threshold, because we still
	 * want to allow auto opening adjacent menus even if no button is activated
	 * in between going over to the other button, but only for a short while */
	if (type == BUTTON_ACTIVATE_OVER && but->block->auto_open == TRUE)
		if (but->block->auto_open_last + BUTTON_AUTO_OPEN_THRESH < PIL_check_seconds_timer())
			but->block->auto_open = FALSE;

	if (type == BUTTON_ACTIVATE_OVER) {
		data->used_mouse = TRUE;
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
}

static void button_activate_exit(bContext *C, uiHandleButtonData *data, uiBut *but, int mousemove, int onfree)
{
	uiBlock *block = but->block;
	uiBut *bt;

	/* ensure we are in the exit state */
	if (data->state != BUTTON_STATE_EXIT)
		button_activate_state(C, but, BUTTON_STATE_EXIT);

	/* apply the button action or value */
	if (!onfree)
		ui_apply_button(C, block, but, data, 0);

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
		ui_apply_autokey_undo(C, but);

		/* popup menu memory */
		if (block->flag & UI_BLOCK_POPUP_MEMORY)
			ui_popup_menu_memory(block, but);
	}

	/* disable tooltips until mousemove + last active flag */
	for (block = data->region->uiblocks.first; block; block = block->next) {
		for (bt = block->buttons.first; bt; bt = bt->next)
			bt->flag &= ~UI_BUT_LAST_ACTIVE;

		block->tooltipdisabled = 1;
	}

	ui_blocks_set_tooltips(data->region, 0);

	/* clean up */
	if (data->str)
		MEM_freeN(data->str);
	if (data->origstr)
		MEM_freeN(data->origstr);

	/* redraw (data is but->active!) */
	ED_region_tag_redraw(data->region);
	
	/* clean up button */
	MEM_freeN(but->active);
	but->active = NULL;
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
		data->cancel = 1;
		button_activate_exit((bContext *)C, data, but, 0, 1);
	}
}

/* returns the active button with an optional checking function */
static uiBut *ui_context_button_active(const bContext *C, int (*but_check_cb)(uiBut *))
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

static int ui_context_rna_button_active_test(uiBut *but)
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
			block->handle_func(C, block->handle_func_arg, 0);
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
				
				if (but->active)
					activebut = but;
				else if (!activebut && (but->flag & UI_BUT_LAST_ACTIVE))
					activebut = but;
			}
		}

		if (activebut) {
			/* always recurse into opened menu, so all buttons update (like colorpicker) */
			uiHandleButtonData *data = activebut->active;
			if (data && data->menu)
				ar = data->menu->region;
			else
				return;
		}
		else {
			/* no active button */
			return;
		}
	}
}

/************** handle activating a button *************/

static uiBut *uit_but_find_open_event(ARegion *ar, wmEvent *event)
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

static int ui_handle_button_over(bContext *C, wmEvent *event, ARegion *ar)
{
	uiBut *but;

	if (event->type == MOUSEMOVE) {
		but = ui_but_find_mouse_over(ar, event->x, event->y);
		if (but)
			button_activate_init(C, ar, but, BUTTON_ACTIVATE_OVER);
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
	
	event = *(win->eventstate);  /* XXX huh huh? make api call */
	event.type = EVT_BUT_OPEN;
	event.val = KM_PRESS;
	event.customdata = but;
	event.customdatafree = FALSE;
	
	ui_do_button(C, but->block, but, &event);
}

static void ui_handle_button_activate(bContext *C, ARegion *ar, uiBut *but, uiButtonActivateType type)
{
	uiBut *oldbut;
	uiHandleButtonData *data;

	oldbut = ui_but_find_activated(ar);
	if (oldbut) {
		data = oldbut->active;
		data->cancel = 1;
		button_activate_exit(C, data, oldbut, 0, 0);
	}

	button_activate_init(C, ar, but, type);
}

/************ handle events for an activated button ***********/

static int ui_handle_button_event(bContext *C, wmEvent *event, uiBut *but)
{
	uiHandleButtonData *data;
	uiBlock *block;
	ARegion *ar;
	uiBut *postbut;
	uiButtonActivateType posttype;
	int retval;

	data = but->active;
	block = but->block;
	ar = data->region;

	retval = WM_UI_HANDLER_CONTINUE;
	
	if (data->state == BUTTON_STATE_HIGHLIGHT) {
		switch (event->type) {
			case WINDEACTIVATE:
			case EVT_BUT_CANCEL:
				data->cancel = 1;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				retval = WM_UI_HANDLER_CONTINUE;
				break;
			case MOUSEMOVE:
				/* verify if we are still over the button, if not exit */
				if (!ui_mouse_inside_button(ar, but, event->x, event->y)) {
					data->cancel = 1;
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				else if (ui_but_find_mouse_over(ar, event->x, event->y) != but) {
					data->cancel = 1;
					button_activate_state(C, but, BUTTON_STATE_EXIT);
				}
				else if (event->x != event->prevx || event->y != event->prevy) {
					/* re-enable tooltip on mouse move */
					ui_blocks_set_tooltips(ar, 1);
					button_tooltip_timer_reset(C, but);
				}

				break;
			case TIMER: {
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
				case WHEELUPMOUSE:
				case WHEELDOWNMOUSE:
				case MIDDLEMOUSE:
					/* XXX hardcoded keymap check... but anyway, while view changes, tooltips should be removed */
					if (data->tooltiptimer) {
						WM_event_remove_timer(data->wm, data->window, data->tooltiptimer);
						data->tooltiptimer = NULL;
					}
				/* pass on purposedly */
				default:
					/* handle button type specific events */
					retval = ui_do_button(C, block, but, event);
			}
		}
	}
	else if (data->state == BUTTON_STATE_WAIT_RELEASE) {
		switch (event->type) {
			case WINDEACTIVATE:
				data->cancel = 1;
				button_activate_state(C, but, BUTTON_STATE_EXIT);
				break;

			case MOUSEMOVE:
				if (ELEM(but->type, LINK, INLINK)) {
					but->flag |= UI_SELECT;
					ui_do_button(C, block, but, event);
					ED_region_tag_redraw(data->region);
				}
				else {
					/* deselect the button when moving the mouse away */
					/* also de-activate for buttons that only show higlights */
					if (ui_mouse_inside_button(ar, but, event->x, event->y)) {
						if (!(but->flag & UI_SELECT)) {
							but->flag |= (UI_SELECT | UI_ACTIVE);
							data->cancel = 0;
							ED_region_tag_redraw(data->region);
						}
					}
					else {
						if (but->flag & UI_SELECT) {
							but->flag &= ~(UI_SELECT | UI_ACTIVE);
							data->cancel = 1;
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
			case TIMER: {
				if (event->customdata == data->flashtimer)
					button_activate_state(C, but, BUTTON_STATE_EXIT);
			}
		}

		retval = WM_UI_HANDLER_CONTINUE;
	}
	else if (data->state == BUTTON_STATE_MENU_OPEN) {
		/* check for exit because of mouse-over another button */
		switch (event->type) {
			case MOUSEMOVE:
				
				if (data->menu && data->menu->region)
					if (ui_mouse_inside_region(data->menu->region, event->x, event->y))
						break;
			
				{
					uiBut *bt = ui_but_find_mouse_over(ar, event->x, event->y);

					if (bt && bt->active != data) {
						if (but->type != COL) /* exception */
							data->cancel = 1;
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
		// retval= WM_UI_HANDLER_BREAK; XXX why ? 
	}

	if (data->state == BUTTON_STATE_EXIT) {
		postbut = data->postbut;
		posttype = data->posttype;

		button_activate_exit(C, data, but, (postbut == NULL), 0);

		/* for jumping to the next button with tab while text editing */
		if (postbut)
			button_activate_init(C, ar, postbut, posttype);
	}

	return retval;
}

static int ui_handle_list_event(bContext *C, wmEvent *event, ARegion *ar)
{
	uiBut *but = ui_list_find_mouse_over(ar, event->x, event->y);
	int retval = WM_UI_HANDLER_CONTINUE;
	int value, min, max;

	if (but && (event->val == KM_PRESS)) {
		Panel *pa = but->block->panel;

		if (ELEM(event->type, UPARROWKEY, DOWNARROWKEY) ||
		    ((ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE) && event->alt)))
		{
			/* activate up/down the list */
			value = RNA_property_int_get(&but->rnapoin, but->rnaprop);

			if (ELEM(event->type, UPARROWKEY, WHEELUPMOUSE))
				value--;
			else
				value++;

			if (value < pa->list_scroll)
				pa->list_scroll = value;
			else if (value >= pa->list_scroll + pa->list_size)
				pa->list_scroll = value - pa->list_size + 1;

			RNA_property_int_range(&but->rnapoin, but->rnaprop, &min, &max);
			value = CLAMPIS(value, min, max);

			RNA_property_int_set(&but->rnapoin, but->rnaprop, value);
			RNA_property_update(C, &but->rnapoin, but->rnaprop);
			ED_region_tag_redraw(ar);

			retval = WM_UI_HANDLER_BREAK;
		}
		else if (ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE) && event->shift) {
			/* silly replacement for proper grip */
			if (pa->list_grip_size == 0)
				pa->list_grip_size = pa->list_size;

			if (event->type == WHEELUPMOUSE)
				pa->list_grip_size--;
			else
				pa->list_grip_size++;

			pa->list_grip_size = MAX2(pa->list_grip_size, 1);

			ED_region_tag_redraw(ar);

			retval = WM_UI_HANDLER_BREAK;
		}
		else if (ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE)) {
			if (pa->list_last_len > pa->list_size) {
				/* list template will clamp */
				if (event->type == WHEELUPMOUSE)
					pa->list_scroll--;
				else
					pa->list_scroll++;

				ED_region_tag_redraw(ar);

				retval = WM_UI_HANDLER_BREAK;
			}
		}
	}

	return retval;
}

static void ui_handle_button_return_submenu(bContext *C, wmEvent *event, uiBut *but)
{
	uiHandleButtonData *data;
	uiPopupBlockHandle *menu;

	data = but->active;
	menu = data->menu;

	/* copy over return values from the closing menu */
	if ((menu->menuretval & UI_RETURN_OK) || (menu->menuretval & UI_RETURN_UPDATE)) {
		if (but->type == COL)
			copy_v3_v3(data->vec, menu->retvec);
		else if (ELEM3(but->type, MENU, ICONROW, ICONTEXTROW))
			data->value = menu->retvalue;
	}

	if (menu->menuretval & UI_RETURN_UPDATE) {
		if (data->interactive) ui_apply_button(C, but->block, but, data, 1);
		else ui_check_but(but);

		menu->menuretval = 0;
	}
	
	/* now change button state or exit, which will close the submenu */
	if ((menu->menuretval & UI_RETURN_OK) || (menu->menuretval & UI_RETURN_CANCEL)) {
		if (menu->menuretval != UI_RETURN_OK)
			data->cancel = 1;

		button_activate_exit(C, data, but, 1, 0);
	}
	else if (menu->menuretval & UI_RETURN_OUT) {
		if (event->type == MOUSEMOVE && ui_mouse_inside_button(data->region, but, event->x, event->y)) {
			button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);
		}
		else {
			if (ISKEYBOARD(event->type)) {
				/* keyboard menu hierarchy navigation, going back to previous level */
				but->active->used_mouse = FALSE;
				button_activate_state(C, but, BUTTON_STATE_HIGHLIGHT);
			}
			else {
				data->cancel = 1;
				button_activate_exit(C, data, but, 1, 0);
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

static void ui_mouse_motion_towards_init(uiPopupBlockHandle *menu, int mx, int my, int force)
{
	if (!menu->dotowards || force) {
		menu->dotowards = 1;
		menu->towardsx = mx;
		menu->towardsy = my;

		if (force)
			menu->towardstime = DBL_MAX;  /* unlimited time */
		else
			menu->towardstime = PIL_check_seconds_timer();
	}
}

static int ui_mouse_motion_towards_check(uiBlock *block, uiPopupBlockHandle *menu, int mx, int my)
{
	float p1[2], p2[2], p3[2], p4[2], oldp[2], newp[2];
	int closer;

	if (!menu->dotowards) return 0;

	/* verify that we are moving towards one of the edges of the
	 * menu block, in other words, in the triangle formed by the
	 * initial mouse location and two edge points. */
	p1[0] = block->minx - 20;
	p1[1] = block->miny - 20;

	p2[0] = block->maxx + 20;
	p2[1] = block->miny - 20;
	
	p3[0] = block->maxx + 20;
	p3[1] = block->maxy + 20;

	p4[0] = block->minx - 20;
	p4[1] = block->maxy + 20;

	oldp[0] = menu->towardsx;
	oldp[1] = menu->towardsy;

	newp[0] = mx;
	newp[1] = my;

	if (len_squared_v2v2(oldp, newp) < (4.0f * 4.0f))
		return menu->dotowards;

	closer = (isect_point_tri_v2(newp, oldp, p1, p2) ||
	          isect_point_tri_v2(newp, oldp, p2, p3) ||
	          isect_point_tri_v2(newp, oldp, p3, p4) ||
	          isect_point_tri_v2(newp, oldp, p4, p1));

	if (!closer)
		menu->dotowards = 0;

	/* 1 second timer */
	if (PIL_check_seconds_timer() - menu->towardstime > BUTTON_MOUSE_TOWARDS_THRESH)
		menu->dotowards = 0;

	return menu->dotowards;
}

static char ui_menu_scroll_test(uiBlock *block, int my)
{
	if (block->flag & (UI_BLOCK_CLIPTOP | UI_BLOCK_CLIPBOTTOM)) {
		if (block->flag & UI_BLOCK_CLIPTOP) 
			if (my > block->maxy - 14)
				return 't';
		if (block->flag & UI_BLOCK_CLIPBOTTOM)
			if (my < block->miny + 14)
				return 'b';
	}
	return 0;
}

static int ui_menu_scroll(ARegion *ar, uiBlock *block, int my)
{
	char test = ui_menu_scroll_test(block, my);
	
	if (test) {
		uiBut *b1 = block->buttons.first;
		uiBut *b2 = block->buttons.last;
		uiBut *bnext;
		uiBut *bprev;
		int dy = 0;
		
		/* get first and last visible buttons */
		while (b1 && ui_but_next(b1) && (b1->flag & UI_SCROLLED))
			b1 = ui_but_next(b1);
		while (b2 && ui_but_prev(b2) && (b2->flag & UI_SCROLLED))
			b2 = ui_but_prev(b2);
		/* skips separators */
		bnext = ui_but_next(b1);
		bprev = ui_but_prev(b2);
		
		if (bnext == NULL || bprev == NULL)
			return 0;
		
		if (test == 't') {
			/* bottom button is first button */
			if (b1->y1 < b2->y1)
				dy = bnext->y1 - b1->y1;
			/* bottom button is last button */
			else 
				dy = bprev->y1 - b2->y1;
		}
		else if (test == 'b') {
			/* bottom button is first button */
			if (b1->y1 < b2->y1)
				dy = b1->y1 - bnext->y1;
			/* bottom button is last button */
			else 
				dy = b2->y1 - bprev->y1;
		}
		if (dy) {
			
			for (b1 = block->buttons.first; b1; b1 = b1->next) {
				b1->y1 -= dy;
				b1->y2 -= dy;
			}
			/* set flags again */
			ui_popup_block_scrolltest(block);
			
			ED_region_tag_redraw(ar);
			
			return 1;
		}
	}
	
	return 0;
}

static int ui_handle_menu_event(bContext *C, wmEvent *event, uiPopupBlockHandle *menu, int UNUSED(topmenu))
{
	ARegion *ar;
	uiBlock *block;
	uiBut *but, *bt;
	int inside, act, count, mx, my, retval;

	ar = menu->region;
	block = ar->uiblocks.first;
	
	act = 0;
	retval = WM_UI_HANDLER_CONTINUE;

	mx = event->x;
	my = event->y;
	ui_window_to_block(ar, block, &mx, &my);

	/* check if mouse is inside block */
	inside = 0;
	if (block->minx <= mx && block->maxx >= mx)
		if (block->miny <= my && block->maxy >= my)
			inside = 1;

	/* if there's an active modal button, don't check events or outside, except for search menu */
	but = ui_but_find_activated(ar);
	if (but && button_modal_state(but->active->state) && but->type != SEARCH_MENU) {
		/* if a button is activated modal, always reset the start mouse
		 * position of the towards mechanism to avoid loosing focus,
		 * and don't handle events */
		ui_mouse_motion_towards_init(menu, mx, my, 1);
	}
	else if (event->type == TIMER) {
		if (event->customdata == menu->scrolltimer)
			ui_menu_scroll(ar, block, my);
	}
	else {
		/* for ui_mouse_motion_towards_block */
		if (event->type == MOUSEMOVE) {
			ui_mouse_motion_towards_init(menu, mx, my, 0);
			
			/* add menu scroll timer, if needed */
			if (ui_menu_scroll_test(block, my))
				if (menu->scrolltimer == NULL)
					menu->scrolltimer =
					    WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, MENU_SCROLL_INTERVAL);
		}
		
		/* first block own event func */
		if (block->block_event_func && block->block_event_func(C, block, event)) ;
		/* events not for active search menu button */
		else if (but == NULL || but->type != SEARCH_MENU) {
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
					/* arrowkeys: only handle for block_loop blocks */
					if (event->alt || event->shift || event->ctrl || event->oskey) ;
					else if (inside || (block->flag & UI_BLOCK_LOOP)) {
						if (event->val == KM_PRESS) {
							but = ui_but_find_activated(ar);
							if (but) {
								/* is there a situation where UI_LEFT or UI_RIGHT would also change navigation direction? */
								if (((ELEM(event->type, DOWNARROWKEY, WHEELDOWNMOUSE)) && (block->direction & UI_DOWN)) ||
								    ((ELEM(event->type, DOWNARROWKEY, WHEELDOWNMOUSE)) && (block->direction & UI_RIGHT)) ||
								    ((ELEM(event->type, UPARROWKEY, WHEELUPMOUSE)) && (block->direction & UI_TOP)))
								{
									/* the following is just a hack - uiBut->type set to BUT and BUTM have there menus built 
									 * opposite ways - this should be changed so that all popup-menus use the same uiBlock->direction */
									if (but->type & BUT)
										but = ui_but_next(but);
									else
										but = ui_but_prev(but);
								}
								else {
									if (but->type & BUT)
										but = ui_but_prev(but);
									else
										but = ui_but_next(but);
								}

								if (but)
									ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE);
							}

							if (!but) {
								if (((ELEM(event->type, UPARROWKEY, WHEELUPMOUSE)) && (block->direction & UI_DOWN)) ||
								    ((ELEM(event->type, UPARROWKEY, WHEELUPMOUSE)) && (block->direction & UI_RIGHT)) ||
								    ((ELEM(event->type, DOWNARROWKEY, WHEELDOWNMOUSE)) && (block->direction & UI_TOP)))
								{
									if ((bt = ui_but_first(block)) && (bt->type & BUT)) {
										bt = ui_but_last(block);
									}
									else {
										/* keep ui_but_first() */
									}
								}
								else {
									if ((bt = ui_but_first(block)) && (bt->type & BUT)) {
										/* keep ui_but_first() */
									}
									else {
										bt = ui_but_last(block);
									}
								}

								if (bt)
									ui_handle_button_activate(C, ar, bt, BUTTON_ACTIVATE);
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
						if (event->alt) act += 10;

						count = 0;
						for (but = block->buttons.first; but; but = but->next) {
							int doit = 0;
							
							if (but->type != LABEL && but->type != SEPR)
								count++;
							
							/* exception for rna layer buts */
							if (but->rnapoin.data && but->rnaprop) {
								if (ELEM(RNA_property_subtype(but->rnaprop), PROP_LAYER, PROP_LAYER_MEMBER)) {
									if (but->rnaindex == act - 1)
										doit = 1;
								}
							}
							/* exception for menus like layer buts, with button aligning they're not drawn in order */
							else if (but->type == TOGR) {
								if (but->bitnr == act - 1)
									doit = 1;
							}
							else if (count == act)
								doit = 1;
							
							if (doit) {
								ui_handle_button_activate(C, ar, but, BUTTON_ACTIVATE_APPLY);
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
					if ((event->val   == KM_PRESS) &&
					    (event->shift == FALSE) &&
					    (event->ctrl  == FALSE) &&
					    (event->oskey == FALSE))
					{
						for (but = block->buttons.first; but; but = but->next) {

							if (but->menu_key == event->type) {
								if (but->type == BUT) {
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
			 * button that opened us, otherwise we need to close */
			if (inside == 0) {
				uiSafetyRct *saferct = block->saferct.first;

				if (ELEM3(event->type, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE) && event->val == KM_PRESS) {
					if (saferct && !BLI_in_rctf(&saferct->parent, event->x, event->y)) {
						if (block->flag & (UI_BLOCK_OUT_1))
							menu->menuretval = UI_RETURN_OK;
						else
							menu->menuretval = UI_RETURN_OUT;
					}
				}
			}

			if (menu->menuretval) ;
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
			else {
				ui_mouse_motion_towards_check(block, menu, mx, my);

				/* check mouse moving outside of the menu */
				if (inside == 0 && (block->flag & UI_BLOCK_MOVEMOUSE_QUIT)) {
					uiSafetyRct *saferct;
					
					/* check for all parent rects, enables arrowkeys to be used */
					for (saferct = block->saferct.first; saferct; saferct = saferct->next) {
						/* for mouse move we only check our own rect, for other
						 * events we check all preceding block rects too to make
						 * arrow keys navigation work */
						if (event->type != MOUSEMOVE || saferct == block->saferct.first) {
							if (BLI_in_rctf(&saferct->parent, (float)event->x, (float)event->y))
								break;
							if (BLI_in_rctf(&saferct->safety, (float)event->x, (float)event->y))
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
		}
	}

	/* if we are didn't handle the event yet, lets pass it on to
	 * buttons inside this region. disabled inside check .. not sure
	 * anymore why it was there? but it meant enter didn't work
	 * for example when mouse was not over submenu */
	if ((/*inside &&*/ (!menu->menuretval || (menu->menuretval & UI_RETURN_UPDATE)) && retval == WM_UI_HANDLER_CONTINUE) || event->type == TIMER) {
		but = ui_but_find_activated(ar);

		if (but) {
			ScrArea *ctx_area = CTX_wm_area(C);
			ARegion *ctx_region = CTX_wm_region(C);
			
			if (menu->ctx_area) CTX_wm_area_set(C, menu->ctx_area);
			if (menu->ctx_region) CTX_wm_region_set(C, menu->ctx_region);
			
			retval = ui_handle_button_event(C, event, but);
			
			if (menu->ctx_area) CTX_wm_area_set(C, ctx_area);
			if (menu->ctx_region) CTX_wm_region_set(C, ctx_region);
		}
		else
			retval = ui_handle_button_over(C, event, ar);
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

static int ui_handle_menu_return_submenu(bContext *C, wmEvent *event, uiPopupBlockHandle *menu)
{
	ARegion *ar;
	uiBut *but;
	uiBlock *block;
	uiHandleButtonData *data;
	uiPopupBlockHandle *submenu;
	int mx, my, update;

	ar = menu->region;
	block = ar->uiblocks.first;

	but = ui_but_find_activated(ar);
	data = but->active;
	submenu = data->menu;

	if (submenu->menuretval) {
		/* first decide if we want to close our own menu cascading, if
		 * so pass on the sub menu return value to our own menu handle */
		if ((submenu->menuretval & UI_RETURN_OK) || (submenu->menuretval & UI_RETURN_CANCEL)) {
			if (!(block->flag & UI_BLOCK_KEEP_OPEN)) {
				menu->menuretval = submenu->menuretval;
				menu->butretval = data->retval;
			}
		}

		update = (submenu->menuretval & UI_RETURN_UPDATE);

		/* now let activated button in this menu exit, which
		 * will actually close the submenu too */
		ui_handle_button_return_submenu(C, event, but);

		if (update)
			submenu->menuretval = 0;
	}

	/* for cases where close does not cascade, allow the user to
	 * move the mouse back towards the menu without closing */
	mx = event->x;
	my = event->y;
	ui_window_to_block(ar, block, &mx, &my);
	ui_mouse_motion_towards_init(menu, mx, my, 1);

	if (menu->menuretval)
		return WM_UI_HANDLER_CONTINUE;
	else
		return WM_UI_HANDLER_BREAK;
}

static int ui_handle_menus_recursive(bContext *C, wmEvent *event, uiPopupBlockHandle *menu)
{
	uiBut *but;
	uiHandleButtonData *data;
	uiPopupBlockHandle *submenu;
	int retval = WM_UI_HANDLER_CONTINUE;

	/* check if we have a submenu, and handle events for it first */
	but = ui_but_find_activated(menu->region);
	data = (but) ? but->active : NULL;
	submenu = (data) ? data->menu : NULL;

	if (submenu)
		retval = ui_handle_menus_recursive(C, event, submenu);

	/* now handle events for our own menu */
	if (retval == WM_UI_HANDLER_CONTINUE || event->type == TIMER) {
		if (submenu && submenu->menuretval)
			retval = ui_handle_menu_return_submenu(C, event, menu);
		else
			retval = ui_handle_menu_event(C, event, menu, (submenu == NULL));
	}

	return retval;
}

/* *************** UI event handlers **************** */

static int ui_handler_region(bContext *C, wmEvent *event, void *UNUSED(userdata))
{
	ARegion *ar;
	uiBut *but;
	int retval;

	/* here we handle buttons at the region level, non-modal */
	ar = CTX_wm_region(C);
	retval = WM_UI_HANDLER_CONTINUE;

	if (ar == NULL) return retval;
	if (ar->uiblocks.first == NULL) return retval;

	/* either handle events for already activated button or try to activate */
	but = ui_but_find_activated(ar);

	retval = ui_handler_panel_region(C, event);

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
		ui_blocks_set_tooltips(ar, 1);
	
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

static int ui_handler_region_menu(bContext *C, wmEvent *event, void *UNUSED(userdata))
{
	ARegion *ar;
	uiBut *but;
	uiHandleButtonData *data;
	int retval;

	/* here we handle buttons at the window level, modal, for example
	 * while number sliding, text editing, or when a menu block is open */
	ar = CTX_wm_menu(C);
	if (!ar)
		ar = CTX_wm_region(C);

	but = ui_but_find_activated(ar);

	if (but) {
		/* handle activated button events */
		data = but->active;

		if (data->state == BUTTON_STATE_MENU_OPEN) {
			/* handle events for menus and their buttons recursively,
			 * this will handle events from the top to the bottom menu */
			retval = ui_handle_menus_recursive(C, event, data->menu);

			/* handle events for the activated button */
			if (retval == WM_UI_HANDLER_CONTINUE || event->type == TIMER) {
				if (data->menu->menuretval)
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
		ui_blocks_set_tooltips(ar, 1);

	/* delayed apply callbacks */
	ui_apply_but_funcs_after(C);

	/* we block all events, this is modal interaction */
	return WM_UI_HANDLER_BREAK;
}

/* two types of popups, one with operator + enum, other with regular callbacks */
static int ui_handler_popup(bContext *C, wmEvent *event, void *userdata)
{
	uiPopupBlockHandle *menu = userdata;

	/* we block all events, this is modal interaction, except for drop events which is described below */
	int retval = WM_UI_HANDLER_BREAK;

	if (event->type == EVT_DROP) {
		/* if we're handling drop event we'll want it to be handled by popup callee as well,
		 * so it'll be possible to perform such operations as opening .blend files by dropping
		 * them into blender even if there's opened popup like splash screen (sergey)
		 */

		retval = WM_UI_HANDLER_CONTINUE;
	}

	ui_handle_menus_recursive(C, event, menu);

	/* free if done, does not free handle itself */
	if (menu->menuretval) {
		/* copy values, we have to free first (closes region) */
		uiPopupBlockHandle temp = *menu;
		
		ui_popup_block_free(C, menu);
		UI_remove_popup_handlers(&CTX_wm_window(C)->modalhandlers, menu);

		if ((temp.menuretval & UI_RETURN_OK) || (temp.menuretval & UI_RETURN_POPUP_OK)) {
			if (temp.popup_func)
				temp.popup_func(C, temp.popup_arg, temp.retvalue);
			if (temp.optype)
				WM_operator_name_call(C, temp.optype->idname, temp.opcontext, NULL);
		}
		else if (temp.cancel_func)
			temp.cancel_func(temp.popup_arg);
	}
	else {
		/* re-enable tooltips */
		if (event->type == MOUSEMOVE && (event->x != event->prevx || event->y != event->prevy))
			ui_blocks_set_tooltips(menu->region, 1);
	}

	/* delayed apply callbacks */
	ui_apply_but_funcs_after(C);

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
	WM_event_remove_ui_handler(handlers, ui_handler_region, ui_handler_remove_region, NULL, FALSE);
	WM_event_add_ui_handler(NULL, handlers, ui_handler_region, ui_handler_remove_region, NULL);
}

void UI_add_popup_handlers(bContext *C, ListBase *handlers, uiPopupBlockHandle *popup)
{
	WM_event_add_ui_handler(C, handlers, ui_handler_popup, ui_handler_remove_popup, popup);
}

void UI_remove_popup_handlers(ListBase *handlers, uiPopupBlockHandle *popup)
{
	WM_event_remove_ui_handler(handlers, ui_handler_popup, ui_handler_remove_popup, popup, FALSE);
}


