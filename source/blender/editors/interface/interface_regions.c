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

/** \file blender/editors/interface/interface_regions.c
 *  \ingroup edinterface
 */



#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_idcode.h"
#include "BKE_report.h"
#include "BKE_global.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_draw.h"
#include "wm_subwindow.h"
#include "wm_window.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"

#include "BLF_api.h"
#include "BLF_translation.h"

#include "ED_screen.h"

#include "IMB_colormanagement.h"

#include "interface_intern.h"

#define MENU_TOP            8
#define MENU_PADDING		(int)(0.2f * UI_UNIT_Y)

static int rna_property_enum_step(const bContext *C, PointerRNA *ptr, PropertyRNA *prop, int direction)
{
	EnumPropertyItem *item_array;
	int totitem;
	bool free;
	int value;
	int i, i_init;
	int step = (direction < 0) ? -1 : 1;
	int step_tot = 0;

	RNA_property_enum_items((bContext *)C, ptr, prop, &item_array, &totitem, &free);
	value = RNA_property_enum_get(ptr, prop);
	i = RNA_enum_from_value(item_array, value);
	i_init = i;

	do {
		i = mod_i(i + step, totitem);
		if (item_array[i].identifier[0]) {
			step_tot += step;
		}
	} while ((i != i_init) && (step_tot != direction));

	if (i != i_init) {
		value = item_array[i].value;
	}

	if (free) {
		MEM_freeN(item_array);
	}

	return value;
}

int ui_step_name_menu(uiBut *but, int direction)
{
	/* currenly only RNA buttons */
	if ((but->rnaprop == NULL) || (RNA_property_type(but->rnaprop) != PROP_ENUM)) {
		printf("%s: cannot cycle button '%s'", __func__, but->str);
		return 0;
	}

	return rna_property_enum_step(but->block->evil_C, &but->rnapoin, but->rnaprop, direction);
}

/******************** Creating Temporary regions ******************/

static ARegion *ui_add_temporary_region(bScreen *sc)
{
	ARegion *ar;

	ar = MEM_callocN(sizeof(ARegion), "area region");
	BLI_addtail(&sc->regionbase, ar);

	ar->regiontype = RGN_TYPE_TEMPORARY;
	ar->alignment = RGN_ALIGN_FLOAT;

	return ar;
}

static void ui_remove_temporary_region(bContext *C, bScreen *sc, ARegion *ar)
{
	wmWindow *win = CTX_wm_window(C);
	if (win)
		wm_draw_region_clear(win, ar);

	ED_region_exit(C, ar);
	BKE_area_region_free(NULL, ar);     /* NULL: no spacetype */
	BLI_freelinkN(&sc->regionbase, ar);
}

/************************* Creating Tooltips **********************/

typedef enum {
	UI_TIP_LC_MAIN,
	UI_TIP_LC_NORMAL,
	UI_TIP_LC_PYTHON,
	UI_TIP_LC_ALERT,
	UI_TIP_LC_SUBMENU
} uiTooltipLineColor;
#define UI_TIP_LC_MAX 5

#define MAX_TOOLTIP_LINES 8
typedef struct uiTooltipData {
	rcti bbox;
	uiFontStyle fstyle;
	char lines[MAX_TOOLTIP_LINES][512];
	uiTooltipLineColor color_id[MAX_TOOLTIP_LINES];
	int totline;
	int toth, spaceh, lineh;
} uiTooltipData;

static void rgb_tint(float col[3],
                     float h, float h_strength,
                     float v, float v_strength)
{
	float col_hsv_from[3];
	float col_hsv_to[3];

	rgb_to_hsv_v(col, col_hsv_from);

	col_hsv_to[0] = h;
	col_hsv_to[1] = h_strength;
	col_hsv_to[2] = (col_hsv_from[2] * (1.0f - v_strength)) + (v * v_strength);

	hsv_to_rgb_v(col_hsv_to, col);
}

static void ui_tooltip_region_draw_cb(const bContext *UNUSED(C), ARegion *ar)
{
	uiTooltipData *data = ar->regiondata;
	uiWidgetColors *theme = ui_tooltip_get_theme();
	rcti bbox = data->bbox;
	float tip_colors[UI_TIP_LC_MAX][3];

	float *main_color    = tip_colors[UI_TIP_LC_MAIN]; /* the color from the theme */
	float *normal_color  = tip_colors[UI_TIP_LC_NORMAL];
	float *python_color  = tip_colors[UI_TIP_LC_PYTHON];
	float *alert_color   = tip_colors[UI_TIP_LC_ALERT];
	float *submenu_color = tip_colors[UI_TIP_LC_SUBMENU];

	float background_color[3];
	float tone_bg;
	int i, multisample_enabled;

	/* disable AA, makes widgets too blurry */
	multisample_enabled = glIsEnabled(GL_MULTISAMPLE_ARB);
	if (multisample_enabled)
		glDisable(GL_MULTISAMPLE_ARB);

	/* draw background */
	ui_draw_tooltip_background(UI_GetStyle(), NULL, &bbox);

	/* set background_color */
	rgb_uchar_to_float(background_color, (const unsigned char *)theme->inner);

	/* calculate normal_color */
	rgb_uchar_to_float(main_color, (const unsigned char *)theme->text);
	copy_v3_v3(normal_color, main_color);
	copy_v3_v3(python_color, main_color);
	copy_v3_v3(alert_color, main_color);
	copy_v3_v3(submenu_color, main_color);

	/* find the brightness difference between background and text colors */
	
	tone_bg = rgb_to_grayscale(background_color);
	/* tone_fg = rgb_to_grayscale(main_color); */

	rgb_tint(normal_color, 0.0f, 0.0f, tone_bg, 0.3f);   /* a shade darker (to bg) */
	rgb_tint(python_color, 0.666f, 0.25f, tone_bg, 0.3f); /* blue */
	rgb_tint(alert_color, 0.0f, 0.8f, tone_bg, 0.1f);    /* bright red */
	rgb_tint(submenu_color, 0.0f, 0.0f, tone_bg, 0.3f);  /* a shade darker (to bg) */

	/* draw text */
	uiStyleFontSet(&data->fstyle);

	bbox.ymax = bbox.ymax - 0.5f * (BLI_rcti_size_y(&bbox) - data->toth);
	bbox.ymin = bbox.ymax - data->lineh;

	for (i = 0; i < data->totline; i++) {
		glColor3fv(tip_colors[data->color_id[i]]);
		uiStyleFontDraw(&data->fstyle, &bbox, data->lines[i]);
		bbox.ymin -= data->lineh + data->spaceh;
		bbox.ymax -= data->lineh + data->spaceh;
	}

	if (multisample_enabled)
		glEnable(GL_MULTISAMPLE_ARB);
}

static void ui_tooltip_region_free_cb(ARegion *ar)
{
	uiTooltipData *data;

	data = ar->regiondata;
	MEM_freeN(data);
	ar->regiondata = NULL;
}

ARegion *ui_tooltip_create(bContext *C, ARegion *butregion, uiBut *but)
{
	wmWindow *win = CTX_wm_window(C);
	uiStyle *style = UI_GetStyle();
	static ARegionType type;
	ARegion *ar;
	uiTooltipData *data;
/*	IDProperty *prop;*/
	char buf[512];
	/* aspect values that shrink text are likely unreadable */
	const float aspect = min_ff(1.0f, but->block->aspect);
	float fonth, fontw;
	int winx /*, winy */, ofsx, ofsy, w, h, a;
	rctf rect_fl;
	rcti rect_i;

	uiStringInfo but_tip = {BUT_GET_TIP, NULL};
	uiStringInfo enum_label = {BUT_GET_RNAENUM_LABEL, NULL};
	uiStringInfo enum_tip = {BUT_GET_RNAENUM_TIP, NULL};
	uiStringInfo op_keymap = {BUT_GET_OP_KEYMAP, NULL};
	uiStringInfo prop_keymap = {BUT_GET_PROP_KEYMAP, NULL};
	uiStringInfo rna_struct = {BUT_GET_RNASTRUCT_IDENTIFIER, NULL};
	uiStringInfo rna_prop = {BUT_GET_RNAPROP_IDENTIFIER, NULL};

	if (but->drawflag & UI_BUT_NO_TOOLTIP)
		return NULL;

	/* create tooltip data */
	data = MEM_callocN(sizeof(uiTooltipData), "uiTooltipData");

	uiButGetStrInfo(C, but, &but_tip, &enum_label, &enum_tip, &op_keymap, &prop_keymap, &rna_struct, &rna_prop, NULL);

	/* special case, enum rna buttons only have enum item description,
	 * use general enum description too before the specific one */

	/* Tip */
	if (but_tip.strinfo) {
		/* Expanded Bit-flag enums have a specific way to select multiple... */
		if ((but->type & ROW) && but->rnaprop && RNA_property_flag(but->rnaprop) & PROP_ENUM_FLAG) {
			BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]),
			             "%s %s", but_tip.strinfo, IFACE_("(Shift-click to select multiple)"));
		}
		else {
			BLI_strncpy(data->lines[data->totline], but_tip.strinfo, sizeof(data->lines[0]));
		}
		data->color_id[data->totline] = UI_TIP_LC_MAIN;
		data->totline++;
	}
	/* Enum item label & tip */
	if (enum_label.strinfo && enum_tip.strinfo) {
		BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]),
		             "%s: %s", enum_label.strinfo, enum_tip.strinfo);
		data->color_id[data->totline] = UI_TIP_LC_SUBMENU;
		data->totline++;
	}

	/* Op shortcut */
	if (op_keymap.strinfo) {
		BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Shortcut: %s"), op_keymap.strinfo);
		data->color_id[data->totline] = UI_TIP_LC_NORMAL;
		data->totline++;
	}
	
	/* Property context-toggle shortcut */
	if (prop_keymap.strinfo) {
		BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Shortcut: %s"), prop_keymap.strinfo);
		data->color_id[data->totline] = UI_TIP_LC_NORMAL;
		data->totline++;
	}

	if (ELEM3(but->type, TEX, SEARCH_MENU, SEARCH_MENU_UNLINK)) {
		/* better not show the value of a password */
		if ((but->rnaprop && (RNA_property_subtype(but->rnaprop) == PROP_PASSWORD)) == 0) {
			/* full string */
			ui_get_but_string(but, buf, sizeof(buf));
			if (buf[0]) {
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Value: %s"), buf);
				data->color_id[data->totline] = UI_TIP_LC_NORMAL;
				data->totline++;
			}
		}
	}

	if (but->rnaprop) {
		int unit_type = uiButGetUnitType(but);
		
		if (unit_type == PROP_UNIT_ROTATION) {
			if (RNA_property_type(but->rnaprop) == PROP_FLOAT) {
				float value = RNA_property_array_check(but->rnaprop) ?
				                  RNA_property_float_get_index(&but->rnapoin, but->rnaprop, but->rnaindex) :
				                  RNA_property_float_get(&but->rnapoin, but->rnaprop);
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Radians: %f"), value);
				data->color_id[data->totline] = UI_TIP_LC_NORMAL;
				data->totline++;
			}
		}
		
		if (but->flag & UI_BUT_DRIVEN) {
			if (ui_but_anim_expression_get(but, buf, sizeof(buf))) {
				/* expression */
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Expression: %s"), buf);
				data->color_id[data->totline] = UI_TIP_LC_NORMAL;
				data->totline++;
			}
		}

		if (but->rnapoin.id.data) {
			ID *id = but->rnapoin.id.data;
			if (id->lib) {
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Library: %s"), id->lib->name);
				data->color_id[data->totline] = UI_TIP_LC_NORMAL;
				data->totline++;
			}
		}
	}
	else if (but->optype) {
		PointerRNA *opptr;
		char *str;
		opptr = uiButGetOperatorPtrRNA(but); /* allocated when needed, the button owns it */

		/* so the context is passed to itemf functions (some py itemf functions use it) */
		WM_operator_properties_sanitize(opptr, false);

		str = WM_operator_pystring_ex(C, NULL, false, false, but->optype, opptr);

		/* avoid overly verbose tips (eg, arrays of 20 layers), exact limit is arbitrary */
		WM_operator_pystring_abbreviate(str, 32);

		/* operator info */
		if ((U.flag & USER_TOOLTIPS_PYTHON) == 0) {
			BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Python: %s"), str);
			data->color_id[data->totline] = UI_TIP_LC_PYTHON;
			data->totline++;
		}

		MEM_freeN(str);

		/* second check if we are disabled - why */
		if (but->flag & UI_BUT_DISABLED) {
			const char *poll_msg;
			CTX_wm_operator_poll_msg_set(C, NULL);
			WM_operator_poll_context(C, but->optype, but->opcontext);
			poll_msg = CTX_wm_operator_poll_msg_get(C);
			if (poll_msg) {
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Disabled: %s"), poll_msg);
				data->color_id[data->totline] = UI_TIP_LC_ALERT; /* alert */
				data->totline++;
			}
		}
	}
	if ((U.flag & USER_TOOLTIPS_PYTHON) == 0 && !but->optype && rna_struct.strinfo) {
		if (rna_prop.strinfo) {
			/* Struct and prop */
			BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]),
			             TIP_("Python: %s.%s"),
			             rna_struct.strinfo, rna_prop.strinfo);
		}
		else {
			/* Only struct (e.g. menus) */
			BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]),
			             TIP_("Python: %s"), rna_struct.strinfo);
		}
		data->color_id[data->totline] = UI_TIP_LC_PYTHON;
		data->totline++;

		if (but->rnapoin.id.data) {
			/* this could get its own 'BUT_GET_...' type */
			PointerRNA *ptr = &but->rnapoin;
			PropertyRNA *prop = but->rnaprop;
			ID *id = ptr->id.data;

			char *id_path;
			char *data_path = NULL;

			/* never fails */
			id_path = RNA_path_full_ID_py(id);

			if (ptr->data && prop) {
				data_path = RNA_path_from_ID_to_property(ptr, prop);
			}

			if (data_path) {
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]),
				             "%s.%s",  /* no need to translate */
				             id_path, data_path);
				MEM_freeN(data_path);
			}
			else if (prop) {
				/* can't find the path. be explicit in our ignorance "..." */
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]),
				             "%s ... %s",  /* no need to translate */
				             id_path, rna_prop.strinfo ? rna_prop.strinfo : RNA_property_identifier(prop));
			}
			MEM_freeN(id_path);

			data->color_id[data->totline] = UI_TIP_LC_PYTHON;
			data->totline++;
		}
	}

	/* Free strinfo's... */
	if (but_tip.strinfo)
		MEM_freeN(but_tip.strinfo);
	if (enum_label.strinfo)
		MEM_freeN(enum_label.strinfo);
	if (enum_tip.strinfo)
		MEM_freeN(enum_tip.strinfo);
	if (op_keymap.strinfo)
		MEM_freeN(op_keymap.strinfo);
	if (prop_keymap.strinfo)
		MEM_freeN(prop_keymap.strinfo);
	if (rna_struct.strinfo)
		MEM_freeN(rna_struct.strinfo);
	if (rna_prop.strinfo)
		MEM_freeN(rna_prop.strinfo);

	BLI_assert(data->totline < MAX_TOOLTIP_LINES);
	
	if (data->totline == 0) {
		MEM_freeN(data);
		return NULL;
	}

	/* create area region */
	ar = ui_add_temporary_region(CTX_wm_screen(C));

	memset(&type, 0, sizeof(ARegionType));
	type.draw = ui_tooltip_region_draw_cb;
	type.free = ui_tooltip_region_free_cb;
	type.regionid = RGN_TYPE_TEMPORARY;
	ar->type = &type;
	
	/* set font, get bb */
	data->fstyle = style->widget; /* copy struct */
	data->fstyle.align = UI_STYLE_TEXT_CENTER;
	ui_fontscale(&data->fstyle.points, aspect);

	uiStyleFontSet(&data->fstyle);

	/* these defines tweaked depending on font */
#define TIP_MARGIN_Y (2.0f / aspect)
#define TIP_BORDER_X (16.0f / aspect)
#define TIP_BORDER_Y (6.0f / aspect)

	h = BLF_height_max(data->fstyle.uifont_id);

	for (a = 0, fontw = 0, fonth = 0; a < data->totline; a++) {
		w = BLF_width(data->fstyle.uifont_id, data->lines[a], sizeof(data->lines[a]));
		fontw = max_ff(fontw, (float)w);
		fonth += (a == 0) ? h : h + TIP_MARGIN_Y;
	}

	//fontw *= aspect;

	ar->regiondata = data;

	data->toth = fonth;
	data->lineh = h;
	data->spaceh = TIP_MARGIN_Y;

	/* compute position */
	ofsx = 0; //(but->block->panel) ? but->block->panel->ofsx : 0;
	ofsy = 0; //(but->block->panel) ? but->block->panel->ofsy : 0;

	rect_fl.xmin = BLI_rctf_cent_x(&but->rect) + ofsx - TIP_BORDER_X;
	rect_fl.xmax = rect_fl.xmin + fontw + (TIP_BORDER_X * 2);
	rect_fl.ymax = but->rect.ymin + ofsy - TIP_BORDER_Y;
	rect_fl.ymin = rect_fl.ymax - fonth  - TIP_BORDER_Y;

#undef TIP_MARGIN_Y
#undef TIP_BORDER_X
#undef TIP_BORDER_Y
	
	/* since the text has beens caled already, the size of tooltips is defined now */
	/* here we try to figure out the right location */
	if (butregion) {
		float ofsx_fl = rect_fl.xmin, ofsy_fl = rect_fl.ymax;
		ui_block_to_window_fl(butregion, but->block, &ofsx_fl, &ofsy_fl);
		BLI_rctf_translate(&rect_fl, ofsx_fl - rect_fl.xmin, ofsy_fl - rect_fl.ymax);
	}
	BLI_rcti_rctf_copy(&rect_i, &rect_fl);

	/* clip with window boundaries */
	winx = WM_window_pixels_x(win);

	if (rect_i.xmax > winx) {
		/* super size */
		if (rect_i.xmax > winx + rect_i.xmin) {
			rect_i.xmax = winx;
			rect_i.xmin = 0;
		}
		else {
			rect_i.xmin -= rect_i.xmax - winx;
			rect_i.xmax = winx;
		}
	}
	/* ensure at least 5 px above screen bounds
	 * 25 is just a guess to be above the menu item */
	if (rect_i.ymin < 5) {
		rect_i.ymax += (-rect_i.ymin) + 30;
		rect_i.ymin = 30;
	}

	/* widget rect, in region coords */
	{
		int width = UI_ThemeMenuShadowWidth();
		
		data->bbox.xmin = width;
		data->bbox.xmax = BLI_rcti_size_x(&rect_i) + width;
		data->bbox.ymin = width;
		data->bbox.ymax = BLI_rcti_size_y(&rect_i) + width;
		
		/* region bigger for shadow */
		ar->winrct.xmin = rect_i.xmin - width;
		ar->winrct.xmax = rect_i.xmax + width;
		ar->winrct.ymin = rect_i.ymin - width;
		ar->winrct.ymax = rect_i.ymax + MENU_TOP;
	}

	/* adds subwindow */
	ED_region_init(C, ar);
	
	/* notify change and redraw */
	ED_region_tag_redraw(ar);

	return ar;
}

void ui_tooltip_free(bContext *C, ARegion *ar)
{
	ui_remove_temporary_region(C, CTX_wm_screen(C), ar);
}


/************************* Creating Search Box **********************/

struct uiSearchItems {
	int maxitem, totitem, maxstrlen;
	
	int offset, offset_i; /* offset for inserting in array */
	int more;  /* flag indicating there are more items */
	
	char **names;
	void **pointers;
	int *icons;

	AutoComplete *autocpl;
	void *active;
};

typedef struct uiSearchboxData {
	rcti bbox;
	uiFontStyle fstyle;
	uiSearchItems items;
	int active;     /* index in items array */
	bool noback;    /* when menu opened with enough space for this */
	bool preview;   /* draw thumbnail previews, rather than list */
	bool use_sep;   /* use the UI_SEP_CHAR char for splitting shortcuts (good for operators, bad for data) */
	int prv_rows, prv_cols;
} uiSearchboxData;

#define SEARCH_ITEMS    10

/* exported for use by search callbacks */
/* returns zero if nothing to add */
bool uiSearchItemAdd(uiSearchItems *items, const char *name, void *poin, int iconid)
{
	/* hijack for autocomplete */
	if (items->autocpl) {
		autocomplete_do_name(items->autocpl, name);
		return true;
	}
	
	/* hijack for finding active item */
	if (items->active) {
		if (poin == items->active)
			items->offset_i = items->totitem;
		items->totitem++;
		return true;
	}
	
	if (items->totitem >= items->maxitem) {
		items->more = 1;
		return false;
	}
	
	/* skip first items in list */
	if (items->offset_i > 0) {
		items->offset_i--;
		return true;
	}
	
	if (items->names)
		BLI_strncpy(items->names[items->totitem], name, items->maxstrlen);
	if (items->pointers)
		items->pointers[items->totitem] = poin;
	if (items->icons)
		items->icons[items->totitem] = iconid;
	
	items->totitem++;
	
	return true;
}

int uiSearchBoxHeight(void)
{
	return SEARCH_ITEMS * UI_UNIT_Y + 2 * MENU_TOP;
}

int uiSearchBoxWidth(void)
{
	return 12 * UI_UNIT_X;
}

int uiSearchItemFindIndex(uiSearchItems *items, const char *name)
{
	int i;
	for (i = 0; i < items->totitem; i++) {
		if (STREQ(name, items->names[i])) {
			return i;
		}
	}
	return -1;
}

/* ar is the search box itself */
static void ui_searchbox_select(bContext *C, ARegion *ar, uiBut *but, int step)
{
	uiSearchboxData *data = ar->regiondata;
	
	/* apply step */
	data->active += step;
	
	if (data->items.totitem == 0) {
		data->active = -1;
	}
	else if (data->active >= data->items.totitem) {
		if (data->items.more) {
			data->items.offset++;
			data->active = data->items.totitem - 1;
			ui_searchbox_update(C, ar, but, false);
		}
		else {
			data->active = data->items.totitem - 1;
		}
	}
	else if (data->active < 0) {
		if (data->items.offset) {
			data->items.offset--;
			data->active = 0;
			ui_searchbox_update(C, ar, but, false);
		}
		else if (data->active < -1)
			data->active = -1;
	}
	
	ED_region_tag_redraw(ar);
}

static void ui_searchbox_butrect(rcti *r_rect, uiSearchboxData *data, int itemnr)
{
	/* thumbnail preview */
	if (data->preview) {
		int butw =  BLI_rcti_size_x(&data->bbox)                 / data->prv_cols;
		int buth = (BLI_rcti_size_y(&data->bbox) - 2 * MENU_TOP) / data->prv_rows;
		int row, col;
		
		*r_rect = data->bbox;
		
		col = itemnr % data->prv_cols;
		row = itemnr / data->prv_cols;
		
		r_rect->xmin += col * butw;
		r_rect->xmax = r_rect->xmin + butw;
		
		r_rect->ymax = data->bbox.ymax - MENU_TOP - (row * buth);
		r_rect->ymin = r_rect->ymax - buth;
	}
	/* list view */
	else {
		int buth = (BLI_rcti_size_y(&data->bbox) - 2 * MENU_TOP) / SEARCH_ITEMS;
		
		*r_rect = data->bbox;
		r_rect->xmin = data->bbox.xmin + 3.0f;
		r_rect->xmax = data->bbox.xmax - 3.0f;
		
		r_rect->ymax = data->bbox.ymax - MENU_TOP - itemnr * buth;
		r_rect->ymin = r_rect->ymax - buth;
	}
	
}

int ui_searchbox_find_index(ARegion *ar, const char *name)
{
	uiSearchboxData *data = ar->regiondata;
	return uiSearchItemFindIndex(&data->items, name);
}

/* x and y in screencoords */
bool ui_searchbox_inside(ARegion *ar, int x, int y)
{
	uiSearchboxData *data = ar->regiondata;
	
	return BLI_rcti_isect_pt(&data->bbox, x - ar->winrct.xmin, y - ar->winrct.ymin);
}

/* string validated to be of correct length (but->hardmax) */
bool ui_searchbox_apply(uiBut *but, ARegion *ar)
{
	uiSearchboxData *data = ar->regiondata;

	but->func_arg2 = NULL;
	
	if (data->active != -1) {
		const char *name = data->items.names[data->active];
		const char *name_sep = data->use_sep ? strchr(name, UI_SEP_CHAR) : NULL;

		BLI_strncpy(but->editstr, name, name_sep ? (name_sep - name) : data->items.maxstrlen);
		
		but->func_arg2 = data->items.pointers[data->active];

		return true;
	}
	else {
		return false;
	}
}

void ui_searchbox_event(bContext *C, ARegion *ar, uiBut *but, const wmEvent *event)
{
	uiSearchboxData *data = ar->regiondata;
	int type = event->type, val = event->val;
	
	if (type == MOUSEPAN)
		ui_pan_to_scroll(event, &type, &val);
	
	switch (type) {
		case WHEELUPMOUSE:
		case UPARROWKEY:
			ui_searchbox_select(C, ar, but, -1);
			break;
		case WHEELDOWNMOUSE:
		case DOWNARROWKEY:
			ui_searchbox_select(C, ar, but, 1);
			break;
		case MOUSEMOVE:
			if (BLI_rcti_isect_pt(&ar->winrct, event->x, event->y)) {
				rcti rect;
				int a;
				
				for (a = 0; a < data->items.totitem; a++) {
					ui_searchbox_butrect(&rect, data, a);
					if (BLI_rcti_isect_pt(&rect, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin)) {
						if (data->active != a) {
							data->active = a;
							ui_searchbox_select(C, ar, but, 0);
							break;
						}
					}
				}
			}
			break;
	}
}

/* ar is the search box itself */
void ui_searchbox_update(bContext *C, ARegion *ar, uiBut *but, const bool reset)
{
	uiSearchboxData *data = ar->regiondata;
	
	/* reset vars */
	data->items.totitem = 0;
	data->items.more = 0;
	if (reset == false) {
		data->items.offset_i = data->items.offset;
	}
	else {
		data->items.offset_i = data->items.offset = 0;
		data->active = -1;
		
		/* handle active */
		if (but->search_func && but->func_arg2) {
			data->items.active = but->func_arg2;
			but->search_func(C, but->search_arg, but->editstr, &data->items);
			data->items.active = NULL;
			
			/* found active item, calculate real offset by centering it */
			if (data->items.totitem) {
				/* first case, begin of list */
				if (data->items.offset_i < data->items.maxitem) {
					data->active = data->items.offset_i;
					data->items.offset_i = 0;
				}
				else {
					/* second case, end of list */
					if (data->items.totitem - data->items.offset_i <= data->items.maxitem) {
						data->active = data->items.offset_i - data->items.totitem + data->items.maxitem;
						data->items.offset_i = data->items.totitem - data->items.maxitem;
					}
					else {
						/* center active item */
						data->items.offset_i -= data->items.maxitem / 2;
						data->active = data->items.maxitem / 2;
					}
				}
			}
			data->items.offset = data->items.offset_i;
			data->items.totitem = 0;
		}
	}
	
	/* callback */
	if (but->search_func)
		but->search_func(C, but->search_arg, but->editstr, &data->items);
	
	/* handle case where editstr is equal to one of items */
	if (reset && data->active == -1) {
		int a;
		
		for (a = 0; a < data->items.totitem; a++) {
			const char *name = data->items.names[a];
			const char *name_sep = data->use_sep ? strchr(name, UI_SEP_CHAR) : NULL;
			if (STREQLEN(but->editstr, name, name_sep ? (name_sep - name) : data->items.maxstrlen)) {
				data->active = a;
				break;
			}
		}
		if (data->items.totitem == 1 && but->editstr[0])
			data->active = 0;
	}

	/* validate selected item */
	ui_searchbox_select(C, ar, but, 0);
	
	ED_region_tag_redraw(ar);
}

int ui_searchbox_autocomplete(bContext *C, ARegion *ar, uiBut *but, char *str)
{
	uiSearchboxData *data = ar->regiondata;
	int match = AUTOCOMPLETE_NO_MATCH;

	if (str[0]) {
		data->items.autocpl = autocomplete_begin(str, ui_get_but_string_max_length(but));

		but->search_func(C, but->search_arg, but->editstr, &data->items);

		match = autocomplete_end(data->items.autocpl, str);
		data->items.autocpl = NULL;
	}

	return match;
}

static void ui_searchbox_region_draw_cb(const bContext *UNUSED(C), ARegion *ar)
{
	uiSearchboxData *data = ar->regiondata;
	
	/* pixel space */
	wmOrtho2(-0.01f, ar->winx - 0.01f, -0.01f, ar->winy - 0.01f);

	if (data->noback == false)
		ui_draw_search_back(NULL, NULL, &data->bbox);  /* style not used yet */
	
	/* draw text */
	if (data->items.totitem) {
		rcti rect;
		int a;
		
		if (data->preview) {
			/* draw items */
			for (a = 0; a < data->items.totitem; a++) {
				ui_searchbox_butrect(&rect, data, a);
				
				/* widget itself */
				if (data->preview) {
					ui_draw_preview_item(&data->fstyle, &rect, data->items.names[a], data->items.icons[a],
					                     (a == data->active) ? UI_ACTIVE : 0);
				}
				else {
					ui_draw_menu_item(&data->fstyle, &rect, data->items.names[a], data->items.icons[a],
					                  (a == data->active) ? UI_ACTIVE : 0, data->use_sep);
				}
			}
			
			/* indicate more */
			if (data->items.more) {
				ui_searchbox_butrect(&rect, data, data->items.maxitem - 1);
				glEnable(GL_BLEND);
				UI_icon_draw(rect.xmax - 18, rect.ymin - 7, ICON_TRIA_DOWN);
				glDisable(GL_BLEND);
			}
			if (data->items.offset) {
				ui_searchbox_butrect(&rect, data, 0);
				glEnable(GL_BLEND);
				UI_icon_draw(rect.xmin, rect.ymax - 9, ICON_TRIA_UP);
				glDisable(GL_BLEND);
			}
			
		}
		else {
			/* draw items */
			for (a = 0; a < data->items.totitem; a++) {
				ui_searchbox_butrect(&rect, data, a);
				
				/* widget itself */
				ui_draw_menu_item(&data->fstyle, &rect, data->items.names[a], data->items.icons[a],
				                  (a == data->active) ? UI_ACTIVE : 0, data->use_sep);
				
			}
			/* indicate more */
			if (data->items.more) {
				ui_searchbox_butrect(&rect, data, data->items.maxitem - 1);
				glEnable(GL_BLEND);
				UI_icon_draw((BLI_rcti_size_x(&rect)) / 2, rect.ymin - 9, ICON_TRIA_DOWN);
				glDisable(GL_BLEND);
			}
			if (data->items.offset) {
				ui_searchbox_butrect(&rect, data, 0);
				glEnable(GL_BLEND);
				UI_icon_draw((BLI_rcti_size_x(&rect)) / 2, rect.ymax - 7, ICON_TRIA_UP);
				glDisable(GL_BLEND);
			}
		}
	}
}

static void ui_searchbox_region_free_cb(ARegion *ar)
{
	uiSearchboxData *data = ar->regiondata;
	int a;

	/* free search data */
	for (a = 0; a < data->items.maxitem; a++) {
		MEM_freeN(data->items.names[a]);
	}
	MEM_freeN(data->items.names);
	MEM_freeN(data->items.pointers);
	MEM_freeN(data->items.icons);
	
	MEM_freeN(data);
	ar->regiondata = NULL;
}

ARegion *ui_searchbox_create(bContext *C, ARegion *butregion, uiBut *but)
{
	wmWindow *win = CTX_wm_window(C);
	uiStyle *style = UI_GetStyle();
	static ARegionType type;
	ARegion *ar;
	uiSearchboxData *data;
	float aspect = but->block->aspect;
	rctf rect_fl;
	rcti rect_i;
	int winx /*, winy */, ofsx, ofsy;
	int i;
	
	/* create area region */
	ar = ui_add_temporary_region(CTX_wm_screen(C));
	
	memset(&type, 0, sizeof(ARegionType));
	type.draw = ui_searchbox_region_draw_cb;
	type.free = ui_searchbox_region_free_cb;
	type.regionid = RGN_TYPE_TEMPORARY;
	ar->type = &type;
	
	/* create searchbox data */
	data = MEM_callocN(sizeof(uiSearchboxData), "uiSearchboxData");

	/* set font, get bb */
	data->fstyle = style->widget; /* copy struct */
	data->fstyle.align = UI_STYLE_TEXT_CENTER;
	ui_fontscale(&data->fstyle.points, aspect);
	uiStyleFontSet(&data->fstyle);
	
	ar->regiondata = data;
	
	/* special case, hardcoded feature, not draw backdrop when called from menus,
	 * assume for design that popup already added it */
	if (but->block->flag & UI_BLOCK_SEARCH_MENU)
		data->noback = true;
	
	if (but->a1 > 0 && but->a2 > 0) {
		data->preview = true;
		data->prv_rows = but->a1;
		data->prv_cols = but->a2;
	}

	/* only show key shortcuts when needed (not rna buttons) [#36699] */
	if (but->rnaprop == NULL) {
		data->use_sep = true;
	}
	
	/* compute position */
	if (but->block->flag & UI_BLOCK_SEARCH_MENU) {
		int width = UI_ThemeMenuShadowWidth();
		/* this case is search menu inside other menu */
		/* we copy region size */

		ar->winrct = butregion->winrct;
		
		/* widget rect, in region coords */
		data->bbox.xmin = width;
		data->bbox.xmax = BLI_rcti_size_x(&ar->winrct) - width;
		data->bbox.ymin = width;
		data->bbox.ymax = BLI_rcti_size_y(&ar->winrct) - width;
		
		/* check if button is lower half */
		if (but->rect.ymax < BLI_rctf_cent_y(&but->block->rect)) {
			data->bbox.ymin += BLI_rctf_size_y(&but->rect);
		}
		else {
			data->bbox.ymax -= BLI_rctf_size_y(&but->rect);
		}
	}
	else {
		const int searchbox_width = uiSearchBoxWidth();
		const int shadow_width = UI_ThemeMenuShadowWidth();

		rect_fl.xmin = but->rect.xmin - 5;   /* align text with button */
		rect_fl.xmax = but->rect.xmax + 5;   /* symmetrical */
		rect_fl.ymax = but->rect.ymin;
		rect_fl.ymin = rect_fl.ymax - uiSearchBoxHeight();

		ofsx = (but->block->panel) ? but->block->panel->ofsx : 0;
		ofsy = (but->block->panel) ? but->block->panel->ofsy : 0;

		BLI_rctf_translate(&rect_fl, ofsx, ofsy);
	
		/* minimal width */
		if (BLI_rctf_size_x(&rect_fl) < searchbox_width) {
			rect_fl.xmax = rect_fl.xmin + searchbox_width;
		}
		
		/* copy to int, gets projected if possible too */
		BLI_rcti_rctf_copy(&rect_i, &rect_fl);
		
		if (butregion->v2d.cur.xmin != butregion->v2d.cur.xmax) {
			UI_view2d_to_region_no_clip(&butregion->v2d, rect_fl.xmin, rect_fl.ymin, &rect_i.xmin, &rect_i.ymin);
			UI_view2d_to_region_no_clip(&butregion->v2d, rect_fl.xmax, rect_fl.ymax, &rect_i.xmax, &rect_i.ymax);
		}

		BLI_rcti_translate(&rect_i, butregion->winrct.xmin, butregion->winrct.ymin);

		winx = WM_window_pixels_x(win);
		// winy = WM_window_pixels_y(win);  /* UNUSED */
		//wm_window_get_size(win, &winx, &winy);
		
		if (rect_i.xmax > winx) {
			/* super size */
			if (rect_i.xmax > winx + rect_i.xmin) {
				rect_i.xmax = winx;
				rect_i.xmin = 0;
			}
			else {
				rect_i.xmin -= rect_i.xmax - winx;
				rect_i.xmax = winx;
			}
		}

		if (rect_i.ymin < 0) {
			int newy1 = but->rect.ymax + ofsy;

			if (butregion->v2d.cur.xmin != butregion->v2d.cur.xmax)
				UI_view2d_to_region_no_clip(&butregion->v2d, 0, newy1, NULL, &newy1);

			newy1 += butregion->winrct.ymin;

			rect_i.ymax = BLI_rcti_size_y(&rect_i) + newy1;
			rect_i.ymin = newy1;
		}

		/* widget rect, in region coords */
		data->bbox.xmin = shadow_width;
		data->bbox.xmax = BLI_rcti_size_x(&rect_i) + shadow_width;
		data->bbox.ymin = shadow_width;
		data->bbox.ymax = BLI_rcti_size_y(&rect_i) + shadow_width;
		
		/* region bigger for shadow */
		ar->winrct.xmin = rect_i.xmin - shadow_width;
		ar->winrct.xmax = rect_i.xmax + shadow_width;
		ar->winrct.ymin = rect_i.ymin - shadow_width;
		ar->winrct.ymax = rect_i.ymax;
	}
	
	/* adds subwindow */
	ED_region_init(C, ar);
	
	/* notify change and redraw */
	ED_region_tag_redraw(ar);
	
	/* prepare search data */
	if (data->preview) {
		data->items.maxitem = data->prv_rows * data->prv_cols;
	}
	else {
		data->items.maxitem = SEARCH_ITEMS;
	}
	data->items.maxstrlen = but->hardmax;
	data->items.totitem = 0;
	data->items.names = MEM_callocN(data->items.maxitem * sizeof(void *), "search names");
	data->items.pointers = MEM_callocN(data->items.maxitem * sizeof(void *), "search pointers");
	data->items.icons = MEM_callocN(data->items.maxitem * sizeof(int), "search icons");
	for (i = 0; i < data->items.maxitem; i++)
		data->items.names[i] = MEM_callocN(but->hardmax + 1, "search pointers");
	
	return ar;
}

void ui_searchbox_free(bContext *C, ARegion *ar)
{
	ui_remove_temporary_region(C, CTX_wm_screen(C), ar);
}

/* sets red alert if button holds a string it can't find */
/* XXX weak: search_func adds all partial matches... */
void ui_but_search_test(uiBut *but)
{
	uiSearchItems *items;
	int x1;

	/* possibly very large lists (such as ID datablocks) only
	 * only validate string RNA buts (not pointers) */
	if (but->rnaprop && RNA_property_type(but->rnaprop) != PROP_STRING) {
		return;
	}

	items = MEM_callocN(sizeof(uiSearchItems), "search items");

	/* setup search struct */
	items->maxitem = 10;
	items->maxstrlen = 256;
	items->names = MEM_callocN(items->maxitem * sizeof(void *), "search names");
	for (x1 = 0; x1 < items->maxitem; x1++)
		items->names[x1] = MEM_callocN(but->hardmax + 1, "search names");
	
	but->search_func(but->block->evil_C, but->search_arg, but->drawstr, items);
	
	/* only redalert when we are sure of it, this can miss cases when >10 matches */
	if (items->totitem == 0) {
		uiButSetFlag(but, UI_BUT_REDALERT);
	}
	else if (items->more == 0) {
		if (uiSearchItemFindIndex(items, but->drawstr) == -1) {
			uiButSetFlag(but, UI_BUT_REDALERT);
		}
	}
	
	for (x1 = 0; x1 < items->maxitem; x1++) {
		MEM_freeN(items->names[x1]);
	}
	MEM_freeN(items->names);
	MEM_freeN(items);
}


/************************* Creating Menu Blocks **********************/

/* position block relative to but, result is in window space */
static void ui_block_position(wmWindow *window, ARegion *butregion, uiBut *but, uiBlock *block)
{
	uiBut *bt;
	uiSafetyRct *saferct;
	rctf butrct;
	/*float aspect;*/ /*UNUSED*/
	int xsize, ysize, xof = 0, yof = 0, center;
	short dir1 = 0, dir2 = 0;
	
	/* transform to window coordinates, using the source button region/block */
	butrct = but->rect;

	ui_block_to_window_fl(butregion, but->block, &butrct.xmin, &butrct.ymin);
	ui_block_to_window_fl(butregion, but->block, &butrct.xmax, &butrct.ymax);

	/* widget_roundbox_set has this correction too, keep in sync */
	if (but->type != PULLDOWN) {
		if (but->drawflag & UI_BUT_ALIGN_TOP)
			butrct.ymax += U.pixelsize;
		if (but->drawflag & UI_BUT_ALIGN_LEFT)
			butrct.xmin -= U.pixelsize;
	}
	
	/* calc block rect */
	if (block->rect.xmin == 0.0f && block->rect.xmax == 0.0f) {
		if (block->buttons.first) {
			BLI_rctf_init_minmax(&block->rect);

			for (bt = block->buttons.first; bt; bt = bt->next) {
				BLI_rctf_union(&block->rect, &bt->rect);
			}
		}
		else {
			/* we're nice and allow empty blocks too */
			block->rect.xmin = block->rect.ymin = 0;
			block->rect.xmax = block->rect.ymax = 20;
		}
	}
		
	/* aspect = (float)(BLI_rcti_size_x(&block->rect) + 4);*/ /*UNUSED*/
	ui_block_to_window_fl(butregion, but->block, &block->rect.xmin, &block->rect.ymin);
	ui_block_to_window_fl(butregion, but->block, &block->rect.xmax, &block->rect.ymax);

	//block->rect.xmin -= 2.0; block->rect.ymin -= 2.0;
	//block->rect.xmax += 2.0; block->rect.ymax += 2.0;
	
	xsize = BLI_rctf_size_x(&block->rect) + 0.2f * UI_UNIT_X;  /* 4 for shadow */
	ysize = BLI_rctf_size_y(&block->rect) + 0.2f * UI_UNIT_Y;
	/* aspect /= (float)xsize;*/ /*UNUSED*/

	{
		int left = 0, right = 0, top = 0, down = 0;
		int winx, winy;
		// int offscreen;

		winx = WM_window_pixels_x(window);
		winy = WM_window_pixels_y(window);
		// wm_window_get_size(window, &winx, &winy);

		if (block->direction & UI_CENTER) center = ysize / 2;
		else center = 0;
		
		/* check if there's space at all */
		if (butrct.xmin - xsize > 0.0f) left = 1;
		if (butrct.xmax + xsize < winx) right = 1;
		if (butrct.ymin - ysize + center > 0.0f) down = 1;
		if (butrct.ymax + ysize - center < winy) top = 1;

		if (top == 0 && down == 0) {
			if (butrct.ymin - ysize < winy - butrct.ymax - ysize)
				top = 1;
			else
				down = 1;
		}
		
		dir1 = block->direction & UI_DIRECTION;

		/* secundary directions */
		if (dir1 & (UI_TOP | UI_DOWN)) {
			if (dir1 & UI_LEFT) dir2 = UI_LEFT;
			else if (dir1 & UI_RIGHT) dir2 = UI_RIGHT;
			dir1 &= (UI_TOP | UI_DOWN);
		}

		if ((dir2 == 0) && (dir1 == UI_LEFT || dir1 == UI_RIGHT)) dir2 = UI_DOWN;
		if ((dir2 == 0) && (dir1 == UI_TOP  || dir1 == UI_DOWN))  dir2 = UI_LEFT;
		
		/* no space at all? don't change */
		if (left || right) {
			if (dir1 == UI_LEFT && left == 0) dir1 = UI_RIGHT;
			if (dir1 == UI_RIGHT && right == 0) dir1 = UI_LEFT;
			/* this is aligning, not append! */
			if (dir2 == UI_LEFT && right == 0) dir2 = UI_RIGHT;
			if (dir2 == UI_RIGHT && left == 0) dir2 = UI_LEFT;
		}
		if (down || top) {
			if (dir1 == UI_TOP && top == 0) dir1 = UI_DOWN;
			if (dir1 == UI_DOWN && down == 0) dir1 = UI_TOP;
			if (dir2 == UI_TOP && top == 0) dir2 = UI_DOWN;
			if (dir2 == UI_DOWN && down == 0) dir2 = UI_TOP;
		}

		if (dir1 == UI_LEFT) {
			xof = butrct.xmin - block->rect.xmax;
			if (dir2 == UI_TOP) yof = butrct.ymin - block->rect.ymin - center - MENU_PADDING;
			else yof = butrct.ymax - block->rect.ymax + center + MENU_PADDING;
		}
		else if (dir1 == UI_RIGHT) {
			xof = butrct.xmax - block->rect.xmin;
			if (dir2 == UI_TOP) yof = butrct.ymin - block->rect.ymin - center - MENU_PADDING;
			else yof = butrct.ymax - block->rect.ymax + center + MENU_PADDING;
		}
		else if (dir1 == UI_TOP) {
			yof = butrct.ymax - block->rect.ymin;
			if (dir2 == UI_RIGHT) xof = butrct.xmax - block->rect.xmax;
			else xof = butrct.xmin - block->rect.xmin;
			/* changed direction? */
			if ((dir1 & block->direction) == 0) {
				if (block->direction & UI_SHIFT_FLIPPED)
					xof += dir2 == UI_LEFT ? 25 : -25;
				uiBlockFlipOrder(block);
			}
		}
		else if (dir1 == UI_DOWN) {
			yof = butrct.ymin - block->rect.ymax;
			if (dir2 == UI_RIGHT) xof = butrct.xmax - block->rect.xmax;
			else xof = butrct.xmin - block->rect.xmin;
			/* changed direction? */
			if ((dir1 & block->direction) == 0) {
				if (block->direction & UI_SHIFT_FLIPPED)
					xof += dir2 == UI_LEFT ? 25 : -25;
				uiBlockFlipOrder(block);
			}
		}

		/* and now we handle the exception; no space below or to top */
		if (top == 0 && down == 0) {
			if (dir1 == UI_LEFT || dir1 == UI_RIGHT) {
				/* align with bottom of screen */
				// yof = ysize; (not with menu scrolls)
			}
		}
		
		/* or no space left or right */
		if (left == 0 && right == 0) {
			if (dir1 == UI_TOP || dir1 == UI_DOWN) {
				/* align with left size of screen */
				xof = -block->rect.xmin + 5;
			}
		}
		
#if 0
		/* clamp to window bounds, could be made into an option if its ever annoying */
		if (     (offscreen = (block->rect.ymin + yof)) < 0) yof -= offscreen;   /* bottom */
		else if ((offscreen = (block->rect.ymax + yof) - winy) > 0) yof -= offscreen;  /* top */
		if (     (offscreen = (block->rect.xmin + xof)) < 0) xof -= offscreen;   /* left */
		else if ((offscreen = (block->rect.xmax + xof) - winx) > 0) xof -= offscreen;  /* right */
#endif
	}
	
	/* apply offset, buttons in window coords */
	
	for (bt = block->buttons.first; bt; bt = bt->next) {
		ui_block_to_window_fl(butregion, but->block, &bt->rect.xmin, &bt->rect.ymin);
		ui_block_to_window_fl(butregion, but->block, &bt->rect.xmax, &bt->rect.ymax);

		BLI_rctf_translate(&bt->rect, xof, yof);

		/* ui_check_but recalculates drawstring size in pixels */
		ui_check_but(bt);
	}
	
	BLI_rctf_translate(&block->rect, xof, yof);

	/* safety calculus */
	{
		const float midx = BLI_rctf_cent_x(&butrct);
		const float midy = BLI_rctf_cent_y(&butrct);
		
		/* when you are outside parent button, safety there should be smaller */
		
		/* parent button to left */
		if (midx < block->rect.xmin) block->safety.xmin = block->rect.xmin - 3;
		else block->safety.xmin = block->rect.xmin - 40;
		/* parent button to right */
		if (midx > block->rect.xmax) block->safety.xmax = block->rect.xmax + 3;
		else block->safety.xmax = block->rect.xmax + 40;

		/* parent button on bottom */
		if (midy < block->rect.ymin) block->safety.ymin = block->rect.ymin - 3;
		else block->safety.ymin = block->rect.ymin - 40;
		/* parent button on top */
		if (midy > block->rect.ymax) block->safety.ymax = block->rect.ymax + 3;
		else block->safety.ymax = block->rect.ymax + 40;

		/* exception for switched pulldowns... */
		if (dir1 && (dir1 & block->direction) == 0) {
			if (dir2 == UI_RIGHT) block->safety.xmax = block->rect.xmax + 3;
			if (dir2 == UI_LEFT) block->safety.xmin = block->rect.xmin - 3;
		}
		block->direction = dir1;
	}

	/* keep a list of these, needed for pulldown menus */
	saferct = MEM_callocN(sizeof(uiSafetyRct), "uiSafetyRct");
	saferct->parent = butrct;
	saferct->safety = block->safety;
	BLI_freelistN(&block->saferct);
	BLI_duplicatelist(&block->saferct, &but->block->saferct);
	BLI_addhead(&block->saferct, saferct);
}

static void ui_block_region_draw(const bContext *C, ARegion *ar)
{
	uiBlock *block;

	for (block = ar->uiblocks.first; block; block = block->next)
		uiDrawBlock(C, block);
}

static void ui_popup_block_clip(wmWindow *window, uiBlock *block)
{
	uiBut *bt;
	float xofs = 0.0f;
	int width = UI_SCREEN_MARGIN;
	int winx, winy;

	if (block->flag & UI_BLOCK_NO_WIN_CLIP) {
		return;
	}

	winx = WM_window_pixels_x(window);
	winy = WM_window_pixels_y(window);
	
	/* shift menus to right if outside of view */
	if (block->rect.xmin < width) {
		xofs = (width - block->rect.xmin);
		block->rect.xmin += xofs;
		block->rect.xmax += xofs;
	}
	/* or shift to left if outside of view */
	if (block->rect.xmax > winx - width) {
		xofs = winx - width - block->rect.xmax;
		block->rect.xmin += xofs;
		block->rect.xmax += xofs;
	}
	
	if (block->rect.ymin < width)
		block->rect.ymin = width;
	if (block->rect.ymax > winy - MENU_TOP)
		block->rect.ymax = winy - MENU_TOP;
	
	/* ensure menu items draw inside left/right boundary */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		bt->rect.xmin += xofs;
		bt->rect.xmax += xofs;
	}

}

void ui_popup_block_scrolltest(uiBlock *block)
{
	uiBut *bt;
	
	block->flag &= ~(UI_BLOCK_CLIPBOTTOM | UI_BLOCK_CLIPTOP);
	
	for (bt = block->buttons.first; bt; bt = bt->next)
		bt->flag &= ~UI_SCROLLED;
	
	if (block->buttons.first == block->buttons.last)
		return;
	
	/* mark buttons that are outside boundary */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		if (bt->rect.ymin < block->rect.ymin) {
			bt->flag |= UI_SCROLLED;
			block->flag |= UI_BLOCK_CLIPBOTTOM;
		}
		if (bt->rect.ymax > block->rect.ymax) {
			bt->flag |= UI_SCROLLED;
			block->flag |= UI_BLOCK_CLIPTOP;
		}
	}

	/* mark buttons overlapping arrows, if we have them */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		if (block->flag & UI_BLOCK_CLIPBOTTOM) {
			if (bt->rect.ymin < block->rect.ymin + UI_MENU_SCROLL_ARROW)
				bt->flag |= UI_SCROLLED;
		}
		if (block->flag & UI_BLOCK_CLIPTOP) {
			if (bt->rect.ymax > block->rect.ymax - UI_MENU_SCROLL_ARROW)
				bt->flag |= UI_SCROLLED;
		}
	}
}

uiPopupBlockHandle *ui_popup_block_create(bContext *C, ARegion *butregion, uiBut *but,
                                          uiBlockCreateFunc create_func, uiBlockHandleCreateFunc handle_create_func,
                                          void *arg)
{
	wmWindow *window = CTX_wm_window(C);
	static ARegionType type;
	ARegion *ar;
	uiBlock *block;
	uiPopupBlockHandle *handle;
	uiSafetyRct *saferct;
	int width = UI_ThemeMenuShadowWidth();

	/* create handle */
	handle = MEM_callocN(sizeof(uiPopupBlockHandle), "uiPopupBlockHandle");

	/* store context for operator */
	handle->ctx_area = CTX_wm_area(C);
	handle->ctx_region = CTX_wm_region(C);
	
	/* create area region */
	ar = ui_add_temporary_region(CTX_wm_screen(C));
	handle->region = ar;

	memset(&type, 0, sizeof(ARegionType));
	type.draw = ui_block_region_draw;
	type.regionid = RGN_TYPE_TEMPORARY;
	ar->type = &type;

	UI_add_region_handlers(&ar->handlers);

	/* create ui block */
	if (create_func)
		block = create_func(C, handle->region, arg);
	else
		block = handle_create_func(C, handle, arg);
	
	if (block->handle) {
		memcpy(block->handle, handle, sizeof(uiPopupBlockHandle));
		MEM_freeN(handle);
		handle = block->handle;
	}
	else
		block->handle = handle;

	ar->regiondata = handle;

	/* set UI_BLOCK_NUMSELECT before uiEndBlock() so we get alphanumeric keys assigned */
	if (but) {
		if (but->type == PULLDOWN) {
			block->flag |= UI_BLOCK_NUMSELECT;
		}
	}
	else {
		block->flag |= UI_BLOCK_POPUP | UI_BLOCK_NUMSELECT;
	}

	block->flag |= UI_BLOCK_LOOP;

	if (!block->endblock)
		uiEndBlock(C, block);

	/* if this is being created from a button */
	if (but) {
		block->aspect = but->block->aspect;
		ui_block_position(window, butregion, but, block);
		handle->direction = block->direction;
	}
	else {
		/* keep a list of these, needed for pulldown menus */
		saferct = MEM_callocN(sizeof(uiSafetyRct), "uiSafetyRct");
		saferct->safety = block->safety;
		BLI_addhead(&block->saferct, saferct);
	}

	/* clip block with window boundary */
	ui_popup_block_clip(window, block);
	
	/* the block and buttons were positioned in window space as in 2.4x, now
	 * these menu blocks are regions so we bring it back to region space.
	 * additionally we add some padding for the menu shadow or rounded menus */
	ar->winrct.xmin = block->rect.xmin - width;
	ar->winrct.xmax = block->rect.xmax + width;
	ar->winrct.ymin = block->rect.ymin - width;
	ar->winrct.ymax = block->rect.ymax + MENU_TOP;
	
	ui_block_translate(block, -ar->winrct.xmin, -ar->winrct.ymin);

	/* adds subwindow */
	ED_region_init(C, ar);

	/* checks which buttons are visible, sets flags to prevent draw (do after region init) */
	ui_popup_block_scrolltest(block);
	
	/* get winmat now that we actually have the subwindow */
	wmSubWindowSet(window, ar->swinid);
	
	wm_subwindow_getmatrix(window, ar->swinid, block->winmat);
	
	/* notify change and redraw */
	ED_region_tag_redraw(ar);

	return handle;
}

void ui_popup_block_free(bContext *C, uiPopupBlockHandle *handle)
{
	ui_remove_temporary_region(C, CTX_wm_screen(C), handle->region);
	
	if (handle->scrolltimer)
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), handle->scrolltimer);
	
	MEM_freeN(handle);
}

/***************************** Menu Button ***************************/

#if 0
static void ui_warp_pointer(int x, int y)
{
	/* XXX 2.50 which function to use for this? */
	/* OSX has very poor mouse-warp support, it sends events;
	 * this causes a menu being pressed immediately ... */
#  ifndef __APPLE__
	warp_pointer(x, y);
#  endif
}
#endif

/********************* Color Button ****************/

/* for picker, while editing hsv */
void ui_set_but_hsv(uiBut *but)
{
	float col[3];
	float *hsv = ui_block_hsv_get(but->block);
	
	hsv_to_rgb_v(hsv, col);
	ui_set_but_vectorf(but, col);
}

/* also used by small picker, be careful with name checks below... */
static void ui_update_block_buts_rgb(uiBlock *block, const float rgb[3])
{
	uiBut *bt;
	float *hsv = ui_block_hsv_get(block);
	struct ColorManagedDisplay *display = NULL;
	
	/* this is to keep the H and S value when V is equal to zero
	 * and we are working in HSV mode, of course!
	 */
	rgb_to_hsv_compat_v(rgb, hsv);

	if (block->color_profile)
		display = ui_block_display_get(block);
	
	/* this updates button strings, is hackish... but button pointers are on stack of caller function */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		if (bt->rnaprop) {
			
			ui_set_but_vectorf(bt, rgb);
			
		}
		else if (strcmp(bt->str, "Hex: ") == 0) {
			float rgb_gamma[3];
			double intpart;
			char col[16];
			
			/* Hex code is assumed to be in sRGB space (coming from other applications, web, etc) */
			
			copy_v3_v3(rgb_gamma, rgb);

			if (display) {
				/* make a display version, for Hex code */
				IMB_colormanagement_scene_linear_to_display_v3(rgb_gamma, display);
			}
			
			if (rgb_gamma[0] > 1.0f) rgb_gamma[0] = modf(rgb_gamma[0], &intpart);
			if (rgb_gamma[1] > 1.0f) rgb_gamma[1] = modf(rgb_gamma[1], &intpart);
			if (rgb_gamma[2] > 1.0f) rgb_gamma[2] = modf(rgb_gamma[2], &intpart);

			BLI_snprintf(col, sizeof(col), "%02X%02X%02X",
			             FTOCHAR(rgb_gamma[0]), FTOCHAR(rgb_gamma[1]), FTOCHAR(rgb_gamma[2]));
			
			strcpy(bt->poin, col);
		}
		else if (bt->str[1] == ' ') {
			if (bt->str[0] == 'R') {
				ui_set_but_val(bt, rgb[0]);
			}
			else if (bt->str[0] == 'G') {
				ui_set_but_val(bt, rgb[1]);
			}
			else if (bt->str[0] == 'B') {
				ui_set_but_val(bt, rgb[2]);
			}
			else if (bt->str[0] == 'H') {
				ui_set_but_val(bt, hsv[0]);
			}
			else if (bt->str[0] == 'S') {
				ui_set_but_val(bt, hsv[1]);
			}
			else if (bt->str[0] == 'V') {
				ui_set_but_val(bt, hsv[2]);
			}
		}

		ui_check_but(bt);
	}
}

static void do_picker_rna_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
	uiBut *but = (uiBut *)bt1;
	uiPopupBlockHandle *popup = but->block->handle;
	PropertyRNA *prop = but->rnaprop;
	PointerRNA ptr = but->rnapoin;
	float rgb[4];
	
	if (prop) {
		RNA_property_float_get_array(&ptr, prop, rgb);
		ui_update_block_buts_rgb(but->block, rgb);
	}
	
	if (popup)
		popup->menuretval = UI_RETURN_UPDATE;
}

static void do_hsv_rna_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
	uiBut *but = (uiBut *)bt1;
	uiPopupBlockHandle *popup = but->block->handle;
	float rgb[3];
	float *hsv = ui_block_hsv_get(but->block);
	
	hsv_to_rgb_v(hsv, rgb);
	
	ui_update_block_buts_rgb(but->block, rgb);
	
	if (popup)
		popup->menuretval = UI_RETURN_UPDATE;
}

static void do_hex_rna_cb(bContext *UNUSED(C), void *bt1, void *hexcl)
{
	uiBut *but = (uiBut *)bt1;
	uiPopupBlockHandle *popup = but->block->handle;
	char *hexcol = (char *)hexcl;
	float rgb[3];
	
	hex_to_rgb(hexcol, rgb, rgb + 1, rgb + 2);
	
	/* Hex code is assumed to be in sRGB space (coming from other applications, web, etc) */
	if (but->block->color_profile) {
		/* so we need to linearise it for Blender */
		ui_block_to_scene_linear_v3(but->block, rgb);
	}
	
	ui_update_block_buts_rgb(but->block, rgb);
	
	if (popup)
		popup->menuretval = UI_RETURN_UPDATE;
}

static void close_popup_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
	uiBut *but = (uiBut *)bt1;
	uiPopupBlockHandle *popup = but->block->handle;
	
	if (popup)
		popup->menuretval = UI_RETURN_OK;
}

static void picker_new_hide_reveal(uiBlock *block, short colormode)
{
	uiBut *bt;
	
	/* tag buttons */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		if (bt->func == do_picker_rna_cb && bt->type == NUMSLI && bt->rnaindex != 3) {
			/* RGB sliders (color circle and alpha are always shown) */
			if (colormode == 0) bt->flag &= ~UI_HIDDEN;
			else bt->flag |= UI_HIDDEN;
		}
		else if (bt->func == do_hsv_rna_cb) {
			/* HSV sliders */
			if (colormode == 1) bt->flag &= ~UI_HIDDEN;
			else bt->flag |= UI_HIDDEN;
		}
		else if (bt->func == do_hex_rna_cb || bt->type == LABEL) {
			/* hex input or gamma correction status label */
			if (colormode == 2) bt->flag &= ~UI_HIDDEN;
			else bt->flag |= UI_HIDDEN;
		}
	}
}

static void do_picker_new_mode_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
	uiBut *bt = bt1;
	short colormode = ui_get_but_val(bt);
	picker_new_hide_reveal(bt->block, colormode);
}

#define PICKER_H    (7.5f * U.widget_unit)
#define PICKER_W    (7.5f * U.widget_unit)
#define PICKER_SPACE    (0.3f * U.widget_unit)
#define PICKER_BAR      (0.7f * U.widget_unit)

#define PICKER_TOTAL_W  (PICKER_W + PICKER_SPACE + PICKER_BAR)

static void circle_picker(uiBlock *block, PointerRNA *ptr, PropertyRNA *prop)
{
	uiBut *bt;
	
	/* HS circle */
	bt = uiDefButR_prop(block, HSVCIRCLE, 0, "", 0, 0, PICKER_H, PICKER_W, ptr, prop, -1, 0.0, 0.0, 0, 0, "Color");
	uiButSetFunc(bt, do_picker_rna_cb, bt, NULL);
	
	/* value */
	bt = uiDefButR_prop(block, HSVCUBE, 0, "", PICKER_W + PICKER_SPACE, 0, PICKER_BAR, PICKER_H, ptr, prop, -1, 0.0, 0.0, UI_GRAD_V_ALT, 0, "Value");
	uiButSetFunc(bt, do_picker_rna_cb, bt, NULL);
}


static void square_picker(uiBlock *block, PointerRNA *ptr, PropertyRNA *prop, int type)
{
	uiBut *bt;
	int bartype = type + 3;
	
	/* HS square */
	bt = uiDefButR_prop(block, HSVCUBE, 0, "",   0, PICKER_BAR + PICKER_SPACE, PICKER_TOTAL_W, PICKER_H, ptr, prop, -1, 0.0, 0.0, type, 0, "Color");
	uiButSetFunc(bt, do_picker_rna_cb, bt, NULL);
	
	/* value */
	bt = uiDefButR_prop(block, HSVCUBE, 0, "",       0, 0, PICKER_TOTAL_W, PICKER_BAR, ptr, prop, -1, 0.0, 0.0, bartype, 0, "Value");
	uiButSetFunc(bt, do_picker_rna_cb, bt, NULL);
}


/* a HS circle, V slider, rgb/hsv/hex sliders */
static void uiBlockPicker(uiBlock *block, float rgba[4], PointerRNA *ptr, PropertyRNA *prop, bool show_picker)
{
	static short colormode = 0;  /* temp? 0=rgb, 1=hsv, 2=hex */
	uiBut *bt;
	int width, butwidth;
	static char tip[50];
	static char hexcol[128];
	float rgb_gamma[3];
	float softmin, softmax, hardmin, hardmax, step, precision;
	float *hsv = ui_block_hsv_get(block);
	int yco;
	
	ui_block_hsv_get(block);
	
	width = PICKER_TOTAL_W;
	butwidth = width - 1.5f * UI_UNIT_X;
	
	/* existence of profile means storage is in linear color space, with display correction */
	/* XXX That tip message is not use anywhere! */
	if (!block->color_profile) {
		BLI_strncpy(tip, N_("Value in Display Color Space"), sizeof(tip));
		copy_v3_v3(rgb_gamma, rgba);
	}
	else {
		BLI_strncpy(tip, N_("Value in Linear RGB Color Space"), sizeof(tip));

		/* make a display version, for Hex code */
		copy_v3_v3(rgb_gamma, rgba);
		ui_block_to_display_space_v3(block, rgb_gamma);
	}
	
	/* sneaky way to check for alpha */
	rgba[3] = FLT_MAX;

	RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);
	RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
	RNA_property_float_get_array(ptr, prop, rgba);

	switch (U.color_picker_type) {
		case USER_CP_CIRCLE:
			circle_picker(block, ptr, prop);
			break;
		case USER_CP_SQUARE_SV:
			square_picker(block, ptr, prop, UI_GRAD_SV);
			break;
		case USER_CP_SQUARE_HS:
			square_picker(block, ptr, prop, UI_GRAD_HS);
			break;
		case USER_CP_SQUARE_HV:
			square_picker(block, ptr, prop, UI_GRAD_HV);
			break;
		default:
			circle_picker(block, ptr, prop);
			break;
	}
	
	/* mode */
	yco = -1.5f * UI_UNIT_Y;
	uiBlockBeginAlign(block);
	bt = uiDefButS(block, ROW, 0, IFACE_("RGB"), 0, yco, width / 3, UI_UNIT_Y, &colormode, 0.0, 0.0, 0, 0, "");
	uiButSetFunc(bt, do_picker_new_mode_cb, bt, NULL);
	bt = uiDefButS(block, ROW, 0, IFACE_("HSV"), width / 3, yco, width / 3, UI_UNIT_Y, &colormode, 0.0, 1.0, 0, 0, "");
	uiButSetFunc(bt, do_picker_new_mode_cb, bt, NULL);
	bt = uiDefButS(block, ROW, 0, IFACE_("Hex"), 2 * width / 3, yco, width / 3, UI_UNIT_Y, &colormode, 0.0, 2.0, 0, 0, "");
	uiButSetFunc(bt, do_picker_new_mode_cb, bt, NULL);
	uiBlockEndAlign(block);

	yco = -3.0f * UI_UNIT_Y;
	if (show_picker) {
		bt = uiDefIconButO(block, BUT, "UI_OT_eyedropper_color", WM_OP_INVOKE_DEFAULT, ICON_EYEDROPPER, butwidth + 10, yco, UI_UNIT_X, UI_UNIT_Y, NULL);
		uiButSetFunc(bt, close_popup_cb, bt, NULL);
	}
	
	/* RGB values */
	uiBlockBeginAlign(block);
	bt = uiDefButR_prop(block, NUMSLI, 0, IFACE_("R:"),  0, yco, butwidth, UI_UNIT_Y, ptr, prop, 0, 0.0, 0.0, 0, 3, TIP_("Red"));
	uiButSetFunc(bt, do_picker_rna_cb, bt, NULL);
	bt = uiDefButR_prop(block, NUMSLI, 0, IFACE_("G:"),  0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, ptr, prop, 1, 0.0, 0.0, 0, 3, TIP_("Green"));
	uiButSetFunc(bt, do_picker_rna_cb, bt, NULL);
	bt = uiDefButR_prop(block, NUMSLI, 0, IFACE_("B:"),  0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, ptr, prop, 2, 0.0, 0.0, 0, 3, TIP_("Blue"));
	uiButSetFunc(bt, do_picker_rna_cb, bt, NULL);

	/* could use uiItemFullR(col, ptr, prop, -1, 0, UI_ITEM_R_EXPAND|UI_ITEM_R_SLIDER, "", ICON_NONE);
	 * but need to use uiButSetFunc for updating other fake buttons */
	
	/* HSV values */
	yco = -3.0f * UI_UNIT_Y;
	uiBlockBeginAlign(block);
	bt = uiDefButF(block, NUMSLI, 0, IFACE_("H:"),   0, yco, butwidth, UI_UNIT_Y, hsv, 0.0, 1.0, 10, 3, TIP_("Hue"));
	uiButSetFunc(bt, do_hsv_rna_cb, bt, hsv);
	bt = uiDefButF(block, NUMSLI, 0, IFACE_("S:"),   0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, hsv + 1, 0.0, 1.0, 10, 3, TIP_("Saturation"));
	uiButSetFunc(bt, do_hsv_rna_cb, bt, hsv);
	bt = uiDefButF(block, NUMSLI, 0, IFACE_("V:"),   0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, hsv + 2, 0.0, softmax, 10, 3, TIP_("Value"));
	bt->hardmax = hardmax;  /* not common but rgb  may be over 1.0 */
	uiButSetFunc(bt, do_hsv_rna_cb, bt, hsv);
	uiBlockEndAlign(block);

	if (rgba[3] != FLT_MAX) {
		bt = uiDefButR_prop(block, NUMSLI, 0, IFACE_("A: "),  0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, ptr, prop, 3, 0.0, 0.0, 0, 3, TIP_("Alpha"));
		uiButSetFunc(bt, do_picker_rna_cb, bt, NULL);
	}
	else {
		rgba[3] = 1.0f;
	}

	BLI_snprintf(hexcol, sizeof(hexcol), "%02X%02X%02X", FTOCHAR(rgb_gamma[0]), FTOCHAR(rgb_gamma[1]), FTOCHAR(rgb_gamma[2]));

	yco = -3.0f * UI_UNIT_Y;
	bt = uiDefBut(block, TEX, 0, IFACE_("Hex: "), 0, yco, butwidth, UI_UNIT_Y, hexcol, 0, 8, 0, 0, TIP_("Hex triplet for color (#RRGGBB)"));
	uiButSetFunc(bt, do_hex_rna_cb, bt, hexcol);
	uiDefBut(block, LABEL, 0, IFACE_("(Gamma Corrected)"), 0, yco - UI_UNIT_Y, butwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");

	rgb_to_hsv_v(rgba, hsv);

	picker_new_hide_reveal(block, colormode);
}


static int ui_picker_small_wheel_cb(const bContext *UNUSED(C), uiBlock *block, const wmEvent *event)
{
	float add = 0.0f;
	
	if (event->type == WHEELUPMOUSE)
		add = 0.05f;
	else if (event->type == WHEELDOWNMOUSE)
		add = -0.05f;
	
	if (add != 0.0f) {
		uiBut *but;
		
		for (but = block->buttons.first; but; but = but->next) {
			if (but->type == HSVCUBE && but->active == NULL) {
				uiPopupBlockHandle *popup = block->handle;
				float rgb[3];
				float *hsv = ui_block_hsv_get(block);
				
				ui_get_but_vectorf(but, rgb);
				
				rgb_to_hsv_compat_v(rgb, hsv);
				hsv[2] = CLAMPIS(hsv[2] + add, 0.0f, 1.0f);
				hsv_to_rgb_v(hsv, rgb);

				ui_set_but_vectorf(but, rgb);
				
				ui_update_block_buts_rgb(block, rgb);
				if (popup)
					popup->menuretval = UI_RETURN_UPDATE;
				
				return 1;
			}
		}
	}
	return 0;
}

uiBlock *ui_block_func_COLOR(bContext *C, uiPopupBlockHandle *handle, void *arg_but)
{
	uiBut *but = arg_but;
	uiBlock *block;
	bool show_picker = true;
	
	block = uiBeginBlock(C, handle->region, __func__, UI_EMBOSS);
	
	if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
		block->color_profile = false;
	}

	if (but->block) {
		/* if color block is invoked from a popup we wouldn't be able to set color properly
		 * this is because color picker will close popups first and then will try to figure
		 * out active button RNA, and of course it'll fail
		 */
		show_picker = (but->block->flag & UI_BLOCK_POPUP) == 0;
	}
	
	copy_v3_v3(handle->retvec, but->editvec);
	
	uiBlockPicker(block, handle->retvec, &but->rnapoin, but->rnaprop, show_picker);
	
	block->flag = UI_BLOCK_LOOP | UI_BLOCK_REDRAW | UI_BLOCK_KEEP_OPEN | UI_BLOCK_OUT_1 | UI_BLOCK_MOVEMOUSE_QUIT;
	uiBoundsBlock(block, 0.5 * UI_UNIT_X);
	
	block->block_event_func = ui_picker_small_wheel_cb;
	
	/* and lets go */
	block->direction = UI_TOP;
	
	return block;
}

/************************ Popup Menu Memory ****************************/

static unsigned int ui_popup_string_hash(const char *str)
{
	/* sometimes button contains hotkey, sometimes not, strip for proper compare */
	int hash;
	char *delimit = strchr(str, UI_SEP_CHAR);

	if (delimit) *delimit = '\0';
	hash = BLI_ghashutil_strhash(str);
	if (delimit) *delimit = UI_SEP_CHAR;

	return hash;
}

static unsigned int ui_popup_menu_hash(const char *str)
{
	return BLI_ghashutil_strhash(str);
}

/* but == NULL read, otherwise set */
static uiBut *ui_popup_menu_memory__internal(uiBlock *block, uiBut *but)
{
	static unsigned int mem[256];
	static bool first = true;

	const unsigned int hash = block->puphash;
	const unsigned int hash_mod = hash & 255;
	
	if (first) {
		/* init */
		memset(mem, -1, sizeof(mem));
		first = 0;
	}

	if (but) {
		/* set */
		mem[hash_mod] = ui_popup_string_hash(but->str);
		return NULL;
	}
	else {
		/* get */
		for (but = block->buttons.first; but; but = but->next)
			if (ui_popup_string_hash(but->str) == mem[hash_mod])
				return but;

		return NULL;
	}
}

uiBut *ui_popup_menu_memory_get(uiBlock *block)
{
	return ui_popup_menu_memory__internal(block, NULL);
}

void ui_popup_menu_memory_set(uiBlock *block, uiBut *but)
{
	ui_popup_menu_memory__internal(block, but);
}

/******************** Popup Menu with callback or string **********************/

struct uiPopupMenu {
	uiBlock *block;
	uiLayout *layout;
	uiBut *but;

	int mx, my;
	bool popup, slideout;

	uiMenuCreateFunc menu_func;
	void *menu_arg;
};

static uiBlock *ui_block_func_POPUP(bContext *C, uiPopupBlockHandle *handle, void *arg_pup)
{
	uiBlock *block;
	uiBut *bt;
	uiPopupMenu *pup = arg_pup;
	int offset[2], minwidth, width, height;
	char direction;
	bool flip;

	if (pup->menu_func) {
		pup->block->handle = handle;
		pup->menu_func(C, pup->layout, pup->menu_arg);
		pup->block->handle = NULL;
	}

	if (pup->but) {
		/* minimum width to enforece */
		minwidth = BLI_rctf_size_x(&pup->but->rect);

		/* settings (typically rna-enum-popups) show above the button,
		 * menu's like file-menu, show below */
		if (pup->block->direction != 0) {
			/* allow overriding the direction from menu_func */
			direction = pup->block->direction;
		}
		else if ((pup->but->type == PULLDOWN) ||
		         (uiButGetMenuType(pup->but) != NULL))
		{
			direction = UI_DOWN;
		}
		else {
			direction = UI_TOP;
		}
	}
	else {
		minwidth = 50;
		direction = UI_DOWN;
	}

	flip = (direction == UI_DOWN);

	block = pup->block;
	
	/* in some cases we create the block before the region,
	 * so we set it delayed here if necessary */
	if (BLI_findindex(&handle->region->uiblocks, block) == -1)
		uiBlockSetRegion(block, handle->region);

	block->direction = direction;

	uiBlockLayoutResolve(block, &width, &height);

	uiBlockSetFlag(block, UI_BLOCK_MOVEMOUSE_QUIT);
	
	if (pup->popup) {
		uiBlockSetFlag(block, UI_BLOCK_LOOP | UI_BLOCK_REDRAW | UI_BLOCK_NUMSELECT);
		uiBlockSetDirection(block, direction);

		/* offset the mouse position, possibly based on earlier selection */
		if ((block->flag & UI_BLOCK_POPUP_MEMORY) &&
		    (bt = ui_popup_menu_memory_get(block)))
		{
			/* position mouse on last clicked item, at 0.8*width of the
			 * button, so it doesn't overlap the text too much, also note
			 * the offset is negative because we are inverse moving the
			 * block to be under the mouse */
			offset[0] = -(bt->rect.xmin + 0.8f * BLI_rctf_size_x(&bt->rect));
			offset[1] = -(bt->rect.ymin + 0.5f * UI_UNIT_Y);
		}
		else {
			/* position mouse at 0.8*width of the button and below the tile
			 * on the first item */
			offset[0] = 0;
			for (bt = block->buttons.first; bt; bt = bt->next)
				offset[0] = min_ii(offset[0], -(bt->rect.xmin + 0.8f * BLI_rctf_size_x(&bt->rect)));

			offset[1] = 2.1 * UI_UNIT_Y;
		}

		block->minbounds = minwidth;
		uiMenuPopupBoundsBlock(block, 1, offset[0], offset[1]);
	}
	else {
		/* for a header menu we set the direction automatic */
		if (!pup->slideout && flip) {
			ScrArea *sa = CTX_wm_area(C);
			if (sa && sa->headertype == HEADERDOWN) {
				ARegion *ar = CTX_wm_region(C);
				if (ar && ar->regiontype == RGN_TYPE_HEADER) {
					uiBlockSetDirection(block, UI_TOP);
					uiBlockFlipOrder(block);
				}
			}
		}

		block->minbounds = minwidth;
		uiTextBoundsBlock(block, 3.0f * UI_UNIT_X);
	}

	/* if menu slides out of other menu, override direction */
	if (pup->slideout)
		uiBlockSetDirection(block, UI_RIGHT);

	uiEndBlock(C, block);

	return pup->block;
}

uiPopupBlockHandle *ui_popup_menu_create(bContext *C, ARegion *butregion, uiBut *but,
                                         uiMenuCreateFunc menu_func, void *arg)
{
	wmWindow *window = CTX_wm_window(C);
	uiStyle *style = UI_GetStyleDraw();
	uiPopupBlockHandle *handle;
	uiPopupMenu *pup;

	pup = MEM_callocN(sizeof(uiPopupMenu), __func__);
	pup->block = uiBeginBlock(C, NULL, __func__, UI_EMBOSSP);
	pup->block->flag |= UI_BLOCK_NUMSELECT;  /* default menus to numselect */
	pup->layout = uiBlockLayout(pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, 200, 0, MENU_PADDING, style);
	pup->slideout = but ? ui_block_is_menu(but->block) : false;
	pup->but = but;
	uiLayoutSetOperatorContext(pup->layout, WM_OP_INVOKE_REGION_WIN);

	if (!but) {
		/* no button to start from, means we are a popup */
		pup->mx = window->eventstate->x;
		pup->my = window->eventstate->y;
		pup->popup = true;
		pup->block->flag |= UI_BLOCK_NO_FLIP;
	}
	/* some enums reversing is strange, currently we have no good way to
	 * reverse some enum's but not others, so reverse all so the first menu
	 * items are always close to the mouse cursor */
	else {
#if 0
		/* if this is an rna button then we can assume its an enum
		 * flipping enums is generally not good since the order can be
		 * important [#28786] */
		if (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_ENUM) {
			pup->block->flag |= UI_BLOCK_NO_FLIP;
		}
#endif
		if (but->context)
			uiLayoutContextCopy(pup->layout, but->context);
	}

	/* menu is created from a callback */
	pup->menu_func = menu_func;
	pup->menu_arg = arg;
	
	handle = ui_popup_block_create(C, butregion, but, NULL, ui_block_func_POPUP, pup);

	if (!but) {
		handle->popup = true;

		UI_add_popup_handlers(C, &window->modalhandlers, handle);
		WM_event_add_mousemove(C);
	}
	
	MEM_freeN(pup);

	return handle;
}

/******************** Popup Menu API with begin and end ***********************/

/* only return handler, and set optional title */
uiPopupMenu *uiPupMenuBegin(bContext *C, const char *title, int icon)
{
	uiStyle *style = UI_GetStyleDraw();
	uiPopupMenu *pup = MEM_callocN(sizeof(uiPopupMenu), "popup menu");
	uiBut *but;

	pup->block = uiBeginBlock(C, NULL, __func__, UI_EMBOSSP);
	pup->block->flag |= UI_BLOCK_POPUP_MEMORY;
	pup->block->puphash = ui_popup_menu_hash(title);
	pup->layout = uiBlockLayout(pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, 200, 0, MENU_PADDING, style);

	/* note, this intentionally differs from the menu & submenu default because many operators
	 * use popups like this to select one of their options - where having invoke doesn't make sense */
	uiLayoutSetOperatorContext(pup->layout, WM_OP_EXEC_REGION_WIN);

	/* create in advance so we can let buttons point to retval already */
	pup->block->handle = MEM_callocN(sizeof(uiPopupBlockHandle), "uiPopupBlockHandle");
	
	/* create title button */
	if (title[0]) {
		char titlestr[256];
		
		if (icon) {
			BLI_snprintf(titlestr, sizeof(titlestr), " %s", title);
			uiDefIconTextBut(pup->block, LABEL, 0, icon, titlestr, 0, 0, 200, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
		}
		else {
			but = uiDefBut(pup->block, LABEL, 0, title, 0, 0, 200, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
			but->drawflag = UI_BUT_TEXT_LEFT;
		}

		uiItemS(pup->layout);
	}

	return pup;
}

/* set the whole structure to work */
void uiPupMenuEnd(bContext *C, uiPopupMenu *pup)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *menu;
	
	pup->popup = true;
	pup->mx = window->eventstate->x;
	pup->my = window->eventstate->y;
	
	menu = ui_popup_block_create(C, NULL, NULL, NULL, ui_block_func_POPUP, pup);
	menu->popup = true;
	
	UI_add_popup_handlers(C, &window->modalhandlers, menu);
	WM_event_add_mousemove(C);
	
	MEM_freeN(pup);
}

uiLayout *uiPupMenuLayout(uiPopupMenu *pup)
{
	return pup->layout;
}

/*************************** Standard Popup Menus ****************************/

void uiPupMenuReports(bContext *C, ReportList *reports)
{
	Report *report;

	uiPopupMenu *pup = NULL;
	uiLayout *layout;

	if (!CTX_wm_window(C))
		return;

	for (report = reports->list.first; report; report = report->next) {
		int icon;
		const char *msg, *msg_next;

		if (report->type < reports->printlevel) {
			continue;
		}

		if (pup == NULL) {
			char title[UI_MAX_DRAW_STR];
			BLI_snprintf(title, sizeof(title), "%s: %s", IFACE_("Report"), report->typestr);
			pup = uiPupMenuBegin(C, title, ICON_NONE);
			layout = uiPupMenuLayout(pup);
		}
		else {
			uiItemS(layout);
		}

		/* split each newline into a label */
		msg = report->message;
		icon = uiIconFromReportType(report->type);
		do {
			char buf[UI_MAX_DRAW_STR];
			msg_next = strchr(msg, '\n');
			if (msg_next) {
				msg_next++;
				BLI_strncpy(buf, msg, MIN2(sizeof(buf), msg_next - msg));
				msg = buf;
			}
			uiItemL(layout, msg, icon);
			icon = ICON_NONE;
		} while ((msg = msg_next) && *msg);
	}

	if (pup) {
		uiPupMenuEnd(C, pup);
	}
}

bool uiPupMenuInvoke(bContext *C, const char *idname, ReportList *reports)
{
	uiPopupMenu *pup;
	uiLayout *layout;
	Menu menu;
	MenuType *mt = WM_menutype_find(idname, true);

	if (mt == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Menu \"%s\" not found", idname);
		return false;
	}

	if (mt->poll && mt->poll(C, mt) == 0)
		return false;

	pup = uiPupMenuBegin(C, IFACE_(mt->label), ICON_NONE);
	layout = uiPupMenuLayout(pup);

	menu.layout = layout;
	menu.type = mt;

	if (G.debug & G_DEBUG_WM) {
		printf("%s: opening menu \"%s\"\n", __func__, idname);
	}

	mt->draw(C, &menu);

	uiPupMenuEnd(C, pup);

	return true;
}


/*************************** Popup Block API **************************/

void uiPupBlockO(bContext *C, uiBlockCreateFunc func, void *arg, const char *opname, int opcontext)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *handle;
	
	handle = ui_popup_block_create(C, NULL, NULL, func, NULL, arg);
	handle->popup = true;
	handle->optype = (opname) ? WM_operatortype_find(opname, 0) : NULL;
	handle->opcontext = opcontext;
	
	UI_add_popup_handlers(C, &window->modalhandlers, handle);
	WM_event_add_mousemove(C);
}

void uiPupBlock(bContext *C, uiBlockCreateFunc func, void *arg)
{
	uiPupBlockO(C, func, arg, NULL, 0);
}

void uiPupBlockEx(bContext *C, uiBlockCreateFunc func, uiBlockHandleFunc popup_func, uiBlockCancelFunc cancel_func, void *arg)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *handle;
	
	handle = ui_popup_block_create(C, NULL, NULL, func, NULL, arg);
	handle->popup = true;
	handle->retvalue = 1;

	handle->popup_arg = arg;
	handle->popup_func = popup_func;
	handle->cancel_func = cancel_func;
	// handle->opcontext = opcontext;
	
	UI_add_popup_handlers(C, &window->modalhandlers, handle);
	WM_event_add_mousemove(C);
}

#if 0 /* UNUSED */
void uiPupBlockOperator(bContext *C, uiBlockCreateFunc func, wmOperator *op, int opcontext)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *handle;
	
	handle = ui_popup_block_create(C, NULL, NULL, func, NULL, op);
	handle->popup = 1;
	handle->retvalue = 1;

	handle->popup_arg = op;
	handle->popup_func = operator_cb;
	handle->cancel_func = confirm_cancel_operator;
	handle->opcontext = opcontext;
	
	UI_add_popup_handlers(C, &window->modalhandlers, handle);
	WM_event_add_mousemove(C);
}
#endif

void uiPupBlockClose(bContext *C, uiBlock *block)
{
	if (block->handle) {
		wmWindow *win = CTX_wm_window(C);

		/* if loading new .blend while popup is open, window will be NULL */
		if (win) {
			UI_remove_popup_handlers(&win->modalhandlers, block->handle);
			ui_popup_block_free(C, block->handle);
		}
	}
}

float *ui_block_hsv_get(uiBlock *block)
{
	return block->_hsv;
}
