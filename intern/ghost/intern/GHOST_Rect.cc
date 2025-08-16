/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_Rect.hh"

void GHOST_Rect::inset(int32_t i)
{
  if (i > 0) {
    /* Grow the rectangle. */
    l_ -= i;
    r_ += i;
    t_ -= i;
    b_ += i;
  }
  else if (i < 0) {
    /* Shrink the rectangle, check for insets larger than half the size. */
    int32_t i2 = i * 2;
    if (getWidth() > i2) {
      l_ += i;
      r_ -= i;
    }
    else {
      l_ = l_ + ((r_ - l_) / 2);
      r_ = l_;
    }
    if (getHeight() > i2) {
      t_ += i;
      b_ -= i;
    }
    else {
      t_ = t_ + ((b_ - t_) / 2);
      b_ = t_;
    }
  }
}

GHOST_TVisibility GHOST_Rect::getVisibility(GHOST_Rect &r) const
{
  bool lt = isInside(r.l_, r.t_);
  bool rt = isInside(r.r_, r.t_);
  bool lb = isInside(r.l_, r.b_);
  bool rb = isInside(r.r_, r.b_);
  GHOST_TVisibility v;
  if (lt && rt && lb && rb) {
    /* All points inside, rectangle is inside this. */
    v = GHOST_kFullyVisible;
  }
  else if (!(lt || rt || lb || rb)) {
    /* None of the points inside.
     * Check to see whether the rectangle is larger than this one. */
    if ((r.l_ < l_) && (r.t_ < t_) && (r.r_ > r_) && (r.b_ > b_)) {
      v = GHOST_kPartiallyVisible;
    }
    else {
      v = GHOST_kNotVisible;
    }
  }
  else {
    /* Some of the points inside, rectangle is partially inside. */
    v = GHOST_kPartiallyVisible;
  }
  return v;
}

void GHOST_Rect::setCenter(int32_t cx, int32_t cy)
{
  int32_t offset = cx - (l_ + (r_ - l_) / 2);
  l_ += offset;
  r_ += offset;
  offset = cy - (t_ + (b_ - t_) / 2);
  t_ += offset;
  b_ += offset;
}

void GHOST_Rect::setCenter(int32_t cx, int32_t cy, int32_t w, int32_t h)
{
  long w_2, h_2;

  w_2 = w >> 1;
  h_2 = h >> 1;
  l_ = cx - w_2;
  t_ = cy - h_2;
  r_ = l_ + w;
  b_ = t_ + h;
}

bool GHOST_Rect::clip(GHOST_Rect &r) const
{
  bool clipped = false;
  if (r.l_ < l_) {
    r.l_ = l_;
    clipped = true;
  }
  if (r.t_ < t_) {
    r.t_ = t_;
    clipped = true;
  }
  if (r.r_ > r_) {
    r.r_ = r_;
    clipped = true;
  }
  if (r.b_ > b_) {
    r.b_ = b_;
    clipped = true;
  }
  return clipped;
}
