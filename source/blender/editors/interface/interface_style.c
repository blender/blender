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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_style.c
 *  \ingroup edinterface
 */


#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "BIF_gl.h"

#include "BLF_api.h"
#ifdef WITH_INTERNATIONAL
#  include "BLT_translation.h"
#endif

#include "UI_interface.h"

#include "ED_datafiles.h"

#include "interface_intern.h"

#ifdef WIN32
#  include "BLI_math_base.h" /* M_PI */
#endif

/* style + theme + layout-engine = UI */

/**
 * This is a complete set of layout rules, the 'state' of the Layout
 * Engine. Multiple styles are possible, defined via C or Python. Styles
 * get a name, and will typically get activated per region type, like
 * "Header", or "Listview" or "Toolbar". Properties of Style definitions
 * are:
 *
 * - default column properties, internal spacing, aligning, min/max width
 * - button alignment rules (for groups)
 * - label placement rules
 * - internal labeling or external labeling default
 * - default minimum widths for buttons/labels (in amount of characters)
 * - font types, styles and relative sizes for Panel titles, labels, etc.
 */


/* ********************************************** */

static uiStyle *ui_style_new(ListBase *styles, const char *name, short uifont_id)
{
	uiStyle *style = MEM_callocN(sizeof(uiStyle), "new style");

	BLI_addtail(styles, style);
	BLI_strncpy(style->name, name, MAX_STYLE_NAME);

	style->panelzoom = 1.0; /* unused */

	style->paneltitle.uifont_id = uifont_id;
	style->paneltitle.points = 12;
	style->paneltitle.kerning = 1;
	style->paneltitle.shadow = 1;
	style->paneltitle.shadx = 0;
	style->paneltitle.shady = -1;
	style->paneltitle.shadowalpha = 0.15f;
	style->paneltitle.shadowcolor = 1.0f;

	style->grouplabel.uifont_id = uifont_id;
	style->grouplabel.points = 12;
	style->grouplabel.kerning = 1;
	style->grouplabel.shadow = 3;
	style->grouplabel.shadx = 0;
	style->grouplabel.shady = -1;
	style->grouplabel.shadowalpha = 0.25f;

	style->widgetlabel.uifont_id = uifont_id;
	style->widgetlabel.points = 11;
	style->widgetlabel.kerning = 1;
	style->widgetlabel.shadow = 3;
	style->widgetlabel.shadx = 0;
	style->widgetlabel.shady = -1;
	style->widgetlabel.shadowalpha = 0.15f;
	style->widgetlabel.shadowcolor = 1.0f;

	style->widget.uifont_id = uifont_id;
	style->widget.points = 11;
	style->widget.kerning = 1;
	style->widget.shadowalpha = 0.25f;

	style->columnspace = 8;
	style->templatespace = 5;
	style->boxspace = 5;
	style->buttonspacex = 8;
	style->buttonspacey = 2;
	style->panelspace = 8;
	style->panelouter = 4;

	return style;
}

static uiFont *uifont_to_blfont(int id)
{
	uiFont *font = U.uifonts.first;

	for (; font; font = font->next) {
		if (font->uifont_id == id) {
			return font;
		}
	}
	return U.uifonts.first;
}

/* *************** draw ************************ */


void UI_fontstyle_draw_ex(
        const uiFontStyle *fs, const rcti *rect, const char *str, const unsigned char col[4],
        size_t len, float *r_xofs, float *r_yofs)
{
	int xofs = 0, yofs;
	int font_flag = BLF_CLIPPING;

	UI_fontstyle_set(fs);

	/* set the flag */
	if (fs->shadow) {
		font_flag |= BLF_SHADOW;
		const float shadow_color[4] = {fs->shadowcolor, fs->shadowcolor, fs->shadowcolor, fs->shadowalpha};
		BLF_shadow(fs->uifont_id, fs->shadow, shadow_color);
		BLF_shadow_offset(fs->uifont_id, fs->shadx, fs->shady);
	}
	if (fs->kerning == 1) {
		font_flag |= BLF_KERNING_DEFAULT;
	}
	if (fs->word_wrap == 1) {
		font_flag |= BLF_WORD_WRAP;
	}

	BLF_enable(fs->uifont_id, font_flag);

	if (fs->word_wrap == 1) {
		/* draw from boundbox top */
		yofs = BLI_rcti_size_y(rect) - BLF_height_max(fs->uifont_id);
	}
	else {
		/* draw from boundbox center */
		yofs = ceil(0.5f * (BLI_rcti_size_y(rect) - BLF_ascender(fs->uifont_id)));
	}

	if (fs->align == UI_STYLE_TEXT_CENTER) {
		xofs = floor(0.5f * (BLI_rcti_size_x(rect) - BLF_width(fs->uifont_id, str, len)));
		/* don't center text if it chops off the start of the text, 2 gives some margin */
		if (xofs < 2) {
			xofs = 2;
		}
	}
	else if (fs->align == UI_STYLE_TEXT_RIGHT) {
		xofs = BLI_rcti_size_x(rect) - BLF_width(fs->uifont_id, str, len) - 0.1f * U.widget_unit;
	}

	/* clip is very strict, so we give it some space */
	BLF_clipping(fs->uifont_id, rect->xmin - 2, rect->ymin - 4, rect->xmax + 1, rect->ymax + 4);
	BLF_position(fs->uifont_id, rect->xmin + xofs, rect->ymin + yofs, 0.0f);
	BLF_color4ubv(fs->uifont_id, col);

	BLF_draw(fs->uifont_id, str, len);

	BLF_disable(fs->uifont_id, font_flag);

	*r_xofs = xofs;
	*r_yofs = yofs;
}

void UI_fontstyle_draw(const uiFontStyle *fs, const rcti *rect, const char *str, const unsigned char col[4])
{
	float xofs, yofs;

	UI_fontstyle_draw_ex(
	        fs, rect, str, col,
	        BLF_DRAW_STR_DUMMY_MAX, &xofs, &yofs);
}

/* drawn same as above, but at 90 degree angle */
void UI_fontstyle_draw_rotated(const uiFontStyle *fs, const rcti *rect, const char *str, const unsigned char col[4])
{
	float height;
	int xofs, yofs;
	float angle;
	rcti txtrect;

	UI_fontstyle_set(fs);

	height = BLF_ascender(fs->uifont_id);
	/* becomes x-offset when rotated */
	xofs = ceil(0.5f * (BLI_rcti_size_y(rect) - height));

	/* ignore UI_STYLE, always aligned to top */

	/* rotate counter-clockwise for now (assumes left-to-right language)*/
	xofs += height;
	yofs = BLF_width(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX) + 5;
	angle = M_PI_2;

	/* translate rect to vertical */
	txtrect.xmin = rect->xmin - BLI_rcti_size_y(rect);
	txtrect.ymin = rect->ymin - BLI_rcti_size_x(rect);
	txtrect.xmax = rect->xmin;
	txtrect.ymax = rect->ymin;

	/* clip is very strict, so we give it some space */
	/* clipping is done without rotation, so make rect big enough to contain both positions */
	BLF_clipping(fs->uifont_id, txtrect.xmin - 1, txtrect.ymin - yofs - xofs - 4, rect->xmax + 1, rect->ymax + 4);
	BLF_enable(fs->uifont_id, BLF_CLIPPING);
	BLF_position(fs->uifont_id, txtrect.xmin + xofs, txtrect.ymax - yofs, 0.0f);

	BLF_enable(fs->uifont_id, BLF_ROTATION);
	BLF_rotation(fs->uifont_id, angle);
	BLF_color4ubv(fs->uifont_id, col);

	if (fs->shadow) {
		BLF_enable(fs->uifont_id, BLF_SHADOW);
		const float shadow_color[4] = {fs->shadowcolor, fs->shadowcolor, fs->shadowcolor, fs->shadowalpha};
		BLF_shadow(fs->uifont_id, fs->shadow, shadow_color);
		BLF_shadow_offset(fs->uifont_id, fs->shadx, fs->shady);
	}

	if (fs->kerning == 1)
		BLF_enable(fs->uifont_id, BLF_KERNING_DEFAULT);

	BLF_draw(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);
	BLF_disable(fs->uifont_id, BLF_ROTATION);
	BLF_disable(fs->uifont_id, BLF_CLIPPING);
	if (fs->shadow)
		BLF_disable(fs->uifont_id, BLF_SHADOW);
	if (fs->kerning == 1)
		BLF_disable(fs->uifont_id, BLF_KERNING_DEFAULT);
}

/**
 * Similar to #UI_fontstyle_draw
 * but ignore alignment, shadow & no clipping rect.
 *
 * For drawing on-screen labels.
 */
void UI_fontstyle_draw_simple(const uiFontStyle *fs, float x, float y, const char *str, const unsigned char col[4])
{
	if (fs->kerning == 1)
		BLF_enable(fs->uifont_id, BLF_KERNING_DEFAULT);

	UI_fontstyle_set(fs);
	BLF_position(fs->uifont_id, x, y, 0.0f);
	BLF_color4ubv(fs->uifont_id, col);
	BLF_draw(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);

	if (fs->kerning == 1)
		BLF_disable(fs->uifont_id, BLF_KERNING_DEFAULT);
}

/**
 * Same as #UI_fontstyle_draw but draw a colored backdrop.
 */
void UI_fontstyle_draw_simple_backdrop(
        const uiFontStyle *fs, float x, float y, const char *str,
        const float col_fg[4], const float col_bg[4])
{
	if (fs->kerning == 1)
		BLF_enable(fs->uifont_id, BLF_KERNING_DEFAULT);

	UI_fontstyle_set(fs);

	{
		const float width = BLF_width(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);
		const float height = BLF_height_max(fs->uifont_id);
		const float decent = BLF_descender(fs->uifont_id);
		const float margin = height / 4.0f;

		/* backdrop */
		float color[4] = { col_bg[0], col_bg[1], col_bg[2], 0.5f };

		UI_draw_roundbox_corner_set(UI_CNR_ALL);
		UI_draw_roundbox_aa(true,
		        x - margin,
		        (y + decent) - margin,
		        x + width + margin,
		        (y + decent) + height + margin,
		        margin, color);
	}

	BLF_position(fs->uifont_id, x, y, 0.0f);
	BLF_color4fv(fs->uifont_id, col_fg);
	BLF_draw(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);

	if (fs->kerning == 1)
		BLF_disable(fs->uifont_id, BLF_KERNING_DEFAULT);
}


/* ************** helpers ************************ */
/* XXX: read a style configure */
uiStyle *UI_style_get(void)
{
#if 0
	uiStyle *style = NULL;
	/* offset is two struct uiStyle pointers */
	style = BLI_findstring(&U.uistyles, "Unifont Style", sizeof(style) * 2);
	return (style != NULL) ? style : U.uistyles.first;
#else
	return U.uistyles.first;
#endif
}

/* for drawing, scaled with DPI setting */
uiStyle *UI_style_get_dpi(void)
{
	uiStyle *style = UI_style_get();
	static uiStyle _style;

	_style = *style;

	_style.paneltitle.shadx = (short)(UI_DPI_FAC * _style.paneltitle.shadx);
	_style.paneltitle.shady = (short)(UI_DPI_FAC * _style.paneltitle.shady);
	_style.grouplabel.shadx = (short)(UI_DPI_FAC * _style.grouplabel.shadx);
	_style.grouplabel.shady = (short)(UI_DPI_FAC * _style.grouplabel.shady);
	_style.widgetlabel.shadx = (short)(UI_DPI_FAC * _style.widgetlabel.shadx);
	_style.widgetlabel.shady = (short)(UI_DPI_FAC * _style.widgetlabel.shady);

	_style.columnspace = (short)(UI_DPI_FAC * _style.columnspace);
	_style.templatespace = (short)(UI_DPI_FAC * _style.templatespace);
	_style.boxspace = (short)(UI_DPI_FAC * _style.boxspace);
	_style.buttonspacex = (short)(UI_DPI_FAC * _style.buttonspacex);
	_style.buttonspacey = (short)(UI_DPI_FAC * _style.buttonspacey);
	_style.panelspace = (short)(UI_DPI_FAC * _style.panelspace);
	_style.panelouter = (short)(UI_DPI_FAC * _style.panelouter);

	return &_style;
}

int UI_fontstyle_string_width(const uiFontStyle *fs, const char *str)
{
	int width;

	if (fs->kerning == 1) /* for BLF_width */
		BLF_enable(fs->uifont_id, BLF_KERNING_DEFAULT);

	UI_fontstyle_set(fs);
	width = BLF_width(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);

	if (fs->kerning == 1)
		BLF_disable(fs->uifont_id, BLF_KERNING_DEFAULT);

	return width;
}

int UI_fontstyle_height_max(const uiFontStyle *fs)
{
	UI_fontstyle_set(fs);
	return BLF_height_max(fs->uifont_id);
}


/* ************** init exit ************************ */

/* called on each startup.blend read */
/* reading without uifont will create one */
void uiStyleInit(void)
{
	uiFont *font;
	uiStyle *style = U.uistyles.first;
	int monofont_size = datatoc_bmonofont_ttf_size;
	unsigned char *monofont_ttf = (unsigned char *)datatoc_bmonofont_ttf;

	/* recover from uninitialized dpi */
	if (U.dpi == 0)
		U.dpi = 72;
	CLAMP(U.dpi, 48, 144);

	for (font = U.uifonts.first; font; font = font->next) {
		BLF_unload_id(font->blf_id);
	}

	if (blf_mono_font != -1) {
		BLF_unload_id(blf_mono_font);
		blf_mono_font = -1;
	}

	if (blf_mono_font_render != -1) {
		BLF_unload_id(blf_mono_font_render);
		blf_mono_font_render = -1;
	}

	font = U.uifonts.first;

	/* default builtin */
	if (font == NULL) {
		font = MEM_callocN(sizeof(uiFont), "ui font");
		BLI_addtail(&U.uifonts, font);
	}

	if (U.font_path_ui[0]) {
		BLI_strncpy(font->filename, U.font_path_ui, sizeof(font->filename));
		font->uifont_id = UIFONT_CUSTOM1;
	}
	else {
		BLI_strncpy(font->filename, "default", sizeof(font->filename));
		font->uifont_id = UIFONT_DEFAULT;
	}

	for (font = U.uifonts.first; font; font = font->next) {

		if (font->uifont_id == UIFONT_DEFAULT) {
#ifdef WITH_INTERNATIONAL
			int font_size = datatoc_bfont_ttf_size;
			unsigned char *font_ttf = (unsigned char *)datatoc_bfont_ttf;
			static int last_font_size = 0;

			/* use unicode font for translation */
			if (U.transopts & USER_DOTRANSLATE) {
				font_ttf = BLF_get_unifont(&font_size);

				if (!font_ttf) {
					/* fall back if not found */
					font_size = datatoc_bfont_ttf_size;
					font_ttf = (unsigned char *)datatoc_bfont_ttf;
				}
			}

			/* relload only if needed */
			if (last_font_size != font_size) {
				BLF_unload("default");
				last_font_size = font_size;
			}

			font->blf_id = BLF_load_mem("default", font_ttf, font_size);
#else
			font->blf_id = BLF_load_mem("default", (unsigned char *)datatoc_bfont_ttf, datatoc_bfont_ttf_size);
#endif
		}
		else {
			font->blf_id = BLF_load(font->filename);
			if (font->blf_id == -1) {
				font->blf_id = BLF_load_mem("default", (unsigned char *)datatoc_bfont_ttf, datatoc_bfont_ttf_size);
			}
		}

		BLF_default_set(font->blf_id);

		if (font->blf_id == -1) {
			if (G.debug & G_DEBUG)
				printf("%s: error, no fonts available\n", __func__);
		}
		else {
			/* ? just for speed to initialize?
			 * Yes, this build the glyph cache and create
			 * the texture.
			 */
			BLF_size(font->blf_id, 11 * U.pixelsize, U.dpi);
			BLF_size(font->blf_id, 12 * U.pixelsize, U.dpi);
			BLF_size(font->blf_id, 14 * U.pixelsize, U.dpi);
		}
	}

	if (style == NULL) {
		ui_style_new(&U.uistyles, "Default Style", UIFONT_DEFAULT);
	}

#ifdef WITH_INTERNATIONAL
	/* use unicode font for text editor and interactive console */
	if (U.transopts & USER_DOTRANSLATE) {
		monofont_ttf = BLF_get_unifont_mono(&monofont_size);

		if (!monofont_ttf) {
			/* fall back if not found */
			monofont_size = datatoc_bmonofont_ttf_size;
			monofont_ttf = (unsigned char *)datatoc_bmonofont_ttf;
		}
	}
#endif

	/* XXX, this should be moved into a style, but for now best only load the monospaced font once. */
	BLI_assert(blf_mono_font == -1);
	if (U.font_path_ui_mono[0]) {
		blf_mono_font = BLF_load_unique(U.font_path_ui_mono);
	}
	if (blf_mono_font == -1) {
		blf_mono_font = BLF_load_mem_unique("monospace", monofont_ttf, monofont_size);
	}

	BLF_size(blf_mono_font, 12 * U.pixelsize, 72);

	/* Set default flags based on UI preferences (not render fonts) */
	{
		int flag_enable = 0, flag_disable = 0;
		if ((U.text_render & USER_TEXT_DISABLE_HINTING) == 0) {
			flag_enable |= BLF_HINTING;
		}
		else {
			flag_disable |= BLF_HINTING;
		}

		if (U.text_render & USER_TEXT_DISABLE_AA) {
			flag_enable |= BLF_MONOCHROME;
		}
		else {
			flag_disable |= BLF_MONOCHROME;
		}

		for (font = U.uifonts.first; font; font = font->next) {
			if (font->blf_id != -1) {
				BLF_enable(font->blf_id, flag_enable);
				BLF_disable(font->blf_id, flag_disable);
			}
		}
		if (blf_mono_font != -1) {
			BLF_enable(blf_mono_font, flag_enable);
			BLF_disable(blf_mono_font, flag_disable);
		}
	}

	/**
	 * Second for rendering else we get threading problems,
	 *
	 * \note This isn't good that the render font depends on the preferences,
	 * keep for now though, since without this there is no way to display many unicode chars.
	 */
	if (blf_mono_font_render == -1)
		blf_mono_font_render = BLF_load_mem_unique("monospace", monofont_ttf, monofont_size);

	BLF_size(blf_mono_font_render, 12 * U.pixelsize, 72);
}

void UI_fontstyle_set(const uiFontStyle *fs)
{
	uiFont *font = uifont_to_blfont(fs->uifont_id);

	BLF_size(font->blf_id, fs->points * U.pixelsize, U.dpi);
}
