/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * OpenGL utilities for setting up 2D viewport for window and regions.
 */

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "GPU_matrix.h"
#include "GPU_viewport.h"

#include "WM_api.hh"

void wmViewport(const rcti *winrct)
{
  int width = BLI_rcti_size_x(winrct) + 1;
  int height = BLI_rcti_size_y(winrct) + 1;

  GPU_viewport(winrct->xmin, winrct->ymin, width, height);
  GPU_scissor(winrct->xmin, winrct->ymin, width, height);

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
  int width = BLI_rcti_size_x(winrct) + 1;
  int height = BLI_rcti_size_y(winrct) + 1;

  int scissor_width = BLI_rcti_size_x(drawrct);
  int scissor_height = BLI_rcti_size_y(drawrct);

  /* Partial redraw rect uses different convention than region rect,
   * so compensate for that here. One pixel offset is noticeable with
   * viewport border render. */
  if (scissor_pad) {
    scissor_width += 1;
    scissor_height += 1;
  }

  GPU_viewport(0, 0, width, height);
  GPU_scissor(x, y, scissor_width, scissor_height);

  wmOrtho2_pixelspace(width, height);
  GPU_matrix_identity_set();
}

void wmWindowViewport(wmWindow *win)
{
  int width = WM_window_pixels_x(win);
  int height = WM_window_pixels_y(win);

  GPU_viewport(0, 0, width, height);
  GPU_scissor(0, 0, width, height);

  wmOrtho2_pixelspace(width, height);
  GPU_matrix_identity_set();
}

void wmOrtho2(float x1, float x2, float y1, float y2)
{
  /* Prevent opengl from generating errors. */
  if (x2 == x1) {
    x2 += 1.0f;
  }
  if (y2 == y1) {
    y2 += 1.0f;
  }

  GPU_matrix_ortho_set(
      x1, x2, y1, y2, GPU_MATRIX_ORTHO_CLIP_NEAR_DEFAULT, GPU_MATRIX_ORTHO_CLIP_FAR_DEFAULT);
}

static void wmOrtho2_offset(const float x, const float y, const float ofs)
{
  wmOrtho2(ofs, x + ofs, ofs, y + ofs);
}

void wmOrtho2_region_pixelspace(const ARegion *region)
{
  wmOrtho2_offset(region->winx, region->winy, -0.01f);
}

void wmOrtho2_pixelspace(const float x, const float y)
{
  wmOrtho2_offset(x, y, -GLA_PIXEL_OFS);
}

void wmGetProjectionMatrix(float mat[4][4], const rcti *winrct)
{
  int width = BLI_rcti_size_x(winrct) + 1;
  int height = BLI_rcti_size_y(winrct) + 1;
  orthographic_m4(mat,
                  -GLA_PIXEL_OFS,
                  float(width) - GLA_PIXEL_OFS,
                  -GLA_PIXEL_OFS,
                  float(height) - GLA_PIXEL_OFS,
                  GPU_MATRIX_ORTHO_CLIP_NEAR_DEFAULT,
                  GPU_MATRIX_ORTHO_CLIP_FAR_DEFAULT);
}
