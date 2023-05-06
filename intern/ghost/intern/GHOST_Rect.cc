/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_Rect.hh"

void GHOST_Rect::inset(int32_t i)
{
  if (i > 0) {
    /* Grow the rectangle. */
    m_l -= i;
    m_r += i;
    m_t -= i;
    m_b += i;
  }
  else if (i < 0) {
    /* Shrink the rectangle, check for insets larger than half the size. */
    int32_t i2 = i * 2;
    if (getWidth() > i2) {
      m_l += i;
      m_r -= i;
    }
    else {
      m_l = m_l + ((m_r - m_l) / 2);
      m_r = m_l;
    }
    if (getHeight() > i2) {
      m_t += i;
      m_b -= i;
    }
    else {
      m_t = m_t + ((m_b - m_t) / 2);
      m_b = m_t;
    }
  }
}

GHOST_TVisibility GHOST_Rect::getVisibility(GHOST_Rect &r) const
{
  bool lt = isInside(r.m_l, r.m_t);
  bool rt = isInside(r.m_r, r.m_t);
  bool lb = isInside(r.m_l, r.m_b);
  bool rb = isInside(r.m_r, r.m_b);
  GHOST_TVisibility v;
  if (lt && rt && lb && rb) {
    /* All points inside, rectangle is inside this. */
    v = GHOST_kFullyVisible;
  }
  else if (!(lt || rt || lb || rb)) {
    /* None of the points inside.
     * Check to see whether the rectangle is larger than this one. */
    if ((r.m_l < m_l) && (r.m_t < m_t) && (r.m_r > m_r) && (r.m_b > m_b)) {
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
  int32_t offset = cx - (m_l + (m_r - m_l) / 2);
  m_l += offset;
  m_r += offset;
  offset = cy - (m_t + (m_b - m_t) / 2);
  m_t += offset;
  m_b += offset;
}

void GHOST_Rect::setCenter(int32_t cx, int32_t cy, int32_t w, int32_t h)
{
  long w_2, h_2;

  w_2 = w >> 1;
  h_2 = h >> 1;
  m_l = cx - w_2;
  m_t = cy - h_2;
  m_r = m_l + w;
  m_b = m_t + h;
}

bool GHOST_Rect::clip(GHOST_Rect &r) const
{
  bool clipped = false;
  if (r.m_l < m_l) {
    r.m_l = m_l;
    clipped = true;
  }
  if (r.m_t < m_t) {
    r.m_t = m_t;
    clipped = true;
  }
  if (r.m_r > m_r) {
    r.m_r = m_r;
    clipped = true;
  }
  if (r.m_b > m_b) {
    r.m_b = m_b;
    clipped = true;
  }
  return clipped;
}
