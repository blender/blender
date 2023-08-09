/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cmath>
#include <cstdlib>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "transform.hh"
#include "transform_mode.hh"

#include "MEM_guardedalloc.h"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Callbacks for #MouseInput.apply
 * \{ */

/** Callback for #INPUT_VECTOR */
static void InputVector(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
  convertViewVec(t, output, mval[0] - mi->imval[0], mval[1] - mi->imval[1]);
}

/** Callback for #INPUT_SPRING */
static void InputSpring(TransInfo * /*t*/, MouseInput *mi, const double mval[2], float output[3])
{
  double dx, dy;
  float ratio;

  dx = double(mi->center[0]) - mval[0];
  dy = double(mi->center[1]) - mval[1];
  ratio = hypot(dx, dy) / double(mi->factor);

  output[0] = ratio;
}

/** Callback for #INPUT_SPRING_FLIP */
static void InputSpringFlip(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
  InputSpring(t, mi, mval, output);

  /* flip scale */
  /* values can become really big when zoomed in so use longs #26598. */
  if ((int64_t(int(mi->center[0]) - mval[0]) * int64_t(int(mi->center[0]) - mi->imval[0]) +
       int64_t(int(mi->center[1]) - mval[1]) * int64_t(int(mi->center[1]) - mi->imval[1])) < 0)
  {
    output[0] *= -1.0f;
  }
}

/** Callback for #INPUT_SPRING_DELTA */
static void InputSpringDelta(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
  InputSpring(t, mi, mval, output);
  output[0] -= 1.0f;
}

/** Callback for #INPUT_TRACKBALL */
static void InputTrackBall(TransInfo * /*t*/,
                           MouseInput *mi,
                           const double mval[2],
                           float output[3])
{
  output[0] = float(mi->imval[1] - mval[1]);
  output[1] = float(mval[0] - mi->imval[0]);

  output[0] *= mi->factor;
  output[1] *= mi->factor;
}

/** Callback for #INPUT_HORIZONTAL_RATIO */
static void InputHorizontalRatio(TransInfo *t,
                                 MouseInput *mi,
                                 const double mval[2],
                                 float output[3])
{
  const int winx = t->region ? t->region->winx : 1;

  output[0] = ((mval[0] - mi->imval[0]) / winx) * 2.0f;
}

/** Callback for #INPUT_HORIZONTAL_ABSOLUTE */
static void InputHorizontalAbsolute(TransInfo *t,
                                    MouseInput *mi,
                                    const double mval[2],
                                    float output[3])
{
  float vec[3];

  InputVector(t, mi, mval, vec);
  project_v3_v3v3(vec, vec, t->viewinv[0]);

  output[0] = dot_v3v3(t->viewinv[0], vec) * 2.0f;
}

static void InputVerticalRatio(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
  const int winy = t->region ? t->region->winy : 1;

  /* Dragging up increases (matching viewport zoom). */
  output[0] = ((mval[1] - mi->imval[1]) / winy) * 2.0f;
}

/** Callback for #INPUT_VERTICAL_ABSOLUTE */
static void InputVerticalAbsolute(TransInfo *t,
                                  MouseInput *mi,
                                  const double mval[2],
                                  float output[3])
{
  float vec[3];

  InputVector(t, mi, mval, vec);
  project_v3_v3v3(vec, vec, t->viewinv[1]);

  /* Dragging up increases (matching viewport zoom). */
  output[0] = dot_v3v3(t->viewinv[1], vec) * 2.0f;
}

/** Callback for #INPUT_CUSTOM_RATIO_FLIP */
static void InputCustomRatioFlip(TransInfo * /*t*/,
                                 MouseInput *mi,
                                 const double mval[2],
                                 float output[3])
{
  double length;
  double distance;
  double dx, dy;
  const int *data = static_cast<const int *>(mi->data);

  if (data) {
    int mdx, mdy;
    dx = data[2] - data[0];
    dy = data[3] - data[1];

    length = hypot(dx, dy);

    mdx = mval[0] - data[2];
    mdy = mval[1] - data[3];

    distance = (length != 0.0) ? (mdx * dx + mdy * dy) / length : 0.0;

    output[0] = (length != 0.0) ? double(distance / length) : 0.0;
  }
}

/** Callback for #INPUT_CUSTOM_RATIO */
static void InputCustomRatio(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
  InputCustomRatioFlip(t, mi, mval, output);
  output[0] = -output[0];
}

struct InputAngle_Data {
  double angle;
  double mval_prev[2];
};

/** Callback for #INPUT_ANGLE */
static void InputAngle(TransInfo * /*t*/, MouseInput *mi, const double mval[2], float output[3])
{
  InputAngle_Data *data = static_cast<InputAngle_Data *>(mi->data);
  float dir_prev[2], dir_curr[2], mi_center[2];
  copy_v2_v2(mi_center, mi->center);

  sub_v2_v2v2(
      dir_prev, blender::float2{float(data->mval_prev[0]), float(data->mval_prev[1])}, mi_center);
  sub_v2_v2v2(dir_curr, blender::float2{float(mval[0]), float(mval[1])}, mi_center);

  if (normalize_v2(dir_prev) && normalize_v2(dir_curr)) {
    float dphi = angle_normalized_v2v2(dir_prev, dir_curr);

    if (cross_v2v2(dir_prev, dir_curr) > 0.0f) {
      dphi = -dphi;
    }

    data->angle += double(dphi) * (mi->precision ? double(mi->precision_factor) : 1.0);

    data->mval_prev[0] = mval[0];
    data->mval_prev[1] = mval[1];
  }

  output[0] = data->angle;
}

static void InputAngleSpring(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
  float toutput[3];

  InputAngle(t, mi, mval, output);
  InputSpring(t, mi, mval, toutput);

  output[1] = toutput[0];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom 2D Start/End Coordinate API
 *
 * - #INPUT_CUSTOM_RATIO
 * - #INPUT_CUSTOM_RATIO_FLIP
 * \{ */

void setCustomPoints(TransInfo * /*t*/,
                     MouseInput *mi,
                     const int mval_start[2],
                     const int mval_end[2])
{
  int *data;

  mi->data = MEM_reallocN(mi->data, sizeof(int[4]));

  data = static_cast<int *>(mi->data);

  data[0] = mval_start[0];
  data[1] = mval_start[1];
  data[2] = mval_end[0];
  data[3] = mval_end[1];
}

void setCustomPointsFromDirection(TransInfo *t, MouseInput *mi, const float2 &dir)
{
  BLI_ASSERT_UNIT_V2(dir);
  const int win_axis =
      t->region ? ((abs(int(t->region->winx * dir[0])) + abs(int(t->region->winy * dir[1]))) / 2) :
                  1;
  const int2 mval_start = int2(mi->imval + dir * win_axis);
  const int2 mval_end = int2(mi->imval);
  setCustomPoints(t, mi, mval_start, mval_end);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Setup & Handle Mouse Input
 * \{ */

void transform_input_reset(TransInfo *t, const float2 &mval)
{
  MouseInput *mi = &t->mouse;

  mi->imval = mval;

  if (mi->data && ELEM(mi->apply, InputAngle, InputAngleSpring)) {
    InputAngle_Data *data = static_cast<InputAngle_Data *>(mi->data);
    data->mval_prev[0] = mi->imval[0];
    data->mval_prev[1] = mi->imval[1];
    data->angle = 0.0f;
  }
}

void initMouseInput(
    TransInfo *t, MouseInput *mi, const float2 &center, const float2 &mval, const bool precision)
{
  mi->factor = 0;
  mi->precision = precision;

  mi->center = center;

  mi->post = nullptr;

  transform_input_reset(t, mval);
}

static void calcSpringFactor(MouseInput *mi)
{
  float mdir[2] = {float(mi->center[1] - mi->imval[1]), float(mi->center[0] - mi->imval[0])};

  mi->factor = len_v2(mdir);

  if (mi->factor == 0.0f) {
    mi->factor = 1.0f; /* prevent Inf */
  }
}

void initMouseInputMode(TransInfo *t, MouseInput *mi, MouseInputMode mode)
{
  /* In case we allocate a new value. */
  void *mi_data_prev = mi->data;

  mi->use_virtual_mval = true;
  mi->precision_factor = 1.0f / 10.0f;

  switch (mode) {
    case INPUT_VECTOR:
      mi->apply = InputVector;
      t->helpline = HLP_NONE;
      break;
    case INPUT_SPRING:
      calcSpringFactor(mi);
      mi->apply = InputSpring;
      t->helpline = HLP_SPRING;
      break;
    case INPUT_SPRING_FLIP:
      calcSpringFactor(mi);
      mi->apply = InputSpringFlip;
      t->helpline = HLP_SPRING;
      break;
    case INPUT_SPRING_DELTA:
      calcSpringFactor(mi);
      mi->apply = InputSpringDelta;
      t->helpline = HLP_SPRING;
      break;
    case INPUT_ANGLE:
    case INPUT_ANGLE_SPRING: {
      InputAngle_Data *data;
      mi->use_virtual_mval = false;
      mi->precision_factor = 1.0f / 30.0f;
      data = static_cast<InputAngle_Data *>(
          MEM_callocN(sizeof(InputAngle_Data), "angle accumulator"));
      data->mval_prev[0] = mi->imval[0];
      data->mval_prev[1] = mi->imval[1];
      mi->data = data;
      if (mode == INPUT_ANGLE) {
        mi->apply = InputAngle;
      }
      else {
        calcSpringFactor(mi);
        mi->apply = InputAngleSpring;
      }
      t->helpline = HLP_ANGLE;
      break;
    }
    case INPUT_TRACKBALL:
      mi->precision_factor = 1.0f / 30.0f;
      /* factor has to become setting or so */
      mi->factor = 0.01f;
      mi->apply = InputTrackBall;
      t->helpline = HLP_TRACKBALL;
      break;
    case INPUT_HORIZONTAL_RATIO:
      mi->apply = InputHorizontalRatio;
      t->helpline = HLP_HARROW;
      break;
    case INPUT_HORIZONTAL_ABSOLUTE:
      mi->apply = InputHorizontalAbsolute;
      t->helpline = HLP_HARROW;
      break;
    case INPUT_VERTICAL_RATIO:
      mi->apply = InputVerticalRatio;
      t->helpline = HLP_VARROW;
      break;
    case INPUT_VERTICAL_ABSOLUTE:
      mi->apply = InputVerticalAbsolute;
      t->helpline = HLP_VARROW;
      break;
    case INPUT_CUSTOM_RATIO:
      mi->apply = InputCustomRatio;
      t->helpline = HLP_CARROW;
      break;
    case INPUT_CUSTOM_RATIO_FLIP:
      mi->apply = InputCustomRatioFlip;
      t->helpline = HLP_CARROW;
      break;
    case INPUT_NONE:
    default:
      mi->apply = nullptr;
      break;
  }

  /* setup for the mouse cursor: either set a custom one,
   * or hide it if it will be drawn with the helpline */
  wmWindow *win = CTX_wm_window(t->context);
  switch (t->helpline) {
    case HLP_NONE:
      /* INPUT_VECTOR, INPUT_CUSTOM_RATIO, INPUT_CUSTOM_RATIO_FLIP */
      if (t->flag & T_MODAL) {
        t->flag |= T_MODAL_CURSOR_SET;
        WM_cursor_modal_set(win, WM_CURSOR_NSEW_SCROLL);
      }
      break;
    case HLP_SPRING:
    case HLP_ANGLE:
    case HLP_TRACKBALL:
    case HLP_HARROW:
    case HLP_VARROW:
    case HLP_CARROW:
      if (t->flag & T_MODAL) {
        t->flag |= T_MODAL_CURSOR_SET;
        WM_cursor_modal_set(win, WM_CURSOR_NONE);
      }
      break;
    default:
      break;
  }

  /* if we've allocated new data, free the old data
   * less hassle than checking before every alloc above */
  if (mi_data_prev && (mi_data_prev != mi->data)) {
    MEM_freeN(mi_data_prev);
  }
}

void setInputPostFct(MouseInput *mi, void (*post)(TransInfo *t, float values[3]))
{
  mi->post = post;
}

void applyMouseInput(TransInfo *t, MouseInput *mi, const float2 &mval, float output[3])
{
  double mval_db[2];

  if (mi->use_virtual_mval) {
    /* update accumulator */
    double mval_delta[2];

    mval_delta[0] = (mval[0] - mi->imval[0]) - mi->virtual_mval.prev[0];
    mval_delta[1] = (mval[1] - mi->imval[1]) - mi->virtual_mval.prev[1];

    mi->virtual_mval.prev[0] += mval_delta[0];
    mi->virtual_mval.prev[1] += mval_delta[1];

    if (mi->precision) {
      mval_delta[0] *= double(mi->precision_factor);
      mval_delta[1] *= double(mi->precision_factor);
    }

    mi->virtual_mval.accum[0] += mval_delta[0];
    mi->virtual_mval.accum[1] += mval_delta[1];

    mval_db[0] = mi->imval[0] + mi->virtual_mval.accum[0];
    mval_db[1] = mi->imval[1] + mi->virtual_mval.accum[1];
  }
  else {
    mval_db[0] = mval[0];
    mval_db[1] = mval[1];
  }

  if (mi->apply != nullptr) {
    mi->apply(t, mi, mval_db, output);
  }

  if (mi->post) {
    mi->post(t, output);
  }
}

void transform_input_update(TransInfo *t, const float fac)
{
  MouseInput *mi = &t->mouse;
  float2 offset = fac * (mi->imval - mi->center);
  mi->imval = t->center2d + offset;
  mi->factor *= fac;

  float center_old[2];
  copy_v2_v2(center_old, mi->center);
  copy_v2_v2(mi->center, t->center2d);

  if (mi->use_virtual_mval) {
    /* Update accumulator. */
    double mval_delta[2];
    sub_v2_v2v2_db(mval_delta, mi->virtual_mval.accum, mi->virtual_mval.prev);
    mval_delta[0] *= fac;
    mval_delta[1] *= fac;
    copy_v2_v2_db(mi->virtual_mval.accum, mi->virtual_mval.prev);
    add_v2_v2_db(mi->virtual_mval.accum, mval_delta);
  }

  if (ELEM(mi->apply, InputAngle, InputAngleSpring)) {
    float offset_center[2];
    sub_v2_v2v2(offset_center, mi->center, center_old);
    InputAngle_Data *data = static_cast<InputAngle_Data *>(mi->data);
    data->mval_prev[0] += offset_center[0];
    data->mval_prev[1] += offset_center[1];
  }

  if (t->mode == TFM_EDGE_SLIDE) {
    transform_mode_edge_slide_reproject_input(t);
  }
  else if (t->mode == TFM_VERT_SLIDE) {
    transform_mode_vert_slide_reproject_input(t);
  }
}

void transform_input_virtual_mval_reset(TransInfo *t)
{
  MouseInput *mi = &t->mouse;
  if (ELEM(mi->apply, InputAngle, InputAngleSpring)) {
    InputAngle_Data *data = static_cast<InputAngle_Data *>(mi->data);
    data->angle = 0.0;
    data->mval_prev[0] = mi->imval[0];
    data->mval_prev[1] = mi->imval[1];
  }
  else {
    memset(&mi->virtual_mval, 0, sizeof(mi->virtual_mval));
  }
}

/** \} */
