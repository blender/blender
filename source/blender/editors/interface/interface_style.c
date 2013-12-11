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

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"


#include "BLF_api.h"
#include "BLF_translation.h"

#include "UI_interface.h"

#include "ED_datafiles.h"

#include "interface_intern.h"


/* style + theme + layout-engine = UI */

/* 
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


void uiStyleFontDrawExt(uiFontStyle *fs, const rcti *rect, const char *str,
                        size_t len, float *r_xofs, float *r_yofs)
{
	float height;
	int xofs = 0, yofs;
	
	uiStyleFontSet(fs);

	height = BLF_ascender(fs->uifont_id);
	yofs = ceil(0.5f * (BLI_rcti_size_y(rect) - height));

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
	BLF_enable(fs->uifont_id, BLF_CLIPPING);
	BLF_position(fs->uifont_id, rect->xmin + xofs, rect->ymin + yofs, 0.0f);

	if (fs->shadow) {
		BLF_enable(fs->uifont_id, BLF_SHADOW);
		BLF_shadow(fs->uifont_id, fs->shadow, fs->shadowcolor, fs->shadowcolor, fs->shadowcolor, fs->shadowalpha);
		BLF_shadow_offset(fs->uifont_id, fs->shadx, fs->shady);
	}

	if (fs->kerning == 1)
		BLF_enable(fs->uifont_id, BLF_KERNING_DEFAULT);

	BLF_draw(fs->uifont_id, str, len);
	BLF_disable(fs->uifont_id, BLF_CLIPPING);
	if (fs->shadow)
		BLF_disable(fs->uifont_id, BLF_SHADOW);
	if (fs->kerning == 1)
		BLF_disable(fs->uifont_id, BLF_KERNING_DEFAULT);

	*r_xofs = xofs;
	*r_yofs = yofs;
}

void uiStyleFontDraw(uiFontStyle *fs, const rcti *rect, const char *str)
{
	float xofs, yofs;
	uiStyleFontDrawExt(fs, rect, str,
	                   BLF_DRAW_STR_DUMMY_MAX, &xofs, &yofs);
}

/* drawn same as above, but at 90 degree angle */
void uiStyleFontDrawRotated(uiFontStyle *fs, const rcti *rect, const char *str)
{
	float height;
	int xofs, yofs;
	float angle;
	rcti txtrect;

	uiStyleFontSet(fs);

	height = BLF_ascender(fs->uifont_id);
	/* becomes x-offset when rotated */
	xofs = ceil(0.5f * (BLI_rcti_size_y(rect) - height));

	/* ignore UI_STYLE, always aligned to top */

	/* rotate counter-clockwise for now (assumes left-to-right language)*/
	xofs += height;
	yofs = BLF_width(fs->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX) + 5;
	angle = (float)M_PI / 2.0f;

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

	if (fs->shadow) {
		BLF_enable(fs->uifont_id, BLF_SHADOW);
		BLF_shadow(fs->uifont_id, fs->shadow, fs->shadowcolor, fs->shadowcolor, fs->shadowcolor, fs->shadowalpha);
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

/* ************** helpers ************************ */
/* XXX: read a style configure */
uiStyle *UI_GetStyle(void)
{
	uiStyle *style = NULL;
	/* offset is two struct uiStyle pointers */
	/* style = BLI_findstring(&U.uistyles, "Unifont Style", sizeof(style) * 2) */;
	return (style != NULL) ? style : U.uistyles.first;
}

/* for drawing, scaled with DPI setting */
uiStyle *UI_GetStyleDraw(void)
{
	uiStyle *style = UI_GetStyle();
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

/* temporarily, does widget font */
int UI_GetStringWidth(const char *str)
{
	uiStyle *style = UI_GetStyle();
	uiFontStyle *fstyle = &style->widget;
	int width;
	
	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	
	uiStyleFontSet(fstyle);
	width = BLF_width(fstyle->uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);
	
	if (fstyle->kerning == 1)
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	
	return width;
}

/* temporarily, does widget font */
void UI_DrawString(float x, float y, const char *str)
{
	uiStyle *style = UI_GetStyle();
	
	if (style->widget.kerning == 1)
		BLF_enable(style->widget.uifont_id, BLF_KERNING_DEFAULT);

	uiStyleFontSet(&style->widget);
	BLF_position(style->widget.uifont_id, x, y, 0.0f);
	BLF_draw(style->widget.uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);

	if (style->widget.kerning == 1)
		BLF_disable(style->widget.uifont_id, BLF_KERNING_DEFAULT);
}

/* ************** init exit ************************ */

/* called on each startup.blend read */
/* reading without uifont will create one */
void uiStyleInit(void)
{
	uiFont *font = U.uifonts.first;
	uiStyle *style = U.uistyles.first;
	int monofont_size = datatoc_bmonofont_ttf_size;
	unsigned char *monofont_ttf = (unsigned char *)datatoc_bmonofont_ttf;
	
	/* recover from uninitialized dpi */
	if (U.dpi == 0)
		U.dpi = 72;
	CLAMP(U.dpi, 48, 144);
	
	/* default builtin */
	if (font == NULL) {
		font = MEM_callocN(sizeof(uiFont), "ui font");
		BLI_addtail(&U.uifonts, font);
		
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
			if (font->blf_id == -1)
				font->blf_id = BLF_load_mem("default", (unsigned char *)datatoc_bfont_ttf, datatoc_bfont_ttf_size);
		}

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

	/* reload */
	BLF_unload("monospace");
	blf_mono_font = -1;
	blf_mono_font_render = -1;
#endif

	/* XXX, this should be moved into a style, but for now best only load the monospaced font once. */
	if (blf_mono_font == -1)
		blf_mono_font = BLF_load_mem_unique("monospace", monofont_ttf, monofont_size);

	BLF_size(blf_mono_font, 12 * U.pixelsize, 72);
	
	/* second for rendering else we get threading problems */
	if (blf_mono_font_render == -1)
		blf_mono_font_render = BLF_load_mem_unique("monospace", monofont_ttf, monofont_size);

	BLF_size(blf_mono_font_render, 12 * U.pixelsize, 72 );
}

void uiStyleFontSet(uiFontStyle *fs)
{
	uiFont *font = uifont_to_blfont(fs->uifont_id);
	
	BLF_size(font->blf_id, fs->points * U.pixelsize, U.dpi);
}

