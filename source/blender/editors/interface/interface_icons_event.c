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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_icons_event.c
 *  \ingroup edinterface
 *
 * A special set of icons to represent input devices,
 * this is a mix of text (via fonts) and a handful of custom glyphs for special keys.
 *
 * Event codes are used as identifiers.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "GPU_draw.h"
#include "GPU_matrix.h"
#include "GPU_batch.h"
#include "GPU_immediate.h"
#include "GPU_state.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_fileops_types.h"
#include "BLI_math_vector.h"

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_appdir.h"
#include "BKE_studiolight.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "BIF_glutil.h"
#include "BLF_api.h"

#include "DEG_depsgraph.h"

#include "DRW_engine.h"

#include "ED_datafiles.h"
#include "ED_keyframes_draw.h"
#include "ED_render.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

static void icon_draw_rect_input_small_text_ex(
        const rctf *rect, const float color[4], const float margin[2], const char *str,
        int font_size)
{
	BLF_batch_draw_flush();
	const int font_id = BLF_default();
	BLF_color4fv(font_id, color);
	BLF_size(font_id, font_size * U.pixelsize, U.dpi);
	BLF_position(font_id, rect->xmin + margin[0] * 2, rect->ymin + margin[1] * 5, 0.0f);
	BLF_draw(font_id, str, BLF_DRAW_STR_DUMMY_MAX);
	BLF_batch_draw_flush();
}

static void icon_draw_rect_input_small_text(
        const rctf *rect, const float color[4], const float margin[2], const char *str)
{
	icon_draw_rect_input_small_text_ex(rect, color, margin, str, 8);
}

static void icon_draw_rect_input_default_text(
        const rctf *rect,
        const float color[4], const float margin[2], const char *str)
{
	BLF_batch_draw_flush();
	const int font_id = BLF_default();
	BLF_color4fv(font_id, color);
	BLF_position(font_id, (int)(rect->xmin + margin[0] * 5), (int)(rect->ymin + margin[1] * 5), 0.0f);
	BLF_draw(font_id, str, BLF_DRAW_STR_DUMMY_MAX);
	BLF_batch_draw_flush();
}

static void icon_draw_rect_input_mono_text(
        const rctf *rect,
        const float color[4], const float margin[2], const char *str)
{
	BLF_batch_draw_flush();
	const int font_id = blf_mono_font;
	BLF_color4fv(font_id, color);
	BLF_size(font_id, 20 * U.pixelsize, U.dpi);
	BLF_position(font_id, (int)(rect->xmin + margin[0] * 5), (int)(rect->ymin + margin[1] * 5), 0.0f);
	BLF_draw(font_id, str, BLF_DRAW_STR_DUMMY_MAX);
	BLF_batch_draw_flush();
}

static void icon_draw_rect_input_line_prim(
        const rctf *rect,
        const float color[4],
        const int prim,
        const char lines[][2], int lines_len)
{
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	BLI_assert(ELEM(prim, GPU_PRIM_LINE_LOOP, GPU_PRIM_LINE_STRIP));
	const uint pos_id = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor4fv(color);
	immBegin(prim, lines_len);
	float w_inv = BLI_rctf_size_x(rect) / 255.0f;
	float h_inv = BLI_rctf_size_y(rect) / 255.0f;
	for (int i = 0; i < lines_len; i++) {
		immVertex2f(
		        pos_id,
		        round_fl_to_int(rect->xmin + ((float)lines[i][0] * w_inv)),
		        round_fl_to_int(rect->ymin + ((float)lines[i][1] * h_inv))
		);
	}
	immEnd();
	immUnbindProgram();
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}

void icon_draw_rect_input(
        float x, float y, int w, int h, float UNUSED(alpha),
        short event_type, short UNUSED(event_value))
{
	float color[4];
	const float margin[2] = {w / 20.0f, h / 20.0f};
	UI_GetThemeColor4fv(TH_TEXT, color);
	UI_draw_roundbox_corner_set(UI_CNR_ALL);
	UI_draw_roundbox_aa(
	        false,
	        (int)x,
	        (int)y,
	        (int)(x + w),
	        (int)(y + h), 4.0f, color
	);

	const rctf rect = {
		.xmin = x,
		.ymin = y,
		.xmax = x + w,
		.ymax = y + h,
	};

	const bool simple_text = false;

	if ((event_type >= AKEY) || (ZKEY <= event_type)) {
		char str[2] = {'A' + (event_type - AKEY), '\0'};
		icon_draw_rect_input_default_text(&rect, color, margin, str);
	}
	if ((event_type >= F1KEY) || (F12KEY <= event_type)) {
		char str[3] = {'F', '1' + (event_type - F1KEY), '\0'};
		icon_draw_rect_input_default_text(&rect, color, margin, str);
	}
	else if (event_type == LEFTSHIFTKEY) {
		if (simple_text) {
			icon_draw_rect_input_small_text(&rect, color, margin, "Shift");
		}
		else {
			rctf rect_ofs = rect;
			BLI_rctf_translate(&rect_ofs, (w / -14.0f), (w / -14.0f));
			icon_draw_rect_input_mono_text(&rect_ofs, color, margin, (const char[]){0xe2, 0x87, 0xa7, 0x0});
		}
	}
	else if (event_type == LEFTCTRLKEY) {
		if (simple_text) {
			icon_draw_rect_input_small_text(&rect, color, margin, "Ctrl");
		}
		else {
			rctf rect_ofs = rect;
			BLI_rctf_translate(&rect_ofs, (w / -16.0f), 0.0f);
			icon_draw_rect_input_default_text(&rect_ofs, color, margin, "^");
		}
	}
	else if (event_type == LEFTALTKEY) {
		if (simple_text) {
			icon_draw_rect_input_small_text(&rect, color, margin, "Alt");
		}
		else {
			rctf rect_ofs = rect;
			BLI_rctf_translate(&rect_ofs, (w / -8.0f), 0.0f);
			icon_draw_rect_input_default_text(&rect_ofs, color, margin, (const char[]){0xe2, 0x8c, 0xa5, 0x0});
		}
	}
	else if (event_type == OSKEY) {
		icon_draw_rect_input_small_text(&rect, color, margin, "OS");
	}
	else if (event_type == DELKEY) {
		icon_draw_rect_input_small_text(&rect, color, margin, "Del");
	}
	else if (event_type == TABKEY) {
		if (simple_text) {
			icon_draw_rect_input_small_text(&rect, color, margin, "Tab");
		}
		else {
			rctf rect_ofs = rect;
			BLI_rctf_translate(&rect_ofs, (w / -12.0f), (w / -12.0f));
			icon_draw_rect_input_mono_text(&rect_ofs, color, margin, (const char[]){0xe2, 0x86, 0xb9, 0x0});
		}
	}
	else if (event_type == HOMEKEY) {
		if (simple_text) {
			icon_draw_rect_input_small_text(&rect, color, margin, "Home");
		}
		else {
			rctf rect_ofs = rect;
			BLI_rctf_translate(&rect_ofs, (w / -12.0f), (w / -12.0f));
			icon_draw_rect_input_mono_text(&rect_ofs, color, margin, (const char[]){0xe2, 0x87, 0xa4, 0x0});
		}
	}
	else if (event_type == ENDKEY) {
		if (simple_text) {
			icon_draw_rect_input_small_text(&rect, color, margin, "End");
		}
		else {
			rctf rect_ofs = rect;
			BLI_rctf_translate(&rect_ofs, (w / -12.0f), (w / -12.0f));
			icon_draw_rect_input_mono_text(&rect_ofs, color, margin, (const char[]){0xe2, 0x87, 0xa5, 0x0});
		}
	}
	else if (event_type == RETKEY) {
		if (simple_text) {
			icon_draw_rect_input_small_text(&rect, color, margin, "Ret");
		}
		else {
			rctf rect_ofs = rect;
			BLI_rctf_translate(&rect_ofs, (w / -8.0f), (w / -6.0f));
			icon_draw_rect_input_mono_text(&rect_ofs, color, margin, (const char[]){0xe2, 0x8f, 0x8e, 0x0});
		}
	}
	else if (event_type == ESCKEY) {
		icon_draw_rect_input_small_text(&rect, color, margin, "Esc");
	}
	else if (event_type == PAGEUPKEY) {
		icon_draw_rect_input_small_text_ex(&rect, color, margin, (const char[]){'P', 0xe2, 0x86, 0x91, 0x0}, 10);
	}
	else if (event_type == PAGEDOWNKEY) {
		icon_draw_rect_input_small_text_ex(&rect, color, margin, (const char[]){'P', 0xe2, 0x86, 0x93, 0x0}, 10);
	}
	else if (event_type == LEFTARROWKEY) {
		icon_draw_rect_input_default_text(&rect, color, margin, (const char[]){0xe2, 0x86, 0x90, 0x0});
	}
	else if (event_type == UPARROWKEY) {
		icon_draw_rect_input_default_text(&rect, color, margin, (const char[]){0xe2, 0x86, 0x91, 0x0});
	}
	else if (event_type == RIGHTARROWKEY) {
		icon_draw_rect_input_default_text(&rect, color, margin, (const char[]){0xe2, 0x86, 0x92, 0x0});
	}
	else if (event_type == DOWNARROWKEY) {
		icon_draw_rect_input_default_text(&rect, color, margin, (const char[]){0xe2, 0x86, 0x93, 0x0});
	}
	else if (event_type == SPACEKEY) {
		const uchar lines[] = {60, 118, 60, 60, 195, 60, 195, 118};
		icon_draw_rect_input_line_prim(
		        &rect, color, GPU_PRIM_LINE_STRIP,
		        (const void *)lines, ARRAY_SIZE(lines) / 2);
	}
}
