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

/** \file blender/editors/screen/screen_draw.c
 *  \ingroup edscr
 */

#include "ED_screen.h"

#include "GPU_framebuffer.h"
#include "GPU_immediate.h"

#include "WM_api.h"


/**
 * Calculates a scale factor to squash the preview for \a screen into a rectangle of given size and aspect.
 */
static void screen_preview_scale_get(
        const bScreen *screen, float size_x, float size_y,
        const float asp[2],
        float r_scale[2])
{
	float max_x = 0, max_y = 0;

	for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
		max_x = MAX2(max_x, sa->totrct.xmax);
		max_y = MAX2(max_y, sa->totrct.ymax);
	}
	r_scale[0] = (size_x * asp[0]) / max_x;
	r_scale[1] = (size_y * asp[1]) / max_y;
}

static void screen_preview_draw_areas(const bScreen *screen, const float scale[2], const float col[4],
                                      const float ofs_between_areas)
{
	const float ofs_h = ofs_between_areas * 0.5f;
	unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 2, KEEP_FLOAT);
	rctf rect;

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor4fv(col);

	for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
		rect.xmin = sa->totrct.xmin * scale[0] + ofs_h;
		rect.xmax = sa->totrct.xmax * scale[0] - ofs_h;
		rect.ymin = sa->totrct.ymin * scale[1] + ofs_h;
		rect.ymax = sa->totrct.ymax * scale[1] - ofs_h;

		immBegin(PRIM_TRIANGLE_FAN, 4);
		immVertex2f(pos, rect.xmin, rect.ymin);
		immVertex2f(pos, rect.xmax, rect.ymin);
		immVertex2f(pos, rect.xmax, rect.ymax);
		immVertex2f(pos, rect.xmin, rect.ymax);
		immEnd();
	}

	immUnbindProgram();
}

static void screen_preview_draw(const bScreen *screen, int size_x, int size_y)
{
	const float asp[2] = {1.0f, 0.8f}; /* square previews look a bit ugly */
	/* could use theme color (tui.wcol_menu_item.text), but then we'd need to regenerate all previews when changing */
	const float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float scale[2];

	wmOrtho2(0.0f, size_x, 0.0f, size_y);
	/* center */
	glTranslatef(size_x * (1.0f - asp[0]) * 0.5f, size_y * (1.0f - asp[1]) * 0.5f, 0.0f);

	screen_preview_scale_get(screen, size_x, size_y, asp, scale);
	screen_preview_draw_areas(screen, scale, col, 1.5f);
}

/**
 * Render the preview for a screen layout in \a screen.
 */
void ED_screen_preview_render(const bScreen *screen, int size_x, int size_y, unsigned int *r_rect)
{
	char err_out[256] = "unknown";
	GPUOffScreen *offscreen = GPU_offscreen_create(size_x, size_y, 0, err_out);

	GPU_offscreen_bind(offscreen, true);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	screen_preview_draw(screen, size_x, size_y);

	GPU_offscreen_read_pixels(offscreen, GL_UNSIGNED_BYTE, r_rect);
	GPU_offscreen_unbind(offscreen, true);

	GPU_offscreen_free(offscreen);
}
