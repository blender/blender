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
#include "BLI_ghash.h"

#include "PIL_time.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_report.h"
#include "BKE_global.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_draw.h"
#include "wm_subwindow.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "ED_screen.h"

#include "IMB_colormanagement.h"

#include "interface_intern.h"

#define MENU_PADDING		(int)(0.2f * UI_UNIT_Y)
#define MENU_BORDER			(int)(0.3f * U.widget_unit)


bool ui_but_menu_step_poll(const uiBut *but)
{
	BLI_assert(but->type == UI_BTYPE_MENU);

	/* currently only RNA buttons */
	return ((but->menu_step_func != NULL) ||
	        (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_ENUM));
}

int ui_but_menu_step(uiBut *but, int direction)
{
	if (ui_but_menu_step_poll(but)) {
		if (but->menu_step_func) {
			return but->menu_step_func(but->block->evil_C, direction, but->poin);
		}
		else {
			const int curval = RNA_property_enum_get(&but->rnapoin, but->rnaprop);
			return RNA_property_enum_step(but->block->evil_C, &but->rnapoin, but->rnaprop, curval, direction);
		}
	}

	printf("%s: cannot cycle button '%s'\n", __func__, but->str);
	return 0;
}

/******************** Creating Temporary regions ******************/

static ARegion *ui_region_temp_add(bScreen *sc)
{
	ARegion *ar;

	ar = MEM_callocN(sizeof(ARegion), "area region");
	BLI_addtail(&sc->regionbase, ar);

	ar->regiontype = RGN_TYPE_TEMPORARY;
	ar->alignment = RGN_ALIGN_FLOAT;

	return ar;
}

static void ui_region_temp_remove(bContext *C, bScreen *sc, ARegion *ar)
{
	wmWindow *win = CTX_wm_window(C);
	if (win)
		wm_draw_region_clear(win, ar);

	ED_region_exit(C, ar);
	BKE_area_region_free(NULL, ar);     /* NULL: no spacetype */
	BLI_freelinkN(&sc->regionbase, ar);
}

/************************* Creating Tooltips **********************/

#define UI_TIP_PAD_FAC      1.3f
#define UI_TIP_PADDING      (int)(UI_TIP_PAD_FAC * UI_UNIT_Y)
#define UI_TIP_MAXWIDTH     600

#define MAX_TOOLTIP_LINES 8
typedef struct uiTooltipData {
	rcti bbox;
	uiFontStyle fstyle;
	char lines[MAX_TOOLTIP_LINES][2048];
	char header[2048], active_info[2048];
	struct {
		enum {
			UI_TIP_STYLE_NORMAL = 0,
			UI_TIP_STYLE_HEADER,
			UI_TIP_STYLE_MONO,
		} style : 3;
		enum {
			UI_TIP_LC_MAIN = 0,     /* primary text */
			UI_TIP_LC_VALUE,        /* the value of buttons (also shortcuts) */
			UI_TIP_LC_ACTIVE,       /* titles of active enum values */
			UI_TIP_LC_NORMAL,       /* regular text */
			UI_TIP_LC_PYTHON,       /* Python snippet */
			UI_TIP_LC_ALERT,        /* description of why operator can't run */
		} color_id : 4;
		int is_pad : 1;
	} format[MAX_TOOLTIP_LINES];

	struct {
		unsigned int x_pos;     /* x cursor position at the end of the last line */
		unsigned int lines;     /* number of lines, 1 or more with word-wrap */
	} line_geom[MAX_TOOLTIP_LINES];

	int wrap_width;

	int totline;
	int toth, lineh;
} uiTooltipData;

#define UI_TIP_LC_MAX 6

BLI_STATIC_ASSERT(UI_TIP_LC_MAX == UI_TIP_LC_ALERT + 1, "invalid lc-max");
BLI_STATIC_ASSERT(sizeof(((uiTooltipData *)NULL)->format[0]) <= sizeof(int), "oversize");

static void rgb_tint(
        float col[3],
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
	const float pad_px = UI_TIP_PADDING;
	uiTooltipData *data = ar->regiondata;
	uiWidgetColors *theme = ui_tooltip_get_theme();
	rcti bbox = data->bbox;
	float tip_colors[UI_TIP_LC_MAX][3];

	float *main_color    = tip_colors[UI_TIP_LC_MAIN]; /* the color from the theme */
	float *value_color   = tip_colors[UI_TIP_LC_VALUE];
	float *active_color  = tip_colors[UI_TIP_LC_ACTIVE];
	float *normal_color  = tip_colors[UI_TIP_LC_NORMAL];
	float *python_color  = tip_colors[UI_TIP_LC_PYTHON];
	float *alert_color   = tip_colors[UI_TIP_LC_ALERT];

	float background_color[3];
	float tone_bg;
	int i, multisample_enabled;

	/* disable AA, makes widgets too blurry */
	multisample_enabled = glIsEnabled(GL_MULTISAMPLE);
	if (multisample_enabled)
		glDisable(GL_MULTISAMPLE);

	wmOrtho2_region_pixelspace(ar);

	/* draw background */
	ui_draw_tooltip_background(UI_style_get(), NULL, &bbox);

	/* set background_color */
	rgb_uchar_to_float(background_color, (const unsigned char *)theme->inner);

	/* calculate normal_color */
	rgb_uchar_to_float(main_color, (const unsigned char *)theme->text);
	copy_v3_v3(active_color, main_color);
	copy_v3_v3(normal_color, main_color);
	copy_v3_v3(python_color, main_color);
	copy_v3_v3(alert_color, main_color);
	copy_v3_v3(value_color, main_color);

	/* find the brightness difference between background and text colors */
	
	tone_bg = rgb_to_grayscale(background_color);
	/* tone_fg = rgb_to_grayscale(main_color); */

	/* mix the colors */
	rgb_tint(value_color,  0.0f, 0.0f, tone_bg, 0.2f);  /* light gray */
	rgb_tint(active_color, 0.6f, 0.2f, tone_bg, 0.2f);  /* light blue */
	rgb_tint(normal_color, 0.0f, 0.0f, tone_bg, 0.4f);  /* gray       */
	rgb_tint(python_color, 0.0f, 0.0f, tone_bg, 0.5f);  /* dark gray  */
	rgb_tint(alert_color,  0.0f, 0.8f, tone_bg, 0.1f);  /* red        */

	/* draw text */
	BLF_wordwrap(data->fstyle.uifont_id, data->wrap_width);
	BLF_wordwrap(blf_mono_font, data->wrap_width);

	bbox.xmin += 0.5f * pad_px;  /* add padding to the text */
	bbox.ymax -= 0.25f * pad_px;

	for (i = 0; i < data->totline; i++) {
		bbox.ymin = bbox.ymax - (data->lineh * data->line_geom[i].lines);
		if (data->format[i].style == UI_TIP_STYLE_HEADER) {
			/* draw header and active data (is done here to be able to change color) */
			uiFontStyle fstyle_header = data->fstyle;
			float xofs, yofs;

			/* override text-style */
			fstyle_header.shadow = 1;
			fstyle_header.shadowcolor = rgb_to_grayscale(tip_colors[UI_TIP_LC_MAIN]);
			fstyle_header.shadx = fstyle_header.shady = 0;
			fstyle_header.shadowalpha = 1.0f;
			fstyle_header.word_wrap = true;

			UI_fontstyle_set(&fstyle_header);
			glColor3fv(tip_colors[UI_TIP_LC_MAIN]);
			UI_fontstyle_draw(&fstyle_header, &bbox, data->header);

			/* offset to the end of the last line */
			xofs = data->line_geom[i].x_pos;
			yofs = data->lineh * (data->line_geom[i].lines - 1);
			bbox.xmin += xofs;
			bbox.ymax -= yofs;

			glColor3fv(tip_colors[UI_TIP_LC_ACTIVE]);
			fstyle_header.shadow = 0;
			UI_fontstyle_draw(&fstyle_header, &bbox, data->active_info);

			/* undo offset */
			bbox.xmin -= xofs;
			bbox.ymax += yofs;
		}
		else if (data->format[i].style == UI_TIP_STYLE_MONO) {
			uiFontStyle fstyle_mono = data->fstyle;
			fstyle_mono.uifont_id = blf_mono_font;
			fstyle_mono.word_wrap = true;

			UI_fontstyle_set(&fstyle_mono);
			/* XXX, needed because we dont have mono in 'U.uifonts' */
			BLF_size(fstyle_mono.uifont_id, fstyle_mono.points * U.pixelsize, U.dpi);
			glColor3fv(tip_colors[data->format[i].color_id]);
			UI_fontstyle_draw(&fstyle_mono, &bbox, data->lines[i]);
		}
		else {
			uiFontStyle fstyle_normal = data->fstyle;
			BLI_assert(data->format[i].style == UI_TIP_STYLE_NORMAL);
			fstyle_normal.word_wrap = true;

			/* draw remaining data */
			UI_fontstyle_set(&fstyle_normal);
			glColor3fv(tip_colors[data->format[i].color_id]);
			UI_fontstyle_draw(&fstyle_normal, &bbox, data->lines[i]);
		}

		bbox.ymax -= data->lineh * data->line_geom[i].lines;

		if ((i + 1 != data->totline) && data->format[i + 1].is_pad) {
			bbox.ymax -= data->lineh * (UI_TIP_PAD_FAC - 1);
		}
	}

	BLF_disable(data->fstyle.uifont_id, BLF_WORD_WRAP);
	BLF_disable(blf_mono_font, BLF_WORD_WRAP);

	if (multisample_enabled)
		glEnable(GL_MULTISAMPLE);
}

static void ui_tooltip_region_free_cb(ARegion *ar)
{
	uiTooltipData *data;

	data = ar->regiondata;
	MEM_freeN(data);
	ar->regiondata = NULL;
}

static uiTooltipData *ui_tooltip_data_from_button(bContext *C, uiBut *but)
{
	uiStringInfo but_tip = {BUT_GET_TIP, NULL};
	uiStringInfo enum_label = {BUT_GET_RNAENUM_LABEL, NULL};
	uiStringInfo enum_tip = {BUT_GET_RNAENUM_TIP, NULL};
	uiStringInfo op_keymap = {BUT_GET_OP_KEYMAP, NULL};
	uiStringInfo prop_keymap = {BUT_GET_PROP_KEYMAP, NULL};
	uiStringInfo rna_struct = {BUT_GET_RNASTRUCT_IDENTIFIER, NULL};
	uiStringInfo rna_prop = {BUT_GET_RNAPROP_IDENTIFIER, NULL};

	char buf[512];

	/* create tooltip data */
	uiTooltipData *data = MEM_callocN(sizeof(uiTooltipData), "uiTooltipData");

	UI_but_string_info_get(C, but, &but_tip, &enum_label, &enum_tip, &op_keymap, &prop_keymap, &rna_struct, &rna_prop, NULL);

	/* Tip */
	if (but_tip.strinfo) {
		if (enum_label.strinfo) {
			BLI_snprintf(data->header, sizeof(data->header), "%s:  ", but_tip.strinfo);
			BLI_strncpy(data->active_info, enum_label.strinfo, sizeof(data->lines[0]));
		}
		else {
			BLI_snprintf(data->header, sizeof(data->header), "%s.", but_tip.strinfo);
		}
		data->format[data->totline].style = UI_TIP_STYLE_HEADER;
		data->totline++;

		/* special case enum rna buttons */
		if ((but->type & UI_BTYPE_ROW) && but->rnaprop && RNA_property_flag(but->rnaprop) & PROP_ENUM_FLAG) {
			BLI_strncpy(data->lines[data->totline], IFACE_("(Shift-Click/Drag to select multiple)"),
			            sizeof(data->lines[0]));

			data->format[data->totline].color_id = UI_TIP_LC_NORMAL;
			data->totline++;
		}

	}
	/* Enum item label & tip */
	if (enum_tip.strinfo) {
		BLI_strncpy(data->lines[data->totline], enum_tip.strinfo, sizeof(data->lines[0]));
		data->format[data->totline].is_pad = true;
		data->format[data->totline].color_id = UI_TIP_LC_VALUE;
		data->totline++;
	}

	/* Op shortcut */
	if (op_keymap.strinfo) {
		BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Shortcut: %s"), op_keymap.strinfo);
		data->format[data->totline].is_pad = true;
		data->format[data->totline].color_id = UI_TIP_LC_VALUE;
		data->totline++;
	}
	
	/* Property context-toggle shortcut */
	if (prop_keymap.strinfo) {
		BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Shortcut: %s"), prop_keymap.strinfo);
		data->format[data->totline].is_pad = true;
		data->format[data->totline].color_id = UI_TIP_LC_VALUE;
		data->totline++;
	}

	if (ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU)) {
		/* better not show the value of a password */
		if ((but->rnaprop && (RNA_property_subtype(but->rnaprop) == PROP_PASSWORD)) == 0) {
			/* full string */
			ui_but_string_get(but, buf, sizeof(buf));
			if (buf[0]) {
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Value: %s"), buf);
				data->format[data->totline].is_pad = true;
				data->format[data->totline].color_id = UI_TIP_LC_VALUE;
				data->totline++;
			}
		}
	}

	if (but->rnaprop) {
		int unit_type = UI_but_unit_type_get(but);
		
		if (unit_type == PROP_UNIT_ROTATION) {
			if (RNA_property_type(but->rnaprop) == PROP_FLOAT) {
				float value = RNA_property_array_check(but->rnaprop) ?
				                  RNA_property_float_get_index(&but->rnapoin, but->rnaprop, but->rnaindex) :
				                  RNA_property_float_get(&but->rnapoin, but->rnaprop);
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Radians: %f"), value);
				data->format[data->totline].color_id = UI_TIP_LC_NORMAL;
				data->totline++;
			}
		}
		
		if (but->flag & UI_BUT_DRIVEN) {
			if (ui_but_anim_expression_get(but, buf, sizeof(buf))) {
				/* expression */
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Expression: %s"), buf);
				data->format[data->totline].color_id = UI_TIP_LC_NORMAL;
				data->totline++;
			}
		}

		if (but->rnapoin.id.data) {
			ID *id = but->rnapoin.id.data;
			if (ID_IS_LINKED_DATABLOCK(id)) {
				BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Library: %s"), id->lib->name);
				data->format[data->totline].color_id = UI_TIP_LC_NORMAL;
				data->totline++;
			}
		}
	}
	else if (but->optype) {
		PointerRNA *opptr;
		char *str;
		opptr = UI_but_operator_ptr_get(but); /* allocated when needed, the button owns it */

		/* so the context is passed to itemf functions (some py itemf functions use it) */
		WM_operator_properties_sanitize(opptr, false);

		str = WM_operator_pystring_ex(C, NULL, false, false, but->optype, opptr);

		/* avoid overly verbose tips (eg, arrays of 20 layers), exact limit is arbitrary */
		WM_operator_pystring_abbreviate(str, 32);

		/* operator info */
		if ((U.flag & USER_TOOLTIPS_PYTHON) == 0) {
			BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Python: %s"), str);
			data->format[data->totline].style = UI_TIP_STYLE_MONO;
			data->format[data->totline].is_pad = true;
			data->format[data->totline].color_id = UI_TIP_LC_PYTHON;
			data->totline++;
		}

		MEM_freeN(str);
	}

	/* button is disabled, we may be able to tell user why */
	if (but->flag & UI_BUT_DISABLED) {
		const char *disabled_msg = NULL;

		/* if operator poll check failed, it can give pretty precise info why */
		if (but->optype) {
			CTX_wm_operator_poll_msg_set(C, NULL);
			WM_operator_poll_context(C, but->optype, but->opcontext);
			disabled_msg = CTX_wm_operator_poll_msg_get(C);
		}
		/* alternatively, buttons can store some reasoning too */
		else if (but->disabled_info) {
			disabled_msg = TIP_(but->disabled_info);
		}

		if (disabled_msg && disabled_msg[0]) {
			BLI_snprintf(data->lines[data->totline], sizeof(data->lines[0]), TIP_("Disabled: %s"), disabled_msg);
			data->format[data->totline].color_id = UI_TIP_LC_ALERT;
			data->totline++;
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
		data->format[data->totline].style = UI_TIP_STYLE_MONO;
		data->format[data->totline].is_pad = true;
		data->format[data->totline].color_id = UI_TIP_LC_PYTHON;
		data->totline++;

		if (but->rnapoin.id.data) {
			/* this could get its own 'BUT_GET_...' type */

			/* never fails */
			char *id_path;

			if (but->rnaprop) {
				id_path = RNA_path_full_property_py_ex(&but->rnapoin, but->rnaprop, but->rnaindex, true);
			}
			else {
				id_path = RNA_path_full_struct_py(&but->rnapoin);
			}

			BLI_strncat_utf8(data->lines[data->totline], id_path, sizeof(data->lines[0]));
			MEM_freeN(id_path);

			data->format[data->totline].style = UI_TIP_STYLE_MONO;
			data->format[data->totline].color_id = UI_TIP_LC_PYTHON;
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
	else {
		return data;
	}
}

ARegion *ui_tooltip_create(bContext *C, ARegion *butregion, uiBut *but)
{
	const float pad_px = UI_TIP_PADDING;
	wmWindow *win = CTX_wm_window(C);
	const int winx = WM_window_pixels_x(win);
	uiStyle *style = UI_style_get();
	static ARegionType type;
	ARegion *ar;
/*	IDProperty *prop;*/
	/* aspect values that shrink text are likely unreadable */
	const float aspect = min_ff(1.0f, but->block->aspect);
	int fonth, fontw;
	int ofsx, ofsy, h, i;
	rctf rect_fl;
	rcti rect_i;
	int font_flag = 0;

	if (but->drawflag & UI_BUT_NO_TOOLTIP) {
		return NULL;
	}

	uiTooltipData *data = ui_tooltip_data_from_button(C, but);
	if (data == NULL) {
		return NULL;
	}

	/* create area region */
	ar = ui_region_temp_add(CTX_wm_screen(C));

	memset(&type, 0, sizeof(ARegionType));
	type.draw = ui_tooltip_region_draw_cb;
	type.free = ui_tooltip_region_free_cb;
	type.regionid = RGN_TYPE_TEMPORARY;
	ar->type = &type;
	
	/* set font, get bb */
	data->fstyle = style->widget; /* copy struct */
	ui_fontscale(&data->fstyle.points, aspect);

	UI_fontstyle_set(&data->fstyle);

	data->wrap_width = min_ii(UI_TIP_MAXWIDTH * U.pixelsize / aspect, winx - (UI_TIP_PADDING * 2));

	font_flag |= BLF_WORD_WRAP;
	if (data->fstyle.kerning == 1) {
		font_flag |= BLF_KERNING_DEFAULT;
	}
	BLF_enable(data->fstyle.uifont_id, font_flag);
	BLF_enable(blf_mono_font, font_flag);
	BLF_wordwrap(data->fstyle.uifont_id, data->wrap_width);
	BLF_wordwrap(blf_mono_font, data->wrap_width);

	/* these defines tweaked depending on font */
#define TIP_BORDER_X (16.0f / aspect)
#define TIP_BORDER_Y (6.0f / aspect)

	h = BLF_height_max(data->fstyle.uifont_id);

	for (i = 0, fontw = 0, fonth = 0; i < data->totline; i++) {
		struct ResultBLF info;
		int w, x_pos = 0;

		if (data->format[i].style == UI_TIP_STYLE_HEADER) {
			w = BLF_width_ex(data->fstyle.uifont_id, data->header, sizeof(data->header), &info);
			/* check for enum label */
			if (data->active_info[0]) {
				x_pos = info.width;
				w = max_ii(w, x_pos + BLF_width(data->fstyle.uifont_id, data->active_info, sizeof(data->active_info)));
			}
		}
		else if (data->format[i].style == UI_TIP_STYLE_MONO) {
			BLF_size(blf_mono_font, data->fstyle.points * U.pixelsize, U.dpi);

			w = BLF_width_ex(blf_mono_font, data->lines[i], sizeof(data->lines[i]), &info);
		}
		else {
			BLI_assert(data->format[i].style == UI_TIP_STYLE_NORMAL);

			w = BLF_width_ex(data->fstyle.uifont_id, data->lines[i], sizeof(data->lines[i]), &info);
		}

		fontw = max_ii(fontw, w);

		fonth += h * info.lines;
		if ((i + 1 != data->totline) && data->format[i + 1].is_pad) {
			fonth += h * (UI_TIP_PAD_FAC - 1);
		}

		data->line_geom[i].lines = info.lines;
		data->line_geom[i].x_pos = x_pos;
	}

	//fontw *= aspect;

	BLF_disable(data->fstyle.uifont_id, font_flag);
	BLF_disable(blf_mono_font, font_flag);

	ar->regiondata = data;

	data->toth = fonth;
	data->lineh = h;

	/* compute position */
	ofsx = 0; //(but->block->panel) ? but->block->panel->ofsx : 0;
	ofsy = 0; //(but->block->panel) ? but->block->panel->ofsy : 0;

	rect_fl.xmin = BLI_rctf_cent_x(&but->rect) + ofsx - TIP_BORDER_X;
	rect_fl.xmax = rect_fl.xmin + fontw + pad_px;
	rect_fl.ymax = but->rect.ymin + ofsy - TIP_BORDER_Y;
	rect_fl.ymin = rect_fl.ymax - fonth  - TIP_BORDER_Y;
	
	/* since the text has beens caled already, the size of tooltips is defined now */
	/* here we try to figure out the right location */
	if (butregion) {
		float mx, my;
		float ofsx_fl = rect_fl.xmin, ofsy_fl = rect_fl.ymax;
		ui_block_to_window_fl(butregion, but->block, &ofsx_fl, &ofsy_fl);

#if 1
		/* use X mouse location */
		mx = (win->eventstate->x + (TIP_BORDER_X * 2)) - BLI_rctf_cent_x(&but->rect);
#else
		mx = ofsx_fl - rect_fl.xmin;
#endif
		my = ofsy_fl - rect_fl.ymax;

		BLI_rctf_translate(&rect_fl, mx, my);
	}
	BLI_rcti_rctf_copy(&rect_i, &rect_fl);

#undef TIP_BORDER_X
#undef TIP_BORDER_Y

	/* clip with window boundaries */
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

	/* add padding */
	BLI_rcti_resize(&rect_i,
	                BLI_rcti_size_x(&rect_i) + pad_px,
	                BLI_rcti_size_y(&rect_i) + pad_px);

	/* widget rect, in region coords */
	{
		const int margin = UI_POPUP_MARGIN;
		
		data->bbox.xmin = margin;
		data->bbox.xmax = BLI_rcti_size_x(&rect_i) - margin;
		data->bbox.ymin = margin;
		data->bbox.ymax = BLI_rcti_size_y(&rect_i);
		
		/* region bigger for shadow */
		ar->winrct.xmin = rect_i.xmin - margin;
		ar->winrct.xmax = rect_i.xmax + margin;
		ar->winrct.ymin = rect_i.ymin - margin;
		ar->winrct.ymax = rect_i.ymax + margin;
	}

	/* adds subwindow */
	ED_region_init(C, ar);
	
	/* notify change and redraw */
	ED_region_tag_redraw(ar);

	return ar;
}

void ui_tooltip_free(bContext *C, ARegion *ar)
{
	ui_region_temp_remove(C, CTX_wm_screen(C), ar);
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
bool UI_search_item_add(uiSearchItems *items, const char *name, void *poin, int iconid)
{
	/* hijack for autocomplete */
	if (items->autocpl) {
		UI_autocomplete_update_name(items->autocpl, name);
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

int UI_searchbox_size_y(void)
{
	return SEARCH_ITEMS * UI_UNIT_Y + 2 * UI_POPUP_MENU_TOP;
}

int UI_searchbox_size_x(void)
{
	return 12 * UI_UNIT_X;
}

int UI_search_items_find_index(uiSearchItems *items, const char *name)
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
		else {
			/* only let users step into an 'unset' state for unlink buttons */
			data->active = (but->flag & UI_BUT_VALUE_CLEAR) ? -1 : 0;
		}
	}
	
	ED_region_tag_redraw(ar);
}

static void ui_searchbox_butrect(rcti *r_rect, uiSearchboxData *data, int itemnr)
{
	/* thumbnail preview */
	if (data->preview) {
		int butw = (BLI_rcti_size_x(&data->bbox) - 2 * MENU_BORDER) / data->prv_cols;
		int buth = (BLI_rcti_size_y(&data->bbox) - 2 * MENU_BORDER) / data->prv_rows;
		int row, col;
		
		*r_rect = data->bbox;
		
		col = itemnr % data->prv_cols;
		row = itemnr / data->prv_cols;
		
		r_rect->xmin += MENU_BORDER + (col * butw);
		r_rect->xmax = r_rect->xmin + butw;
		
		r_rect->ymax -= MENU_BORDER + (row * buth);
		r_rect->ymin = r_rect->ymax - buth;
	}
	/* list view */
	else {
		int buth = (BLI_rcti_size_y(&data->bbox) - 2 * UI_POPUP_MENU_TOP) / SEARCH_ITEMS;
		
		*r_rect = data->bbox;
		r_rect->xmin = data->bbox.xmin + 3.0f;
		r_rect->xmax = data->bbox.xmax - 3.0f;
		
		r_rect->ymax = data->bbox.ymax - UI_POPUP_MENU_TOP - itemnr * buth;
		r_rect->ymin = r_rect->ymax - buth;
	}
	
}

int ui_searchbox_find_index(ARegion *ar, const char *name)
{
	uiSearchboxData *data = ar->regiondata;
	return UI_search_items_find_index(&data->items, name);
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
		const char *name_sep = data->use_sep ? strrchr(name, UI_SEP_CHAR) : NULL;

		BLI_strncpy(but->editstr, name, name_sep ? (name_sep - name) : data->items.maxstrlen);
		
		but->func_arg2 = data->items.pointers[data->active];

		return true;
	}
	else if (but->flag & UI_BUT_VALUE_CLEAR) {
		/* It is valid for _VALUE_CLEAR flavor to have no active element (it's a valid way to unlink). */
		but->editstr[0] = '\0';

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
			const char *name_sep = data->use_sep ? strrchr(name, UI_SEP_CHAR) : NULL;
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
		data->items.autocpl = UI_autocomplete_begin(str, ui_but_string_get_max_length(but));

		but->search_func(C, but->search_arg, but->editstr, &data->items);

		match = UI_autocomplete_end(data->items.autocpl, str);
		data->items.autocpl = NULL;
	}

	return match;
}

static void ui_searchbox_region_draw_cb(const bContext *UNUSED(C), ARegion *ar)
{
	uiSearchboxData *data = ar->regiondata;
	
	/* pixel space */
	wmOrtho2_region_pixelspace(ar);

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
				ui_draw_preview_item(&data->fstyle, &rect, data->items.names[a], data->items.icons[a],
				                     (a == data->active) ? UI_ACTIVE : 0);
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

ARegion *ui_searchbox_create_generic(bContext *C, ARegion *butregion, uiBut *but)
{
	wmWindow *win = CTX_wm_window(C);
	uiStyle *style = UI_style_get();
	static ARegionType type;
	ARegion *ar;
	uiSearchboxData *data;
	float aspect = but->block->aspect;
	rctf rect_fl;
	rcti rect_i;
	const int margin = UI_POPUP_MARGIN;
	int winx /*, winy */, ofsx, ofsy;
	int i;
	
	/* create area region */
	ar = ui_region_temp_add(CTX_wm_screen(C));
	
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
	UI_fontstyle_set(&data->fstyle);
	
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
		const int search_but_h = BLI_rctf_size_y(&but->rect) + 10;
		/* this case is search menu inside other menu */
		/* we copy region size */

		ar->winrct = butregion->winrct;
		
		/* widget rect, in region coords */
		data->bbox.xmin = margin;
		data->bbox.xmax = BLI_rcti_size_x(&ar->winrct) - margin;
		data->bbox.ymin = margin;
		data->bbox.ymax = BLI_rcti_size_y(&ar->winrct) - margin;
		
		/* check if button is lower half */
		if (but->rect.ymax < BLI_rctf_cent_y(&but->block->rect)) {
			data->bbox.ymin += search_but_h;
		}
		else {
			data->bbox.ymax -= search_but_h;
		}
	}
	else {
		const int searchbox_width = UI_searchbox_size_x();

		rect_fl.xmin = but->rect.xmin - 5;   /* align text with button */
		rect_fl.xmax = but->rect.xmax + 5;   /* symmetrical */
		rect_fl.ymax = but->rect.ymin;
		rect_fl.ymin = rect_fl.ymax - UI_searchbox_size_y();

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
			UI_view2d_view_to_region_rcti(&butregion->v2d, &rect_fl, &rect_i);
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
				newy1 = UI_view2d_view_to_region_y(&butregion->v2d, newy1);

			newy1 += butregion->winrct.ymin;

			rect_i.ymax = BLI_rcti_size_y(&rect_i) + newy1;
			rect_i.ymin = newy1;
		}

		/* widget rect, in region coords */
		data->bbox.xmin = margin;
		data->bbox.xmax = BLI_rcti_size_x(&rect_i) + margin;
		data->bbox.ymin = margin;
		data->bbox.ymax = BLI_rcti_size_y(&rect_i) + margin;
		
		/* region bigger for shadow */
		ar->winrct.xmin = rect_i.xmin - margin;
		ar->winrct.xmax = rect_i.xmax + margin;
		ar->winrct.ymin = rect_i.ymin - margin;
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

/**
 * Similar to Python's `str.title` except...
 *
 * - we know words are upper case and ascii only.
 * - '_' are replaces by spaces.
 */
static void str_tolower_titlecaps_ascii(char *str, const size_t len)
{
	size_t i;
	bool prev_delim = true;

	for (i = 0; (i < len) && str[i]; i++) {
		if (str[i] >= 'A' && str[i] <= 'Z') {
			if (prev_delim == false) {
				str[i] += 'a' - 'A';
			}
		}
		else if (str[i] == '_') {
			str[i] = ' ';
		}

		prev_delim = ELEM(str[i], ' ') || (str[i] >= '0' && str[i] <= '9');
	}

}

static void ui_searchbox_region_draw_cb__operator(const bContext *UNUSED(C), ARegion *ar)
{
	uiSearchboxData *data = ar->regiondata;

	/* pixel space */
	wmOrtho2_region_pixelspace(ar);

	if (data->noback == false)
		ui_draw_search_back(NULL, NULL, &data->bbox);  /* style not used yet */

	/* draw text */
	if (data->items.totitem) {
		rcti rect;
		int a;

		/* draw items */
		for (a = 0; a < data->items.totitem; a++) {
			rcti rect_pre, rect_post;
			ui_searchbox_butrect(&rect, data, a);

			rect_pre  = rect;
			rect_post = rect;

			rect_pre.xmax = rect_post.xmin = rect.xmin + ((rect.xmax - rect.xmin) / 4);

			/* widget itself */
			/* NOTE: i18n messages extracting tool does the same, please keep it in sync. */
			{
				wmOperatorType *ot = data->items.pointers[a];

				int state = (a == data->active) ? UI_ACTIVE : 0;
				char  text_pre[128];
				char *text_pre_p = strstr(ot->idname, "_OT_");
				if (text_pre_p == NULL) {
					text_pre[0] = '\0';
				}
				else {
					int text_pre_len;
					text_pre_p += 1;
					text_pre_len = BLI_strncpy_rlen(
					        text_pre, ot->idname, min_ii(sizeof(text_pre), text_pre_p - ot->idname));
					text_pre[text_pre_len] = ':';
					text_pre[text_pre_len + 1] = '\0';
					str_tolower_titlecaps_ascii(text_pre, sizeof(text_pre));
				}

				rect_pre.xmax += 4;  /* sneaky, avoid showing ugly margin */
				ui_draw_menu_item(&data->fstyle, &rect_pre, CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, text_pre),
				                  data->items.icons[a], state, false);
				ui_draw_menu_item(&data->fstyle, &rect_post, data->items.names[a], 0, state, data->use_sep);
			}

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

ARegion *ui_searchbox_create_operator(bContext *C, ARegion *butregion, uiBut *but)
{
	ARegion *ar;

	ar = ui_searchbox_create_generic(C, butregion, but);

	ar->type->draw = ui_searchbox_region_draw_cb__operator;

	return ar;
}

void ui_searchbox_free(bContext *C, ARegion *ar)
{
	ui_region_temp_remove(C, CTX_wm_screen(C), ar);
}

/* sets red alert if button holds a string it can't find */
/* XXX weak: search_func adds all partial matches... */
void ui_but_search_refresh(uiBut *but)
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
		UI_but_flag_enable(but, UI_BUT_REDALERT);
	}
	else if (items->more == 0) {
		if (UI_search_items_find_index(items, but->drawstr) == -1) {
			UI_but_flag_enable(but, UI_BUT_REDALERT);
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
	ui_block_to_window_rctf(butregion, but->block, &butrct, &but->rect);

	/* widget_roundbox_set has this correction too, keep in sync */
	if (but->type != UI_BTYPE_PULLDOWN) {
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
	ui_block_to_window_rctf(butregion, but->block, &block->rect, &block->rect);

	//block->rect.xmin -= 2.0; block->rect.ymin -= 2.0;
	//block->rect.xmax += 2.0; block->rect.ymax += 2.0;
	
	xsize = BLI_rctf_size_x(&block->rect) + 0.2f * UI_UNIT_X;  /* 4 for shadow */
	ysize = BLI_rctf_size_y(&block->rect) + 0.2f * UI_UNIT_Y;
	/* aspect /= (float)xsize;*/ /*UNUSED*/

	{
		bool left = 0, right = 0, top = 0, down = 0;
		int winx, winy;
		// int offscreen;

		winx = WM_window_pixels_x(window);
		winy = WM_window_pixels_y(window);
		// wm_window_get_size(window, &winx, &winy);

		if (block->direction & UI_DIR_CENTER_Y) {
			center = ysize / 2;
		}
		else {
			center = 0;
		}
		
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
		
		dir1 = (block->direction & UI_DIR_ALL);

		/* secundary directions */
		if (dir1 & (UI_DIR_UP | UI_DIR_DOWN)) {
			if      (dir1 & UI_DIR_LEFT)  dir2 = UI_DIR_LEFT;
			else if (dir1 & UI_DIR_RIGHT) dir2 = UI_DIR_RIGHT;
			dir1 &= (UI_DIR_UP | UI_DIR_DOWN);
		}

		if ((dir2 == 0) && (dir1 == UI_DIR_LEFT || dir1 == UI_DIR_RIGHT)) dir2 = UI_DIR_DOWN;
		if ((dir2 == 0) && (dir1 == UI_DIR_UP   || dir1 == UI_DIR_DOWN))  dir2 = UI_DIR_LEFT;
		
		/* no space at all? don't change */
		if (left || right) {
			if (dir1 == UI_DIR_LEFT  && left == 0)  dir1 = UI_DIR_RIGHT;
			if (dir1 == UI_DIR_RIGHT && right == 0) dir1 = UI_DIR_LEFT;
			/* this is aligning, not append! */
			if (dir2 == UI_DIR_LEFT  && right == 0) dir2 = UI_DIR_RIGHT;
			if (dir2 == UI_DIR_RIGHT && left == 0)  dir2 = UI_DIR_LEFT;
		}
		if (down || top) {
			if (dir1 == UI_DIR_UP   && top == 0)  dir1 = UI_DIR_DOWN;
			if (dir1 == UI_DIR_DOWN && down == 0) dir1 = UI_DIR_UP;
			BLI_assert(dir2 != UI_DIR_UP);
//			if (dir2 == UI_DIR_UP   && top == 0)  dir2 = UI_DIR_DOWN;
			if (dir2 == UI_DIR_DOWN && down == 0) dir2 = UI_DIR_UP;
		}

		if (dir1 == UI_DIR_LEFT) {
			xof = butrct.xmin - block->rect.xmax;
			if (dir2 == UI_DIR_UP) yof = butrct.ymin - block->rect.ymin - center - MENU_PADDING;
			else                   yof = butrct.ymax - block->rect.ymax + center + MENU_PADDING;
		}
		else if (dir1 == UI_DIR_RIGHT) {
			xof = butrct.xmax - block->rect.xmin;
			if (dir2 == UI_DIR_UP) yof = butrct.ymin - block->rect.ymin - center - MENU_PADDING;
			else                   yof = butrct.ymax - block->rect.ymax + center + MENU_PADDING;
		}
		else if (dir1 == UI_DIR_UP) {
			yof = butrct.ymax - block->rect.ymin;
			if (dir2 == UI_DIR_RIGHT) xof = butrct.xmax - block->rect.xmax;
			else                      xof = butrct.xmin - block->rect.xmin;
			/* changed direction? */
			if ((dir1 & block->direction) == 0) {
				UI_block_order_flip(block);
			}
		}
		else if (dir1 == UI_DIR_DOWN) {
			yof = butrct.ymin - block->rect.ymax;
			if (dir2 == UI_DIR_RIGHT) xof = butrct.xmax - block->rect.xmax;
			else                      xof = butrct.xmin - block->rect.xmin;
			/* changed direction? */
			if ((dir1 & block->direction) == 0) {
				UI_block_order_flip(block);
			}
		}

		/* and now we handle the exception; no space below or to top */
		if (top == 0 && down == 0) {
			if (dir1 == UI_DIR_LEFT || dir1 == UI_DIR_RIGHT) {
				/* align with bottom of screen */
				// yof = ysize; (not with menu scrolls)
			}
		}

#if 0 /* seems redundant and causes issues with blocks inside big regions */
		/* or no space left or right */
		if (left == 0 && right == 0) {
			if (dir1 == UI_DIR_UP || dir1 == UI_DIR_DOWN) {
				/* align with left size of screen */
				xof = -block->rect.xmin + 5;
			}
		}
#endif

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
		ui_block_to_window_rctf(butregion, but->block, &bt->rect, &bt->rect);

		BLI_rctf_translate(&bt->rect, xof, yof);

		/* ui_but_update recalculates drawstring size in pixels */
		ui_but_update(bt);
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
			if (dir2 == UI_DIR_RIGHT) block->safety.xmax = block->rect.xmax + 3;
			if (dir2 == UI_DIR_LEFT)  block->safety.xmin = block->rect.xmin - 3;
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

	if (ar->do_draw & RGN_DRAW_REFRESH_UI) {
		uiBlock *block_next;
		ar->do_draw &= ~RGN_DRAW_REFRESH_UI;
		for (block = ar->uiblocks.first; block; block = block_next) {
			block_next = block->next;
			if (block->handle->can_refresh) {
				ui_popup_block_refresh((bContext *)C, block->handle, NULL, NULL);
			}
		}
	}

	for (block = ar->uiblocks.first; block; block = block->next)
		UI_block_draw(C, block);
}

/**
 * Use to refresh centered popups on screen resizing (for splash).
 */
static void ui_block_region_popup_window_listener(
        bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	switch (wmn->category) {
		case NC_WINDOW:
		{
			switch (wmn->action) {
				case NA_EDITED:
				{
					/* window resize */
					ED_region_tag_refresh_ui(ar);
					break;
				}
			}
			break;
		}
	}
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
	if (block->rect.ymax > winy - UI_POPUP_MENU_TOP)
		block->rect.ymax = winy - UI_POPUP_MENU_TOP;
	
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

static void ui_popup_block_remove(bContext *C, uiPopupBlockHandle *handle)
{
	wmWindow *win = CTX_wm_window(C);
	bScreen *sc = CTX_wm_screen(C);

	ui_region_temp_remove(C, sc, handle->region);

	/* reset to region cursor (only if there's not another menu open) */
	if (BLI_listbase_is_empty(&sc->regionbase)) {
		ED_region_cursor_set(win, CTX_wm_area(C), CTX_wm_region(C));
		/* in case cursor needs to be changed again */
		WM_event_add_mousemove(C);
	}

	if (handle->scrolltimer)
		WM_event_remove_timer(CTX_wm_manager(C), win, handle->scrolltimer);
}

/**
 * Called for creating new popups and refreshing existing ones.
 */
uiBlock *ui_popup_block_refresh(
        bContext *C, uiPopupBlockHandle *handle,
        ARegion *butregion, uiBut *but)
{
	BLI_assert(handle->can_refresh == true);

	const int margin = UI_POPUP_MARGIN;
	wmWindow *window = CTX_wm_window(C);
	ARegion *ar = handle->region;

	uiBlockCreateFunc create_func = handle->popup_create_vars.create_func;
	uiBlockHandleCreateFunc handle_create_func = handle->popup_create_vars.handle_create_func;
	void *arg = handle->popup_create_vars.arg;

	uiBlock *block_old = ar->uiblocks.first;
	uiBlock *block;

#ifdef DEBUG
	wmEvent *event_back = window->eventstate;
#endif

	/* create ui block */
	if (create_func)
		block = create_func(C, ar, arg);
	else
		block = handle_create_func(C, handle, arg);
	
	/* callbacks _must_ leave this for us, otherwise we can't call UI_block_update_from_old */
	BLI_assert(!block->endblock);

	/* ensure we don't use mouse coords here! */
#ifdef DEBUG
	window->eventstate = NULL;
#endif

	if (block->handle) {
		memcpy(block->handle, handle, sizeof(uiPopupBlockHandle));
		MEM_freeN(handle);
		handle = block->handle;
	}
	else
		block->handle = handle;

	ar->regiondata = handle;

	/* set UI_BLOCK_NUMSELECT before UI_block_end() so we get alphanumeric keys assigned */
	if (but == NULL) {
		block->flag |= UI_BLOCK_POPUP;
	}

	block->flag |= UI_BLOCK_LOOP;

	/* defer this until blocks are translated (below) */
	block->oldblock = NULL;

	if (!block->endblock)
		UI_block_end_ex(C, block, handle->popup_create_vars.event_xy);

	/* if this is being created from a button */
	if (but) {
		block->aspect = but->block->aspect;
		ui_block_position(window, butregion, but, block);
		handle->direction = block->direction;
	}
	else {
		uiSafetyRct *saferct;
		/* keep a list of these, needed for pulldown menus */
		saferct = MEM_callocN(sizeof(uiSafetyRct), "uiSafetyRct");
		saferct->safety = block->safety;
		BLI_addhead(&block->saferct, saferct);
	}

	if (block->flag & UI_BLOCK_RADIAL) {
		int win_width = UI_SCREEN_MARGIN;
		int winx, winy;

		int x_offset = 0, y_offset = 0;

		winx = WM_window_pixels_x(window);
		winy = WM_window_pixels_y(window);

		copy_v2_v2(block->pie_data.pie_center_init, block->pie_data.pie_center_spawned);

		/* only try translation if area is large enough */
		if (BLI_rctf_size_x(&block->rect) < winx - (2.0f * win_width)) {
			if (block->rect.xmin < win_width )   x_offset += win_width - block->rect.xmin;
			if (block->rect.xmax > winx - win_width) x_offset += winx - win_width - block->rect.xmax;
		}

		if (BLI_rctf_size_y(&block->rect) < winy - (2.0f * win_width)) {
			if (block->rect.ymin < win_width )   y_offset += win_width - block->rect.ymin;
			if (block->rect.ymax > winy - win_width) y_offset += winy - win_width - block->rect.ymax;
		}
		/* if we are offsetting set up initial data for timeout functionality */

		if ((x_offset != 0) || (y_offset != 0)) {
			block->pie_data.pie_center_spawned[0] += x_offset;
			block->pie_data.pie_center_spawned[1] += y_offset;

			ui_block_translate(block, x_offset, y_offset);

			if (U.pie_initial_timeout > 0)
				block->pie_data.flags |= UI_PIE_INITIAL_DIRECTION;
		}

		ar->winrct.xmin = 0;
		ar->winrct.xmax = winx;
		ar->winrct.ymin = 0;
		ar->winrct.ymax = winy;

		ui_block_calc_pie_segment(block, block->pie_data.pie_center_init);

		/* lastly set the buttons at the center of the pie menu, ready for animation */
		if (U.pie_animation_timeout > 0) {
			for (uiBut *but_iter = block->buttons.first; but_iter; but_iter = but_iter->next) {
				if (but_iter->pie_dir != UI_RADIAL_NONE) {
					BLI_rctf_recenter(&but_iter->rect, UNPACK2(block->pie_data.pie_center_spawned));
				}
			}
		}
	}
	else {
		/* clip block with window boundary */
		ui_popup_block_clip(window, block);
		/* the block and buttons were positioned in window space as in 2.4x, now
		 * these menu blocks are regions so we bring it back to region space.
		 * additionally we add some padding for the menu shadow or rounded menus */
		ar->winrct.xmin = block->rect.xmin - margin;
		ar->winrct.xmax = block->rect.xmax + margin;
		ar->winrct.ymin = block->rect.ymin - margin;
		ar->winrct.ymax = block->rect.ymax + UI_POPUP_MENU_TOP;

		ui_block_translate(block, -ar->winrct.xmin, -ar->winrct.ymin);
	}

	if (block_old) {
		block->oldblock = block_old;
		UI_block_update_from_old(C, block);
		UI_blocklist_free_inactive(C, &ar->uiblocks);
	}

	/* checks which buttons are visible, sets flags to prevent draw (do after region init) */
	ui_popup_block_scrolltest(block);
	
	/* adds subwindow */
	ED_region_init(C, ar);

	/* get winmat now that we actually have the subwindow */
	wmSubWindowSet(window, ar->swinid);
	
	wm_subwindow_matrix_get(window, ar->swinid, block->winmat);
	
	/* notify change and redraw */
	ED_region_tag_redraw(ar);

	ED_region_update_rect(C, ar);

#ifdef DEBUG
	window->eventstate = event_back;
#endif

	return block;
}

uiPopupBlockHandle *ui_popup_block_create(
        bContext *C, ARegion *butregion, uiBut *but,
        uiBlockCreateFunc create_func, uiBlockHandleCreateFunc handle_create_func,
        void *arg)
{
	wmWindow *window = CTX_wm_window(C);
	uiBut *activebut = UI_context_active_but_get(C);
	static ARegionType type;
	ARegion *ar;
	uiBlock *block;
	uiPopupBlockHandle *handle;

	/* disable tooltips from buttons below */
	if (activebut) {
		UI_but_tooltip_timer_remove(C, activebut);
	}
	/* standard cursor by default */
	WM_cursor_set(window, CURSOR_STD);

	/* create handle */
	handle = MEM_callocN(sizeof(uiPopupBlockHandle), "uiPopupBlockHandle");

	/* store context for operator */
	handle->ctx_area = CTX_wm_area(C);
	handle->ctx_region = CTX_wm_region(C);

	/* store vars to refresh popup (RGN_DRAW_REFRESH_UI) */
	handle->popup_create_vars.create_func = create_func;
	handle->popup_create_vars.handle_create_func = handle_create_func;
	handle->popup_create_vars.arg = arg;
	handle->popup_create_vars.butregion = but ? butregion : NULL;
	copy_v2_v2_int(handle->popup_create_vars.event_xy, &window->eventstate->x);
	/* caller may free vars used to create this popup, in that case this variable should be disabled. */
	handle->can_refresh = true;

	/* create area region */
	ar = ui_region_temp_add(CTX_wm_screen(C));
	handle->region = ar;

	memset(&type, 0, sizeof(ARegionType));
	type.draw = ui_block_region_draw;
	type.regionid = RGN_TYPE_TEMPORARY;
	ar->type = &type;

	UI_region_handlers_add(&ar->handlers);

	block = ui_popup_block_refresh(C, handle, butregion, but);
	handle = block->handle;

	/* keep centered on window resizing */
	if ((block->bounds_type == UI_BLOCK_BOUNDS_POPUP_CENTER) && handle->can_refresh) {
		type.listener = ui_block_region_popup_window_listener;
	}

	return handle;
}

void ui_popup_block_free(bContext *C, uiPopupBlockHandle *handle)
{
	ui_popup_block_remove(C, handle);
	
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
void ui_but_hsv_set(uiBut *but)
{
	float col[3];
	ColorPicker *cpicker = but->custom_data;
	float *hsv = cpicker->color_data;

	ui_color_picker_to_rgb_v(hsv, col);

	ui_but_v3_set(but, col);
}

/* Updates all buttons who share the same color picker as the one passed
 * also used by small picker, be careful with name checks below... */
static void ui_update_color_picker_buts_rgb(uiBlock *block, ColorPicker *cpicker, const float rgb[3], bool is_display_space)
{
	uiBut *bt;
	float *hsv = cpicker->color_data;
	struct ColorManagedDisplay *display = NULL;
	/* this is to keep the H and S value when V is equal to zero
	 * and we are working in HSV mode, of course!
	 */
	if (is_display_space) {
		ui_rgb_to_color_picker_compat_v(rgb, hsv);
	}
	else {
		/* we need to convert to display space to use hsv, because hsv is stored in display space */
		float rgb_display[3];

		copy_v3_v3(rgb_display, rgb);
		ui_block_cm_to_display_space_v3(block, rgb_display);
		ui_rgb_to_color_picker_compat_v(rgb_display, hsv);
	}

	if (block->color_profile)
		display = ui_block_cm_display_get(block);
	
	/* this updates button strings, is hackish... but button pointers are on stack of caller function */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		if (bt->custom_data != cpicker)
			continue;

		if (bt->rnaprop) {
			ui_but_v3_set(bt, rgb);
			
			/* original button that created the color picker already does undo
			 * push, so disable it on RNA buttons in the color picker block */
			UI_but_flag_disable(bt, UI_BUT_UNDO);
		}
		else if (STREQ(bt->str, "Hex: ")) {
			float rgb_gamma[3];
			unsigned char rgb_gamma_uchar[3];
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

			rgb_float_to_uchar(rgb_gamma_uchar, rgb_gamma);
			BLI_snprintf(col, sizeof(col), "%02X%02X%02X", UNPACK3_EX((unsigned int), rgb_gamma_uchar, ));
			
			strcpy(bt->poin, col);
		}
		else if (bt->str[1] == ' ') {
			if (bt->str[0] == 'R') {
				ui_but_value_set(bt, rgb[0]);
			}
			else if (bt->str[0] == 'G') {
				ui_but_value_set(bt, rgb[1]);
			}
			else if (bt->str[0] == 'B') {
				ui_but_value_set(bt, rgb[2]);
			}
			else if (bt->str[0] == 'H') {
				ui_but_value_set(bt, hsv[0]);
			}
			else if (bt->str[0] == 'S') {
				ui_but_value_set(bt, hsv[1]);
			}
			else if (bt->str[0] == 'V') {
				ui_but_value_set(bt, hsv[2]);
			}
			else if (bt->str[0] == 'L') {
				ui_but_value_set(bt, hsv[2]);
			}
		}

		ui_but_update(bt);
	}
}

static void ui_colorpicker_rna_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
	uiBut *but = (uiBut *)bt1;
	uiPopupBlockHandle *popup = but->block->handle;
	PropertyRNA *prop = but->rnaprop;
	PointerRNA ptr = but->rnapoin;
	float rgb[4];
	
	if (prop) {
		RNA_property_float_get_array(&ptr, prop, rgb);
		ui_update_color_picker_buts_rgb(but->block, but->custom_data, rgb, (RNA_property_subtype(prop) == PROP_COLOR_GAMMA));
	}
	
	if (popup)
		popup->menuretval = UI_RETURN_UPDATE;
}

static void ui_color_wheel_rna_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
	uiBut *but = (uiBut *)bt1;
	uiPopupBlockHandle *popup = but->block->handle;
	float rgb[3];
	ColorPicker *cpicker = but->custom_data;
	float *hsv = cpicker->color_data;
	bool use_display_colorspace = ui_but_is_colorpicker_display_space(but);

	ui_color_picker_to_rgb_v(hsv, rgb);

	/* hsv is saved in display space so convert back */
	if (use_display_colorspace) {
		ui_block_cm_to_scene_linear_v3(but->block, rgb);
	}

	ui_update_color_picker_buts_rgb(but->block, cpicker, rgb, !use_display_colorspace);
	
	if (popup)
		popup->menuretval = UI_RETURN_UPDATE;
}

static void ui_colorpicker_hex_rna_cb(bContext *UNUSED(C), void *bt1, void *hexcl)
{
	uiBut *but = (uiBut *)bt1;
	uiPopupBlockHandle *popup = but->block->handle;
	ColorPicker *cpicker = but->custom_data;
	char *hexcol = (char *)hexcl;
	float rgb[3];
	
	hex_to_rgb(hexcol, rgb, rgb + 1, rgb + 2);
	
	/* Hex code is assumed to be in sRGB space (coming from other applications, web, etc) */
	if (but->block->color_profile) {
		/* so we need to linearise it for Blender */
		ui_block_cm_to_scene_linear_v3(but->block, rgb);
	}
	
	ui_update_color_picker_buts_rgb(but->block, cpicker, rgb, false);
	
	if (popup)
		popup->menuretval = UI_RETURN_UPDATE;
}

static void ui_popup_close_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
	uiBut *but = (uiBut *)bt1;
	uiPopupBlockHandle *popup = but->block->handle;
	
	if (popup)
		popup->menuretval = UI_RETURN_OK;
}

static void ui_colorpicker_hide_reveal(uiBlock *block, short colormode)
{
	uiBut *bt;
	
	/* tag buttons */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		if ((bt->func == ui_colorpicker_rna_cb) && bt->type == UI_BTYPE_NUM_SLIDER && bt->rnaindex != 3) {
			/* RGB sliders (color circle and alpha are always shown) */
			if (colormode == 0) bt->flag &= ~UI_HIDDEN;
			else bt->flag |= UI_HIDDEN;
		}
		else if (bt->func == ui_color_wheel_rna_cb) {
			/* HSV sliders */
			if (colormode == 1) bt->flag &= ~UI_HIDDEN;
			else bt->flag |= UI_HIDDEN;
		}
		else if (bt->func == ui_colorpicker_hex_rna_cb || bt->type == UI_BTYPE_LABEL) {
			/* hex input or gamma correction status label */
			if (colormode == 2) bt->flag &= ~UI_HIDDEN;
			else bt->flag |= UI_HIDDEN;
		}
	}
}

static void ui_colorpicker_create_mode_cb(bContext *UNUSED(C), void *bt1, void *UNUSED(arg))
{
	uiBut *bt = bt1;
	short colormode = ui_but_value_get(bt);
	ui_colorpicker_hide_reveal(bt->block, colormode);
}

#define PICKER_H    (7.5f * U.widget_unit)
#define PICKER_W    (7.5f * U.widget_unit)
#define PICKER_SPACE    (0.3f * U.widget_unit)
#define PICKER_BAR      (0.7f * U.widget_unit)

#define PICKER_TOTAL_W  (PICKER_W + PICKER_SPACE + PICKER_BAR)

static void ui_colorpicker_circle(uiBlock *block, PointerRNA *ptr, PropertyRNA *prop, ColorPicker *cpicker)
{
	uiBut *bt;
	
	/* HS circle */
	bt = uiDefButR_prop(block, UI_BTYPE_HSVCIRCLE, 0, "", 0, 0, PICKER_H, PICKER_W, ptr, prop, -1, 0.0, 0.0, 0.0, 0, TIP_("Color"));
	UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
	bt->custom_data = cpicker;

	/* value */
	if (U.color_picker_type == USER_CP_CIRCLE_HSL) {
		bt = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", PICKER_W + PICKER_SPACE, 0, PICKER_BAR, PICKER_H, ptr, prop, -1, 0.0, 0.0, UI_GRAD_L_ALT, 0, "Lightness");
		UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
	}
	else {
		bt = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "", PICKER_W + PICKER_SPACE, 0, PICKER_BAR, PICKER_H, ptr, prop, -1, 0.0, 0.0, UI_GRAD_V_ALT, 0, TIP_("Value"));
		UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
	}
	bt->custom_data = cpicker;
}


static void ui_colorpicker_square(uiBlock *block, PointerRNA *ptr, PropertyRNA *prop, int type, ColorPicker *cpicker)
{
	uiBut *bt;
	int bartype = type + 3;
	
	/* HS square */
	bt = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "",   0, PICKER_BAR + PICKER_SPACE, PICKER_TOTAL_W, PICKER_H, ptr, prop, -1, 0.0, 0.0, type, 0, TIP_("Color"));
	UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
	bt->custom_data = cpicker;
	
	/* value */
	bt = uiDefButR_prop(block, UI_BTYPE_HSVCUBE, 0, "",       0, 0, PICKER_TOTAL_W, PICKER_BAR, ptr, prop, -1, 0.0, 0.0, bartype, 0, TIP_("Value"));
	UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
	bt->custom_data = cpicker;
}


/* a HS circle, V slider, rgb/hsv/hex sliders */
static void ui_block_colorpicker(uiBlock *block, float rgba[4], PointerRNA *ptr, PropertyRNA *prop, bool show_picker)
{
	static short colormode = 0;  /* temp? 0=rgb, 1=hsv, 2=hex */
	uiBut *bt;
	int width, butwidth;
	static char tip[50];
	static char hexcol[128];
	float rgb_gamma[3];
	unsigned char rgb_gamma_uchar[3];
	float softmin, softmax, hardmin, hardmax, step, precision;
	int yco;
	ColorPicker *cpicker = ui_block_colorpicker_create(block);
	float *hsv = cpicker->color_data;
		
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
		ui_block_cm_to_display_space_v3(block, rgb_gamma);
	}
	
	/* sneaky way to check for alpha */
	rgba[3] = FLT_MAX;

	RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);
	RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
	RNA_property_float_get_array(ptr, prop, rgba);

	/* when the softmax isn't defined in the RNA,
	 * using very large numbers causes sRGB/linear round trip to fail. */
	if (softmax == FLT_MAX) {
		softmax = 1.0f;
	}

	switch (U.color_picker_type) {
		case USER_CP_SQUARE_SV:
			ui_colorpicker_square(block, ptr, prop, UI_GRAD_SV, cpicker);
			break;
		case USER_CP_SQUARE_HS:
			ui_colorpicker_square(block, ptr, prop, UI_GRAD_HS, cpicker);
			break;
		case USER_CP_SQUARE_HV:
			ui_colorpicker_square(block, ptr, prop, UI_GRAD_HV, cpicker);
			break;

		/* user default */
		case USER_CP_CIRCLE_HSV:
		case USER_CP_CIRCLE_HSL:
		default:
			ui_colorpicker_circle(block, ptr, prop, cpicker);
			break;
	}
	
	/* mode */
	yco = -1.5f * UI_UNIT_Y;
	UI_block_align_begin(block);
	bt = uiDefButS(block, UI_BTYPE_ROW, 0, IFACE_("RGB"), 0, yco, width / 3, UI_UNIT_Y, &colormode, 0.0, 0.0, 0, 0, "");
	UI_but_func_set(bt, ui_colorpicker_create_mode_cb, bt, NULL);
	bt->custom_data = cpicker;
	if (U.color_picker_type == USER_CP_CIRCLE_HSL)
		bt = uiDefButS(block, UI_BTYPE_ROW, 0, IFACE_("HSL"), width / 3, yco, width / 3, UI_UNIT_Y, &colormode, 0.0, 1.0, 0, 0, "");
	else
		bt = uiDefButS(block, UI_BTYPE_ROW, 0, IFACE_("HSV"), width / 3, yco, width / 3, UI_UNIT_Y, &colormode, 0.0, 1.0, 0, 0, "");
	UI_but_func_set(bt, ui_colorpicker_create_mode_cb, bt, NULL);
	bt->custom_data = cpicker;
	bt = uiDefButS(block, UI_BTYPE_ROW, 0, IFACE_("Hex"), 2 * width / 3, yco, width / 3, UI_UNIT_Y, &colormode, 0.0, 2.0, 0, 0, "");
	UI_but_func_set(bt, ui_colorpicker_create_mode_cb, bt, NULL);
	bt->custom_data = cpicker;
	UI_block_align_end(block);

	yco = -3.0f * UI_UNIT_Y;
	if (show_picker) {
		bt = uiDefIconButO(block, UI_BTYPE_BUT, "UI_OT_eyedropper_color", WM_OP_INVOKE_DEFAULT, ICON_EYEDROPPER, butwidth + 10, yco, UI_UNIT_X, UI_UNIT_Y, NULL);
		UI_but_func_set(bt, ui_popup_close_cb, bt, NULL);
		bt->custom_data = cpicker;
	}
	
	/* RGB values */
	UI_block_align_begin(block);
	bt = uiDefButR_prop(block, UI_BTYPE_NUM_SLIDER, 0, IFACE_("R:"),  0, yco, butwidth, UI_UNIT_Y, ptr, prop, 0, 0.0, 0.0, 0, 3, TIP_("Red"));
	UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
	bt->custom_data = cpicker;
	bt = uiDefButR_prop(block, UI_BTYPE_NUM_SLIDER, 0, IFACE_("G:"),  0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, ptr, prop, 1, 0.0, 0.0, 0, 3, TIP_("Green"));
	UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
	bt->custom_data = cpicker;
	bt = uiDefButR_prop(block, UI_BTYPE_NUM_SLIDER, 0, IFACE_("B:"),  0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, ptr, prop, 2, 0.0, 0.0, 0, 3, TIP_("Blue"));
	UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
	bt->custom_data = cpicker;

	/* could use uiItemFullR(col, ptr, prop, -1, 0, UI_ITEM_R_EXPAND|UI_ITEM_R_SLIDER, "", ICON_NONE);
	 * but need to use UI_but_func_set for updating other fake buttons */
	
	/* HSV values */
	yco = -3.0f * UI_UNIT_Y;
	UI_block_align_begin(block);
	bt = uiDefButF(block, UI_BTYPE_NUM_SLIDER, 0, IFACE_("H:"),   0, yco, butwidth, UI_UNIT_Y, hsv, 0.0, 1.0, 10, 3, TIP_("Hue"));
	UI_but_func_set(bt, ui_color_wheel_rna_cb, bt, hsv);
	bt->custom_data = cpicker;
	bt = uiDefButF(block, UI_BTYPE_NUM_SLIDER, 0, IFACE_("S:"),   0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, hsv + 1, 0.0, 1.0, 10, 3, TIP_("Saturation"));
	UI_but_func_set(bt, ui_color_wheel_rna_cb, bt, hsv);
	bt->custom_data = cpicker;
	if (U.color_picker_type == USER_CP_CIRCLE_HSL)
		bt = uiDefButF(block, UI_BTYPE_NUM_SLIDER, 0, IFACE_("L:"),   0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, hsv + 2, 0.0, 1.0, 10, 3, TIP_("Lightness"));
	else
		bt = uiDefButF(block, UI_BTYPE_NUM_SLIDER, 0, IFACE_("V:"),   0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, hsv + 2, 0.0, softmax, 10, 3, TIP_("Value"));

	bt->hardmax = hardmax;  /* not common but rgb  may be over 1.0 */
	UI_but_func_set(bt, ui_color_wheel_rna_cb, bt, hsv);
	bt->custom_data = cpicker;

	UI_block_align_end(block);

	if (rgba[3] != FLT_MAX) {
		bt = uiDefButR_prop(block, UI_BTYPE_NUM_SLIDER, 0, IFACE_("A: "),  0, yco -= UI_UNIT_Y, butwidth, UI_UNIT_Y, ptr, prop, 3, 0.0, 0.0, 0, 3, TIP_("Alpha"));
		UI_but_func_set(bt, ui_colorpicker_rna_cb, bt, NULL);
		bt->custom_data = cpicker;
	}
	else {
		rgba[3] = 1.0f;
	}

	rgb_float_to_uchar(rgb_gamma_uchar, rgb_gamma);
	BLI_snprintf(hexcol, sizeof(hexcol), "%02X%02X%02X", UNPACK3_EX((unsigned int), rgb_gamma_uchar, ));

	yco = -3.0f * UI_UNIT_Y;
	bt = uiDefBut(block, UI_BTYPE_TEXT, 0, IFACE_("Hex: "), 0, yco, butwidth, UI_UNIT_Y, hexcol, 0, 8, 0, 0, TIP_("Hex triplet for color (#RRGGBB)"));
	UI_but_func_set(bt, ui_colorpicker_hex_rna_cb, bt, hexcol);
	bt->custom_data = cpicker;
	uiDefBut(block, UI_BTYPE_LABEL, 0, IFACE_("(Gamma Corrected)"), 0, yco - UI_UNIT_Y, butwidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");

	ui_rgb_to_color_picker_v(rgb_gamma, hsv);

	ui_colorpicker_hide_reveal(block, colormode);
}


static int ui_colorpicker_small_wheel_cb(const bContext *UNUSED(C), uiBlock *block, const wmEvent *event)
{
	float add = 0.0f;
	
	if (event->type == WHEELUPMOUSE)
		add = 0.05f;
	else if (event->type == WHEELDOWNMOUSE)
		add = -0.05f;
	
	if (add != 0.0f) {
		uiBut *but;
		
		for (but = block->buttons.first; but; but = but->next) {
			if (but->type == UI_BTYPE_HSVCUBE && but->active == NULL) {
				uiPopupBlockHandle *popup = block->handle;
				float rgb[3];
				ColorPicker *cpicker = but->custom_data;
				float *hsv = cpicker->color_data;
				bool use_display_colorspace = ui_but_is_colorpicker_display_space(but);
				
				ui_but_v3_get(but, rgb);

				if (use_display_colorspace)
					ui_block_cm_to_display_space_v3(block, rgb);

				ui_rgb_to_color_picker_compat_v(rgb, hsv);

				hsv[2] = CLAMPIS(hsv[2] + add, 0.0f, 1.0f);
				ui_color_picker_to_rgb_v(hsv, rgb);

				if (use_display_colorspace)
					ui_block_cm_to_scene_linear_v3(block, rgb);

				ui_but_v3_set(but, rgb);
				
				ui_update_color_picker_buts_rgb(block, cpicker, rgb, !use_display_colorspace);
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
	
	block = UI_block_begin(C, handle->region, __func__, UI_EMBOSS);
	
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
	
	ui_block_colorpicker(block, handle->retvec, &but->rnapoin, but->rnaprop, show_picker);
	
	block->flag = UI_BLOCK_LOOP | UI_BLOCK_KEEP_OPEN | UI_BLOCK_OUT_1 | UI_BLOCK_MOVEMOUSE_QUIT;
	UI_block_bounds_set_normal(block, 0.5 * UI_UNIT_X);
	
	block->block_event_func = ui_colorpicker_small_wheel_cb;
	
	/* and lets go */
	block->direction = UI_DIR_UP;
	
	return block;
}

/************************ Popup Menu Memory ****************************/

static unsigned int ui_popup_string_hash(const char *str)
{
	/* sometimes button contains hotkey, sometimes not, strip for proper compare */
	int hash;
	const char *delimit = strrchr(str, UI_SEP_CHAR);

	if (delimit) {
		hash = BLI_ghashutil_strhash_n(str, delimit - str);
	}
	else {
		hash = BLI_ghashutil_strhash(str);
	}

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

/**
 * Translate any popup regions (so we can drag them).
 */
void ui_popup_translate(bContext *C, ARegion *ar, const int mdiff[2])
{
	uiBlock *block;

	BLI_rcti_translate(&ar->winrct, UNPACK2(mdiff));

	ED_region_update_rect(C, ar);

	ED_region_tag_redraw(ar);

	/* update blocks */
	for (block = ar->uiblocks.first; block; block = block->next) {
		uiSafetyRct *saferct;
		for (saferct = block->saferct.first; saferct; saferct = saferct->next) {
			BLI_rctf_translate(&saferct->parent, UNPACK2(mdiff));
			BLI_rctf_translate(&saferct->safety, UNPACK2(mdiff));
		}
	}
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

struct uiPieMenu {
	uiBlock *block_radial; /* radial block of the pie menu (more could be added later) */
	uiLayout *layout;
	int mx, my;
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
		else if ((pup->but->type == UI_BTYPE_PULLDOWN) ||
		         (UI_but_menutype_get(pup->but) != NULL))
		{
			direction = UI_DIR_DOWN;
		}
		else {
			direction = UI_DIR_UP;
		}
	}
	else {
		minwidth = 50;
		direction = UI_DIR_DOWN;
	}

	flip = (direction == UI_DIR_DOWN);

	block = pup->block;
	
	/* in some cases we create the block before the region,
	 * so we set it delayed here if necessary */
	if (BLI_findindex(&handle->region->uiblocks, block) == -1)
		UI_block_region_set(block, handle->region);

	block->direction = direction;

	UI_block_layout_resolve(block, &width, &height);

	UI_block_flag_enable(block, UI_BLOCK_MOVEMOUSE_QUIT);
	
	if (pup->popup) {
		uiBut *but_activate = NULL;
		UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_NUMSELECT);
		UI_block_direction_set(block, direction);

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

			if (ui_but_is_editable(bt)) {
				but_activate = bt;
			}
		}
		else {
			/* position mouse at 0.8*width of the button and below the tile
			 * on the first item */
			offset[0] = 0;
			for (bt = block->buttons.first; bt; bt = bt->next)
				offset[0] = min_ii(offset[0], -(bt->rect.xmin + 0.8f * BLI_rctf_size_x(&bt->rect)));

			offset[1] = 2.1 * UI_UNIT_Y;

			for (bt = block->buttons.first; bt; bt = bt->next) {
				if (ui_but_is_editable(bt)) {
					but_activate = bt;
					break;
				}
			}
		}

		/* in rare cases this is needed since moving the popup
		 * to be within the window bounds may move it away from the mouse,
		 * This ensures we set an item to be active. */
		if (but_activate) {
			ui_but_activate_over(C, handle->region, but_activate);
		}

		block->minbounds = minwidth;
		UI_block_bounds_set_menu(block, 1, offset[0], offset[1]);
	}
	else {
		/* for a header menu we set the direction automatic */
		if (!pup->slideout && flip) {
			ScrArea *sa = CTX_wm_area(C);
			if (sa && sa->headertype == HEADERDOWN) {
				ARegion *ar = CTX_wm_region(C);
				if (ar && ar->regiontype == RGN_TYPE_HEADER) {
					UI_block_direction_set(block, UI_DIR_UP);
					UI_block_order_flip(block);
				}
			}
		}

		block->minbounds = minwidth;
		UI_block_bounds_set_text(block, 3.0f * UI_UNIT_X);
	}

	/* if menu slides out of other menu, override direction */
	if (pup->slideout)
		UI_block_direction_set(block, UI_DIR_RIGHT);

	return pup->block;
}

uiPopupBlockHandle *ui_popup_menu_create(
        bContext *C, ARegion *butregion, uiBut *but,
        uiMenuCreateFunc menu_func, void *arg)
{
	wmWindow *window = CTX_wm_window(C);
	uiStyle *style = UI_style_get_dpi();
	uiPopupBlockHandle *handle;
	uiPopupMenu *pup;

	pup = MEM_callocN(sizeof(uiPopupMenu), __func__);
	pup->block = UI_block_begin(C, NULL, __func__, UI_EMBOSS_PULLDOWN);
	pup->block->flag |= UI_BLOCK_NUMSELECT;  /* default menus to numselect */
	pup->layout = UI_block_layout(pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, 200, 0, MENU_PADDING, style);
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

		UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
		WM_event_add_mousemove(C);
	}
	
	handle->can_refresh = false;
	MEM_freeN(pup);

	return handle;
}

/******************** Popup Menu API with begin and end ***********************/

/**
 * Only return handler, and set optional title.
 * \param block_name: Assigned to uiBlock.name (useful info for debugging).
 */
uiPopupMenu *UI_popup_menu_begin_ex(bContext *C, const char *title, const char *block_name, int icon)
{
	uiStyle *style = UI_style_get_dpi();
	uiPopupMenu *pup = MEM_callocN(sizeof(uiPopupMenu), "popup menu");
	uiBut *but;

	pup->block = UI_block_begin(C, NULL, block_name, UI_EMBOSS_PULLDOWN);
	pup->block->flag |= UI_BLOCK_POPUP_MEMORY | UI_BLOCK_IS_FLIP;
	pup->block->puphash = ui_popup_menu_hash(title);
	pup->layout = UI_block_layout(pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, 200, 0, MENU_PADDING, style);

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
			uiDefIconTextBut(pup->block, UI_BTYPE_LABEL, 0, icon, titlestr, 0, 0, 200, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
		}
		else {
			but = uiDefBut(pup->block, UI_BTYPE_LABEL, 0, title, 0, 0, 200, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
			but->drawflag = UI_BUT_TEXT_LEFT;
		}

		uiItemS(pup->layout);
	}

	return pup;
}

uiPopupMenu *UI_popup_menu_begin(bContext *C, const char *title, int icon)
{
	return UI_popup_menu_begin_ex(C, title, __func__, icon);
}

/* set the whole structure to work */
void UI_popup_menu_end(bContext *C, uiPopupMenu *pup)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *menu;
	
	pup->popup = true;
	pup->mx = window->eventstate->x;
	pup->my = window->eventstate->y;
	
	menu = ui_popup_block_create(C, NULL, NULL, NULL, ui_block_func_POPUP, pup);
	menu->popup = true;
	
	UI_popup_handlers_add(C, &window->modalhandlers, menu, 0);
	WM_event_add_mousemove(C);

	menu->can_refresh = false;
	MEM_freeN(pup);
}

uiLayout *UI_popup_menu_layout(uiPopupMenu *pup)
{
	return pup->layout;
}

/*************************** Pie Menus ***************************************/

static uiBlock *ui_block_func_PIE(bContext *UNUSED(C), uiPopupBlockHandle *handle, void *arg_pie)
{
	uiBlock *block;
	uiPieMenu *pie = arg_pie;
	int minwidth, width, height;

	minwidth = 50;
	block = pie->block_radial;

	/* in some cases we create the block before the region,
	 * so we set it delayed here if necessary */
	if (BLI_findindex(&handle->region->uiblocks, block) == -1)
		UI_block_region_set(block, handle->region);

	UI_block_layout_resolve(block, &width, &height);

	UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_NUMSELECT);

	block->minbounds = minwidth;
	block->bounds = 1;
	block->mx = 0;
	block->my = 0;
	block->bounds_type = UI_BLOCK_BOUNDS_PIE_CENTER;

	block->pie_data.pie_center_spawned[0] = pie->mx;
	block->pie_data.pie_center_spawned[1] = pie->my;

	return pie->block_radial;
}

static float ui_pie_menu_title_width(const char *name, int icon)
{
	const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
	return (UI_fontstyle_string_width(fstyle, name) +
	         (UI_UNIT_X * (1.50f + (icon ? 0.25f : 0.0f))));
}

uiPieMenu *UI_pie_menu_begin(struct bContext *C, const char *title, int icon, const wmEvent *event)
{
	uiStyle *style;
	uiPieMenu *pie;
	short event_type;

	wmWindow *win = CTX_wm_window(C);

	style = UI_style_get_dpi();
	pie = MEM_callocN(sizeof(*pie), "pie menu");

	pie->block_radial = UI_block_begin(C, NULL, __func__, UI_EMBOSS);
	/* may be useful later to allow spawning pies
	 * from old positions */
	/* pie->block_radial->flag |= UI_BLOCK_POPUP_MEMORY; */
	pie->block_radial->puphash = ui_popup_menu_hash(title);
	pie->block_radial->flag |= UI_BLOCK_RADIAL;

	/* if pie is spawned by a left click, release or click event, it is always assumed to be click style */
	if (event->type == LEFTMOUSE || ELEM(event->val, KM_RELEASE, KM_CLICK)) {
		pie->block_radial->pie_data.flags |= UI_PIE_CLICK_STYLE;
		pie->block_radial->pie_data.event = EVENT_NONE;
		win->lock_pie_event = EVENT_NONE;
	}
	else {
		if (win->last_pie_event != EVENT_NONE) {
			/* original pie key has been released, so don't propagate the event */
			if (win->lock_pie_event == EVENT_NONE) {
				event_type = EVENT_NONE;
				pie->block_radial->pie_data.flags |= UI_PIE_CLICK_STYLE;
			}
			else
				event_type = win->last_pie_event;
		}
		else {
			event_type = event->type;
		}

		pie->block_radial->pie_data.event = event_type;
		win->lock_pie_event = event_type;
	}

	pie->layout = UI_block_layout(pie->block_radial, UI_LAYOUT_VERTICAL, UI_LAYOUT_PIEMENU, 0, 0, 200, 0, 0, style);
	pie->mx = event->x;
	pie->my = event->y;

	/* create title button */
	if (title[0]) {
		uiBut *but;
		char titlestr[256];
		int w;
		if (icon) {
			BLI_snprintf(titlestr, sizeof(titlestr), " %s", title);
			w = ui_pie_menu_title_width(titlestr, icon);
			but = uiDefIconTextBut(pie->block_radial, UI_BTYPE_LABEL, 0, icon, titlestr, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
		}
		else {
			w = ui_pie_menu_title_width(title, 0);
			but = uiDefBut(pie->block_radial, UI_BTYPE_LABEL, 0, title, 0, 0, w, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
		}
		/* do not align left */
		but->drawflag &= ~UI_BUT_TEXT_LEFT;
		pie->block_radial->pie_data.title = but->str;
		pie->block_radial->pie_data.icon = icon;
	}

	return pie;
}

void UI_pie_menu_end(bContext *C, uiPieMenu *pie)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *menu;

	menu = ui_popup_block_create(C, NULL, NULL, NULL, ui_block_func_PIE, pie);
	menu->popup = true;
	menu->towardstime = PIL_check_seconds_timer();

	UI_popup_handlers_add(
	        C, &window->modalhandlers,
	        menu, WM_HANDLER_ACCEPT_DBL_CLICK);
	WM_event_add_mousemove(C);

	menu->can_refresh = false;
	MEM_freeN(pie);
}

uiLayout *UI_pie_menu_layout(uiPieMenu *pie)
{
	return pie->layout;
}

int UI_pie_menu_invoke(struct bContext *C, const char *idname, const wmEvent *event)
{
	uiPieMenu *pie;
	uiLayout *layout;
	Menu menu;
	MenuType *mt = WM_menutype_find(idname, true);

	if (mt == NULL) {
		printf("%s: named menu \"%s\" not found\n", __func__, idname);
		return OPERATOR_CANCELLED;
	}

	if (mt->poll && mt->poll(C, mt) == 0)
		/* cancel but allow event to pass through, just like operators do */
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);

	pie = UI_pie_menu_begin(C, IFACE_(mt->label), ICON_NONE, event);
	layout = UI_pie_menu_layout(pie);

	menu.layout = layout;
	menu.type = mt;

	if (G.debug & G_DEBUG_WM) {
		printf("%s: opening menu \"%s\"\n", __func__, idname);
	}

	mt->draw(C, &menu);

	UI_pie_menu_end(C, pie);

	return OPERATOR_INTERFACE;
}

int UI_pie_menu_invoke_from_operator_enum(
        struct bContext *C, const char *title, const char *opname,
        const char *propname, const wmEvent *event)
{
	uiPieMenu *pie;
	uiLayout *layout;

	pie = UI_pie_menu_begin(C, IFACE_(title), ICON_NONE, event);
	layout = UI_pie_menu_layout(pie);

	layout = uiLayoutRadial(layout);
	uiItemsEnumO(layout, opname, propname);

	UI_pie_menu_end(C, pie);

	return OPERATOR_INTERFACE;
}

int UI_pie_menu_invoke_from_rna_enum(
        struct bContext *C, const char *title, const char *path,
        const wmEvent *event)
{
	PointerRNA ctx_ptr;
	PointerRNA r_ptr;
	PropertyRNA *r_prop;
	uiPieMenu *pie;
	uiLayout *layout;

	RNA_pointer_create(NULL, &RNA_Context, C, &ctx_ptr);

	if (!RNA_path_resolve(&ctx_ptr, path, &r_ptr, &r_prop)) {
		return OPERATOR_CANCELLED;
	}

	/* invalid property, only accept enums */
	if (RNA_property_type(r_prop) != PROP_ENUM) {
		BLI_assert(0);
		return OPERATOR_CANCELLED;
	}

	pie = UI_pie_menu_begin(C, IFACE_(title), ICON_NONE, event);

	layout = UI_pie_menu_layout(pie);

	layout = uiLayoutRadial(layout);
	uiItemFullR(layout, &r_ptr, r_prop, RNA_NO_INDEX, 0, UI_ITEM_R_EXPAND, NULL, 0);

	UI_pie_menu_end(C, pie);

	return OPERATOR_INTERFACE;
}

/**
 * \name Pie Menu Levels
 *
 * Pie menus can't contain more than 8 items (yet). When using #uiItemsFullEnumO, a "More" button is created that calls
 * a new pie menu if the enum has too many items. We call this a new "level".
 * Indirect recursion is used, so that a theoretically unlimited number of items is supported.
 *
 * This is a implementation specifically for operator enums, needed since the object mode pie now has more than 8
 * items. Ideally we'd have some way of handling this for all kinds of pie items, but that's tricky.
 *
 * - Julian (Feb 2016)
 *
 * \{ */

typedef struct PieMenuLevelData {
	char title[UI_MAX_NAME_STR]; /* parent pie title, copied for level */
	int icon; /* parent pie icon, copied for level */
	int totitem; /* total count of *remaining* items */

	/* needed for calling uiItemsFullEnumO_array again for new level */
	wmOperatorType *ot;
	const char *propname;
	IDProperty *properties;
	int context, flag;
} PieMenuLevelData;

/**
 * Invokes a new pie menu for a new level.
 */
static void ui_pie_menu_level_invoke(bContext *C, void *argN, void *arg2)
{
	EnumPropertyItem *item_array = (EnumPropertyItem *)argN;
	PieMenuLevelData *lvl = (PieMenuLevelData *)arg2;
	wmWindow *win = CTX_wm_window(C);

	uiPieMenu *pie = UI_pie_menu_begin(C, IFACE_(lvl->title), lvl->icon, win->eventstate);
	uiLayout *layout = UI_pie_menu_layout(pie);

	layout = uiLayoutRadial(layout);

	PointerRNA ptr;

	WM_operator_properties_create_ptr(&ptr, lvl->ot);
	/* so the context is passed to itemf functions (some need it) */
	WM_operator_properties_sanitize(&ptr, false);
	PropertyRNA *prop = RNA_struct_find_property(&ptr, lvl->propname);

	if (prop) {
		uiItemsFullEnumO_items(
		        layout, lvl->ot, ptr, prop, lvl->properties, lvl->context, lvl->flag,
		        item_array, lvl->totitem);
	}
	else {
		RNA_warning("%s.%s not found", RNA_struct_identifier(ptr.type), lvl->propname);
	}

	UI_pie_menu_end(C, pie);
}

/**
 * Set up data for defining a new pie menu level and add button that invokes it.
 */
void ui_pie_menu_level_create(
        uiBlock *block, wmOperatorType *ot, const char *propname, IDProperty *properties,
        const EnumPropertyItem *items, int totitem, int context, int flag)
{
	const int totitem_parent = PIE_MAX_ITEMS - 1;
	const int totitem_remain = totitem - totitem_parent;
	size_t array_size = sizeof(EnumPropertyItem) * totitem_remain;

	/* used as but->func_argN so freeing is handled elsewhere */
	EnumPropertyItem *remaining = MEM_mallocN(array_size + sizeof(EnumPropertyItem), "pie_level_item_array");
	memcpy(remaining, items + totitem_parent, array_size);
	/* a NULL terminating sentinal element is required */
	memset(&remaining[totitem_remain], 0, sizeof(EnumPropertyItem));


	/* yuk, static... issue is we can't reliably free this without doing dangerous changes */
	static PieMenuLevelData lvl;
	BLI_strncpy(lvl.title, block->pie_data.title, UI_MAX_NAME_STR);
	lvl.totitem    = totitem_remain;
	lvl.ot         = ot;
	lvl.propname   = propname;
	lvl.properties = properties;
	lvl.context    = context;
	lvl.flag       = flag;

	/* add a 'more' menu entry */
	uiBut *but = uiDefIconTextBut(block, UI_BTYPE_BUT, 0, ICON_PLUS, "More", 0, 0, UI_UNIT_X * 3, UI_UNIT_Y, NULL,
	                              0.0f, 0.0f, 0.0f, 0.0f, "Show more items of this menu");
	UI_but_funcN_set(but, ui_pie_menu_level_invoke, remaining, &lvl);
}

/** \} */ /* Pie Menu Levels */


/*************************** Standard Popup Menus ****************************/

void UI_popup_menu_reports(bContext *C, ReportList *reports)
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
			/* popup_menu stuff does just what we need (but pass meaningful block name) */
			pup = UI_popup_menu_begin_ex(C, title, __func__, ICON_NONE);
			layout = UI_popup_menu_layout(pup);
		}
		else {
			uiItemS(layout);
		}

		/* split each newline into a label */
		msg = report->message;
		icon = UI_icon_from_report_type(report->type);
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
		UI_popup_menu_end(C, pup);
	}
}

int UI_popup_menu_invoke(bContext *C, const char *idname, ReportList *reports)
{
	uiPopupMenu *pup;
	uiLayout *layout;
	Menu menu;
	MenuType *mt = WM_menutype_find(idname, true);

	if (mt == NULL) {
		BKE_reportf(reports, RPT_ERROR, "Menu \"%s\" not found", idname);
		return OPERATOR_CANCELLED;
	}

	if (mt->poll && mt->poll(C, mt) == 0)
		/* cancel but allow event to pass through, just like operators do */
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);

	pup = UI_popup_menu_begin(C, IFACE_(mt->label), ICON_NONE);
	layout = UI_popup_menu_layout(pup);

	menu.layout = layout;
	menu.type = mt;

	if (G.debug & G_DEBUG_WM) {
		printf("%s: opening menu \"%s\"\n", __func__, idname);
	}

	mt->draw(C, &menu);

	UI_popup_menu_end(C, pup);

	return OPERATOR_INTERFACE;
}


/*************************** Popup Block API **************************/

void UI_popup_block_invoke_ex(bContext *C, uiBlockCreateFunc func, void *arg, const char *opname, int opcontext)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *handle;
	
	handle = ui_popup_block_create(C, NULL, NULL, func, NULL, arg);
	handle->popup = true;
	handle->optype = (opname) ? WM_operatortype_find(opname, 0) : NULL;
	handle->opcontext = opcontext;
	
	UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
	WM_event_add_mousemove(C);
}

void UI_popup_block_invoke(bContext *C, uiBlockCreateFunc func, void *arg)
{
	UI_popup_block_invoke_ex(C, func, arg, NULL, WM_OP_INVOKE_DEFAULT);
}

void UI_popup_block_ex(bContext *C, uiBlockCreateFunc func, uiBlockHandleFunc popup_func, uiBlockCancelFunc cancel_func, void *arg, wmOperator *op)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *handle;
	
	handle = ui_popup_block_create(C, NULL, NULL, func, NULL, arg);
	handle->popup = true;
	handle->retvalue = 1;

	handle->popup_op = op;
	handle->popup_arg = arg;
	handle->popup_func = popup_func;
	handle->cancel_func = cancel_func;
	// handle->opcontext = opcontext;
	
	UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
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
	
	UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
	WM_event_add_mousemove(C);
}
#endif

void UI_popup_block_close(bContext *C, wmWindow *win, uiBlock *block)
{
	/* if loading new .blend while popup is open, window will be NULL */
	if (block->handle) {
		if (win) {
			UI_popup_handlers_remove(&win->modalhandlers, block->handle);
			ui_popup_block_free(C, block->handle);

			/* In the case we have nested popups, closing one may need to redraw another, see: T48874 */
			for (ARegion *ar = win->screen->regionbase.first; ar; ar = ar->next) {
				ED_region_tag_refresh_ui(ar);
			}
		}
	}
}

ColorPicker *ui_block_colorpicker_create(struct uiBlock *block)
{
	ColorPicker *cpicker = MEM_callocN(sizeof(ColorPicker), "color_picker");
	BLI_addhead(&block->color_pickers.list, cpicker);

	return cpicker;
}

void ui_rgb_to_color_picker_compat_v(const float rgb[3], float r_cp[3])
{
	switch (U.color_picker_type) {
		case USER_CP_CIRCLE_HSL:
			rgb_to_hsl_compat_v(rgb, r_cp);
			break;
		default:
			rgb_to_hsv_compat_v(rgb, r_cp);
			break;
	}
}

void ui_rgb_to_color_picker_v(const float rgb[3], float r_cp[3])
{
	switch (U.color_picker_type) {
		case USER_CP_CIRCLE_HSL:
			rgb_to_hsl_v(rgb, r_cp);
			break;
		default:
			rgb_to_hsv_v(rgb, r_cp);
			break;
	}
}

void ui_color_picker_to_rgb_v(const float r_cp[3], float rgb[3])
{
	switch (U.color_picker_type) {
		case USER_CP_CIRCLE_HSL:
			hsl_to_rgb_v(r_cp, rgb);
			break;
		default:
			hsv_to_rgb_v(r_cp, rgb);
			break;
	}
}

void ui_color_picker_to_rgb(float r_cp0, float r_cp1, float r_cp2, float *r, float *g, float *b)
{
	switch (U.color_picker_type) {
		case USER_CP_CIRCLE_HSL:
			hsl_to_rgb(r_cp0, r_cp1, r_cp2, r, g, b);
			break;
		default:
			hsv_to_rgb(r_cp0, r_cp1, r_cp2, r, g, b);
			break;
	}
}
