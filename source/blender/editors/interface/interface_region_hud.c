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

/** \file blender/editors/interface/interface_region_hud.c
 *  \ingroup edinterface
 *
 * Floating Persistent Region
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_string.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_main.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "BLT_translation.h"

#include "ED_screen.h"
#include "ED_undo.h"

#include "interface_intern.h"
#include "GPU_framebuffer.h"


/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

static bool last_redo_poll(const bContext *C)
{
	wmOperator *op = WM_operator_last_redo(C);
	if (op == NULL) {
		return false;
	}
	bool success = false;
	if (WM_operator_repeat_check(C, op) &&
	    WM_operator_check_ui_empty(op->type) == false)
	{
		success = WM_operator_poll((bContext *)C, op->type);
	}
	return success;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Redo Panel
 * \{ */

static bool hud_panel_operator_redo_poll(const bContext *C, PanelType *UNUSED(pt))
{
	return last_redo_poll(C);
}

static void hud_panel_operator_redo_draw_header(const bContext *C, Panel *pa)
{
	wmOperator *op = WM_operator_last_redo(C);
	BLI_strncpy(pa->drawname, RNA_struct_ui_name(op->type->srna), sizeof(pa->drawname));
}

static void hud_panel_operator_redo_draw(const bContext *C, Panel *pa)
{
	wmOperator *op = WM_operator_last_redo(C);
	if (op == NULL) {
		return;
	}
	if (!WM_operator_check_ui_enabled(C, op->type->name)) {
		uiLayoutSetEnabled(pa->layout, false);
	}
	uiLayout *col = uiLayoutColumn(pa->layout, false);
	uiTemplateOperatorRedoProperties(col, C);
}

static void hud_panels_register(ARegionType *art, int space_type, int region_type)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), __func__);
	strcpy(pt->idname, "OPERATOR_PT_redo");
	strcpy(pt->label, N_("Redo"));
	strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw_header = hud_panel_operator_redo_draw_header;
	pt->draw = hud_panel_operator_redo_draw;
	pt->poll = hud_panel_operator_redo_poll;
	pt->space_type = space_type;
	pt->region_type = region_type;
	BLI_addtail(&art->paneltypes, pt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for Floating Region
 * \{ */

struct HudRegionData {
	short regionid;
};

static void hud_region_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_panels_init(wm, ar);
	UI_region_handlers_add(&ar->handlers);
	ar->flag |= RGN_FLAG_TEMP_REGIONDATA;
}

static void hud_region_free(ARegion *ar)
{
	MEM_SAFE_FREE(ar->regiondata);
}

static void hud_region_layout(const bContext *C, ARegion *ar)
{
	bool ok = false;

	{
		struct HudRegionData *hrd = ar->regiondata;
		if (hrd != NULL) {
			ScrArea *sa = CTX_wm_area(C);
			ARegion *ar_op = (hrd->regionid != -1) ? BKE_area_find_region_type(sa, hrd->regionid) : NULL;
			ARegion *ar_prev = CTX_wm_region(C);
			CTX_wm_region_set((bContext *)C, ar_op);
			ok = last_redo_poll(C);
			CTX_wm_region_set((bContext *)C, ar_prev);
		}
	}

	if (!ok) {
		ED_region_tag_redraw(ar);
		ar->flag |= RGN_FLAG_HIDDEN;
		return;
	}

	int size_y = ar->sizey;

	ED_region_panels_layout(C, ar);

	if (ar->panels.first && (ar->sizey != size_y)) {
		View2D *v2d = &ar->v2d;
		ar->winx = ar->sizex;
		ar->winy = ar->sizey;

		ar->winrct.xmax = (ar->winrct.xmin + ar->winx) - 1;
		ar->winrct.ymax = (ar->winrct.ymin + ar->winy) - 1;

		UI_view2d_region_reinit(v2d, V2D_COMMONVIEW_PANELS_UI, ar->winx, ar->winy);

		/* Weak, but needed to avoid glitches, especially with hi-dpi (where resizing the view glitches often).
		 * Fortunately this only happens occasionally. */
		ED_region_panels_layout(C, ar);
	}

	/* restore view matrix */
	UI_view2d_view_restore(C);
}

static void hud_region_draw(const bContext *C, ARegion *ar)
{
	UI_view2d_view_ortho(&ar->v2d);
	wmOrtho2_region_pixelspace(ar);
	GPU_clear_color(0, 0, 0, 0.0f);
	GPU_clear(GPU_COLOR_BIT);

	if ((ar->flag & RGN_FLAG_HIDDEN) == 0) {
		if (0) {
			/* Has alpha flickering glitch, see T56752. */
			ui_draw_menu_back(NULL, NULL, &(rcti){.xmax = ar->winx, .ymax = ar->winy});
		}
		else {
			/* Use basic drawing instead. */
			bTheme *btheme = UI_GetTheme();
			float color[4];
			rgba_uchar_to_float(color, (const uchar *)btheme->tui.wcol_menu_back.inner);
			const float radius = U.widget_unit * btheme->tui.wcol_menu_back.roundness;
			UI_draw_roundbox_corner_set(UI_CNR_ALL);
			UI_draw_roundbox_4fv(true, 0, 0, ar->winx, ar->winy, radius, color);
		}

		ED_region_panels_draw(C, ar);
	}
}

ARegionType *ED_area_type_hud(int space_type)
{
	ARegionType *art = MEM_callocN(sizeof(ARegionType), __func__);
	art->regionid = RGN_TYPE_HUD;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;
	art->layout = hud_region_layout;
	art->draw = hud_region_draw;
	art->init = hud_region_init;
	art->free = hud_region_free;

	hud_panels_register(art, space_type, art->regionid);

	art->lock = 1;   /* can become flag, see BKE_spacedata_draw_locks */
	return art;
}

static ARegion *hud_region_add(ScrArea *sa)
{
	ARegion *ar = MEM_callocN(sizeof(ARegion), "area region");
	ARegion *ar_win = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
	if (ar_win) {
		BLI_insertlinkbefore(&sa->regionbase, ar_win, ar);
	}
	else {
		BLI_addtail(&sa->regionbase, ar);
	}
	ar->regiontype = RGN_TYPE_HUD;
	ar->alignment = RGN_ALIGN_FLOAT;
	ar->overlap = true;
	ar->flag |= RGN_FLAG_DYNAMIC_SIZE;

	return ar;
}

void ED_area_type_hud_clear(wmWindowManager *wm, ScrArea *sa_keep)
{
	for (wmWindow *win = wm->windows.first; win; win = win->next) {
		bScreen *screen = WM_window_get_active_screen(win);
		for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
			if (sa != sa_keep) {
				for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
					if (ar->regiontype == RGN_TYPE_HUD) {
						if ((ar->flag & RGN_FLAG_HIDDEN) == 0) {
							ar->flag |= RGN_FLAG_HIDDEN;
							ED_region_tag_redraw(ar);
							ED_area_tag_redraw(sa);
						}
					}
				}
			}
		}
	}
}

void ED_area_type_hud_ensure(bContext *C, ScrArea *sa)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	ED_area_type_hud_clear(wm, sa);

	ARegionType *art = BKE_regiontype_from_id(sa->type, RGN_TYPE_HUD);
	if (art == NULL) {
		return;
	}

	bool init = false;
	ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_HUD);
	if (!last_redo_poll(C)) {
		if (ar) {
			ED_region_tag_redraw(ar);
			ar->flag |= RGN_FLAG_HIDDEN;
		}
		return;
	}

	if (ar == NULL) {
		init = true;
		ar = hud_region_add(sa);
		ar->type = art;
	}

	/* Let 'ED_area_update_region_sizes' do the work of placing the region.
	 * Otherwise we could set the 'ar->winrct' & 'ar->winx/winy' here. */
	if (init) {
		sa->flag |= AREA_FLAG_REGION_SIZE_UPDATE;
	}
	else {
		if (ar->flag & RGN_FLAG_HIDDEN) {
			sa->flag |= AREA_FLAG_REGION_SIZE_UPDATE;
		}
		ar->flag &= ~RGN_FLAG_HIDDEN;
	}

	{
		ARegion *ar_op = CTX_wm_region(C);
		BLI_assert((ar_op == NULL) || (ar_op->regiontype != RGN_TYPE_HUD));
		struct HudRegionData *hrd = ar->regiondata;
		if (hrd == NULL) {
			hrd = MEM_callocN(sizeof(*hrd), __func__);
			ar->regiondata = hrd;
		}
		if (ar_op) {
			hrd->regionid = ar_op->regiontype;
		}
		else {
			hrd->regionid = -1;
		}
	}

	if (init) {
		/* This is needed or 'winrct' will be invalid. */
		wmWindow *win = CTX_wm_window(C);
		ED_area_update_region_sizes(wm, win, sa);
	}

	ED_region_init(ar);
	ED_region_tag_redraw(ar);

	/* Reset zoom level (not well supported). */
	ar->v2d.cur = ar->v2d.tot = (rctf){.xmax = ar->winx, .ymax = ar->winy};
	ar->v2d.minzoom = 1.0f;
	ar->v2d.maxzoom = 1.0f;

	/* XXX, should be handled in more general way. */
	ar->visible = !((ar->flag & RGN_FLAG_HIDDEN) || (ar->flag & RGN_FLAG_TOO_SMALL));

	/* We shouldn't need to do this every time :S */
	/* XXX, this is evil! - it also makes the menu show on first draw. :( */
	ARegion *ar_prev = CTX_wm_region(C);
	CTX_wm_region_set((bContext *)C, ar);
	hud_region_layout(C, ar);
	CTX_wm_region_set((bContext *)C, ar_prev);

}

/** \} */
