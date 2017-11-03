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

/** \file blender/editors/interface/interface_region_tooltip.c
 *  \ingroup edinterface
 *
 * ToolTip Region and Construction
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "ED_screen.h"

#include "interface_intern.h"
#include "interface_regions_intern.h"

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
		uint x_pos;     /* x cursor position at the end of the last line */
		uint lines;     /* number of lines, 1 or more with word-wrap */
	} line_geom[MAX_TOOLTIP_LINES];

	int wrap_width;

	int totline;
	int toth, lineh;
} uiTooltipData;

#define UI_TIP_LC_MAX 6

BLI_STATIC_ASSERT(UI_TIP_LC_MAX == UI_TIP_LC_ALERT + 1, "invalid lc-max");
BLI_STATIC_ASSERT(sizeof(((uiTooltipData *)NULL)->format[0]) <= sizeof(int), "oversize");

/* -------------------------------------------------------------------- */
/** \name ToolTip Callbacks (Draw & Free)
 * \{ */

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
	unsigned char drawcol[4] = {0, 0, 0, 255}; /* to store color in while drawing (alpha is always 255) */

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

			rgb_float_to_uchar(drawcol, tip_colors[UI_TIP_LC_MAIN]);
			UI_fontstyle_set(&fstyle_header);
			UI_fontstyle_draw(&fstyle_header, &bbox, data->header, drawcol);

			/* offset to the end of the last line */
			xofs = data->line_geom[i].x_pos;
			yofs = data->lineh * (data->line_geom[i].lines - 1);
			bbox.xmin += xofs;
			bbox.ymax -= yofs;

			fstyle_header.shadow = 0;
			rgb_float_to_uchar(drawcol, tip_colors[UI_TIP_LC_ACTIVE]);
			UI_fontstyle_draw(&fstyle_header, &bbox, data->active_info, drawcol);

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
			rgb_float_to_uchar(drawcol, tip_colors[data->format[i].color_id]);
			UI_fontstyle_draw(&fstyle_mono, &bbox, data->lines[i], drawcol);
		}
		else {
			uiFontStyle fstyle_normal = data->fstyle;
			BLI_assert(data->format[i].style == UI_TIP_STYLE_NORMAL);
			fstyle_normal.word_wrap = true;

			/* draw remaining data */
			rgb_float_to_uchar(drawcol, tip_colors[data->format[i].color_id]);
			UI_fontstyle_set(&fstyle_normal);
			UI_fontstyle_draw(&fstyle_normal, &bbox, data->lines[i], drawcol);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name ToolTip Creation
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name ToolTip Public API
 * \{ */

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

/** \} */
