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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 2007 Blender Foundation (refactor)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_subwindow.c
 *  \ingroup wm
 *
 * OpenGL utilities for setting up 2D viewport for window and regions.
 */

#include "BLI_math.h"
#include "BLI_rect.h"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BIF_gl.h"

#include "GPU_matrix.h"

#include "WM_api.h"

void wmViewport(const rcti *winrct)
{
	int width  = BLI_rcti_size_x(winrct) + 1;
	int height = BLI_rcti_size_y(winrct) + 1;

	glViewport(winrct->xmin, winrct->ymin, width, height);
	glScissor(winrct->xmin, winrct->ymin, width, height);

	wmOrtho2_pixelspace(width, height);
	GPU_matrix_identity_set();
}

void wmPartialViewport(rcti *drawrct, const rcti *winrct, const rcti *partialrct)
{
	/* Setup part of the viewport for partial redraw. */
	bool scissor_pad;

	if (partialrct->xmin == partialrct->xmax) {
		/* Full region. */
		*drawrct = *winrct;
		scissor_pad = true;
	}
	else {
		/* Partial redraw, clipped to region. */
		BLI_rcti_isect(winrct, partialrct, drawrct);
		scissor_pad = false;
	}

	int x = drawrct->xmin - winrct->xmin;
	int y = drawrct->ymin - winrct->ymin;
	int width  = BLI_rcti_size_x(winrct) + 1;
	int height = BLI_rcti_size_y(winrct) + 1;

	int scissor_width  = BLI_rcti_size_x(drawrct);
	int scissor_height = BLI_rcti_size_y(drawrct);

	/* Partial redraw rect uses different convention than region rect,
	 * so compensate for that here. One pixel offset is noticeable with
	 * viewport border render. */
	if (scissor_pad) {
		scissor_width  += 1;
		scissor_height += 1;
	}

	glViewport(0, 0, width, height);
	glScissor(x, y, scissor_width, scissor_height);

	wmOrtho2_pixelspace(width, height);
	GPU_matrix_identity_set();
}

void wmWindowViewport(wmWindow *win)
{
	int width = WM_window_pixels_x(win);
	int height = WM_window_pixels_y(win);

	glViewport(0, 0, width, height);
	glScissor(0, 0, width, height);

	wmOrtho2_pixelspace(width, height);
	GPU_matrix_identity_set();
}

void wmOrtho2(float x1, float x2, float y1, float y2)
{
	/* prevent opengl from generating errors */
	if (x1 == x2) x2 += 1.0f;
	if (y1 == y2) y2 += 1.0f;

	GPU_matrix_ortho_set(x1, x2, y1, y2, -100, 100);
}

static void wmOrtho2_offset(const float x, const float y, const float ofs)
{
	wmOrtho2(ofs, x + ofs, ofs, y + ofs);
}

/* Default pixel alignment for regions. */
void wmOrtho2_region_pixelspace(const ARegion *ar)
{
	wmOrtho2_offset(ar->winx, ar->winy, -0.01f);
}

void wmOrtho2_pixelspace(const float x, const float y)
{
	wmOrtho2_offset(x, y, -GLA_PIXEL_OFS);
}

void wmGetProjectionMatrix(float mat[4][4], const rcti *winrct)
{
	int width  = BLI_rcti_size_x(winrct) + 1;
	int height = BLI_rcti_size_y(winrct) + 1;
	orthographic_m4(mat, -GLA_PIXEL_OFS, (float)width - GLA_PIXEL_OFS, -GLA_PIXEL_OFS, (float)height - GLA_PIXEL_OFS, -100, 100);
}
