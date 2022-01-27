/*
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
 */

/** \file
 * \ingroup render
 */

/* Global includes */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_camera_types.h"

#include "BKE_camera.h"

/* this module */
#include "pipeline.h"
#include "render_types.h"

/* ****************** MASKS and LUTS **************** */

static float filt_quadratic(float x)
{
  if (x < 0.0f) {
    x = -x;
  }
  if (x < 0.5f) {
    return 0.75f - (x * x);
  }
  if (x < 1.5f) {
    return 0.50f * (x - 1.5f) * (x - 1.5f);
  }
  return 0.0f;
}

static float filt_cubic(float x)
{
  float x2 = x * x;

  if (x < 0.0f) {
    x = -x;
  }

  if (x < 1.0f) {
    return 0.5f * x * x2 - x2 + 2.0f / 3.0f;
  }
  if (x < 2.0f) {
    return (2.0f - x) * (2.0f - x) * (2.0f - x) / 6.0f;
  }
  return 0.0f;
}

static float filt_catrom(float x)
{
  float x2 = x * x;

  if (x < 0.0f) {
    x = -x;
  }
  if (x < 1.0f) {
    return 1.5f * x2 * x - 2.5f * x2 + 1.0f;
  }
  if (x < 2.0f) {
    return -0.5f * x2 * x + 2.5f * x2 - 4.0f * x + 2.0f;
  }
  return 0.0f;
}

static float filt_mitchell(float x) /* Mitchell & Netravali's two-param cubic */
{
  float b = 1.0f / 3.0f, c = 1.0f / 3.0f;
  float p0 = (6.0f - 2.0f * b) / 6.0f;
  float p2 = (-18.0f + 12.0f * b + 6.0f * c) / 6.0f;
  float p3 = (12.0f - 9.0f * b - 6.0f * c) / 6.0f;
  float q0 = (8.0f * b + 24.0f * c) / 6.0f;
  float q1 = (-12.0f * b - 48.0f * c) / 6.0f;
  float q2 = (6.0f * b + 30.0f * c) / 6.0f;
  float q3 = (-b - 6.0f * c) / 6.0f;

  if (x < -2.0f) {
    return 0.0f;
  }
  if (x < -1.0f) {
    return (q0 - x * (q1 - x * (q2 - x * q3)));
  }
  if (x < 0.0f) {
    return (p0 + x * x * (p2 - x * p3));
  }
  if (x < 1.0f) {
    return (p0 + x * x * (p2 + x * p3));
  }
  if (x < 2.0f) {
    return (q0 + x * (q1 + x * (q2 + x * q3)));
  }
  return 0.0f;
}

float RE_filter_value(int type, float x)
{
  float gaussfac = 1.6f;

  x = fabsf(x);

  switch (type) {
    case R_FILTER_BOX:
      if (x > 1.0f) {
        return 0.0f;
      }
      return 1.0f;

    case R_FILTER_TENT:
      if (x > 1.0f) {
        return 0.0f;
      }
      return 1.0f - x;

    case R_FILTER_GAUSS: {
      const float two_gaussfac2 = 2.0f * gaussfac * gaussfac;
      x *= 3.0f * gaussfac;
      return 1.0f / sqrtf((float)M_PI * two_gaussfac2) * expf(-x * x / two_gaussfac2);
    }

    case R_FILTER_MITCH:
      return filt_mitchell(x * gaussfac);

    case R_FILTER_QUAD:
      return filt_quadratic(x * gaussfac);

    case R_FILTER_CUBIC:
      return filt_cubic(x * gaussfac);

    case R_FILTER_CATROM:
      return filt_catrom(x * gaussfac);
  }
  return 0.0f;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

struct Object *RE_GetCamera(Render *re)
{
  Object *camera = re->camera_override ? re->camera_override : re->scene->camera;
  return BKE_camera_multiview_render(re->scene, camera, re->viewname);
}

void RE_SetOverrideCamera(Render *re, Object *cam_ob)
{
  re->camera_override = cam_ob;
}

void RE_SetCamera(Render *re, const Object *cam_ob)
{
  CameraParams params;

  /* setup parameters */
  BKE_camera_params_init(&params);
  BKE_camera_params_from_object(&params, cam_ob);
  BKE_camera_multiview_params(&re->r, &params, cam_ob, re->viewname);

  /* Compute matrix, view-plane, etc. */
  BKE_camera_params_compute_viewplane(&params, re->winx, re->winy, re->r.xasp, re->r.yasp);
  BKE_camera_params_compute_matrix(&params);

  /* extract results */
  copy_m4_m4(re->winmat, params.winmat);
  re->clip_start = params.clip_start;
  re->clip_end = params.clip_end;
  re->viewplane = params.viewplane;
}

void RE_GetCameraWindow(struct Render *re, const struct Object *camera, float r_winmat[4][4])
{
  RE_SetCamera(re, camera);
  copy_m4_m4(r_winmat, re->winmat);
}

void RE_GetCameraWindowWithOverscan(const struct Render *re, float overscan, float r_winmat[4][4])
{
  CameraParams params;
  params.is_ortho = re->winmat[3][3] != 0.0f;
  params.clip_start = re->clip_start;
  params.clip_end = re->clip_end;
  params.viewplane = re->viewplane;

  overscan *= max_ff(BLI_rctf_size_x(&params.viewplane), BLI_rctf_size_y(&params.viewplane));

  params.viewplane.xmin -= overscan;
  params.viewplane.xmax += overscan;
  params.viewplane.ymin -= overscan;
  params.viewplane.ymax += overscan;
  BKE_camera_params_compute_matrix(&params);
  copy_m4_m4(r_winmat, params.winmat);
}

void RE_GetCameraModelMatrix(const Render *re, const struct Object *camera, float r_modelmat[4][4])
{
  BKE_camera_multiview_model_matrix(&re->r, camera, re->viewname, r_modelmat);
}

void RE_GetViewPlane(Render *re, rctf *r_viewplane, rcti *r_disprect)
{
  *r_viewplane = re->viewplane;

  /* make disprect zero when no border render, is needed to detect changes in 3d view render */
  if (re->r.mode & R_BORDER) {
    *r_disprect = re->disprect;
  }
  else {
    BLI_rcti_init(r_disprect, 0, 0, 0, 0);
  }
}
