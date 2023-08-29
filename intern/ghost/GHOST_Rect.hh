/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Macro's used in GHOST debug target.
 */

#pragma once

#include "GHOST_Types.h"

/**
 * Implements rectangle functionality.
 * The four extreme coordinates are stored as left, top, right and bottom.
 * To be valid, a rectangle should have a left coordinate smaller than or equal to right.
 * To be valid, a rectangle should have a top coordinate smaller than or equal to bottom.
 */
class GHOST_Rect {
 public:
  /**
   * Constructs a rectangle with the given values.
   * \param l: requested left coordinate of the rectangle.
   * \param t: requested top coordinate of the rectangle.
   * \param r: requested right coordinate of the rectangle.
   * \param b: requested bottom coordinate of the rectangle.
   */
  GHOST_Rect(int32_t l = 0, int32_t t = 0, int32_t r = 0, int32_t b = 0)
      : m_l(l), m_t(t), m_r(r), m_b(b)
  {
  }

  /**
   * Destructor.
   */
  virtual ~GHOST_Rect() {}

  /**
   * Access to rectangle width.
   * \return width of the rectangle.
   */
  virtual inline int32_t getWidth() const;

  /**
   * Access to rectangle height.
   * \return height of the rectangle.
   */
  virtual inline int32_t getHeight() const;

  /**
   * Sets all members of the rectangle.
   * \param l: requested left coordinate of the rectangle.
   * \param t: requested top coordinate of the rectangle.
   * \param r: requested right coordinate of the rectangle.
   * \param b: requested bottom coordinate of the rectangle.
   */
  virtual inline void set(int32_t l, int32_t t, int32_t r, int32_t b);

  /**
   * Returns whether this rectangle is empty.
   * Empty rectangles are rectangles that have width==0 and/or height==0.
   * \return boolean value (true==empty rectangle)
   */
  virtual inline bool isEmpty() const;

  /**
   * Returns whether this rectangle is valid.
   * Valid rectangles are rectangles that have m_l <= m_r and m_t <= m_b.
   * Thus, empty rectangles are valid.
   * \return boolean value (true==valid rectangle)
   */
  virtual inline bool isValid() const;

  /**
   * Grows (or shrinks the rectangle).
   * The method avoids negative insets making the rectangle invalid
   * \param i: The amount of offset given to each extreme (negative values shrink the rectangle).
   */
  virtual void inset(int32_t i);

  /**
   * Does a union of the rectangle given and this rectangle.
   * The result is stored in this rectangle.
   * \param r: The rectangle that is input for the union operation.
   */
  virtual inline void unionRect(const GHOST_Rect &r);

  /**
   * Grows the rectangle to included a point.
   * \param x: The x-coordinate of the point.
   * \param y: The y-coordinate of the point.
   */
  virtual inline void unionPoint(int32_t x, int32_t y);

  /**
   * Grows the rectangle to included a point.
   * \param x: The x-coordinate of the point.
   * \param y: The y-coordinate of the point.
   */
  virtual inline void wrapPoint(int32_t &x, int32_t &y, int32_t ofs, GHOST_TAxisFlag axis);
  /**
   * Confine x & y within the rectangle (inclusive).
   * \param x: The x-coordinate of the point.
   * \param y: The y-coordinate of the point.
   */
  virtual inline void clampPoint(int32_t &x, int32_t &y);

  /**
   * Returns whether the point is inside this rectangle.
   * Point on the boundary is considered inside.
   * \param x: x-coordinate of point to test.
   * \param y: y-coordinate of point to test.
   * \return boolean value (true if point is inside).
   */
  virtual inline bool isInside(int32_t x, int32_t y) const;

  /**
   * Returns whether the rectangle is inside this rectangle.
   * \param r: rectangle to test.
   * \return visibility (not, partially or fully visible).
   */
  virtual GHOST_TVisibility getVisibility(GHOST_Rect &r) const;

  /**
   * Sets rectangle members.
   * Sets rectangle members such that it is centered at the given location.
   * \param cx: requested center x-coordinate of the rectangle.
   * \param cy: requested center y-coordinate of the rectangle.
   */
  virtual void setCenter(int32_t cx, int32_t cy);

  /**
   * Sets rectangle members.
   * Sets rectangle members such that it is centered at the given location,
   * with the width requested.
   * \param cx: requested center x-coordinate of the rectangle.
   * \param cy: requested center y-coordinate of the rectangle.
   * \param w: requested width of the rectangle.
   * \param h: requested height of the rectangle.
   */
  virtual void setCenter(int32_t cx, int32_t cy, int32_t w, int32_t h);

  /**
   * Clips a rectangle.
   * Updates the rectangle given such that it will fit within this one.
   * This can result in an empty rectangle.
   * \param r: the rectangle to clip.
   * \return whether clipping has occurred
   */
  virtual bool clip(GHOST_Rect &r) const;

  /** Left coordinate of the rectangle */
  int32_t m_l;
  /** Top coordinate of the rectangle */
  int32_t m_t;
  /** Right coordinate of the rectangle */
  int32_t m_r;
  /** Bottom coordinate of the rectangle */
  int32_t m_b;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_Rect")
#endif
};

inline int32_t GHOST_Rect::getWidth() const
{
  return m_r - m_l;
}

inline int32_t GHOST_Rect::getHeight() const
{
  return m_b - m_t;
}

inline void GHOST_Rect::set(int32_t l, int32_t t, int32_t r, int32_t b)
{
  m_l = l;
  m_t = t;
  m_r = r;
  m_b = b;
}

inline bool GHOST_Rect::isEmpty() const
{
  return (getWidth() == 0) || (getHeight() == 0);
}

inline bool GHOST_Rect::isValid() const
{
  return (m_l <= m_r) && (m_t <= m_b);
}

inline void GHOST_Rect::unionRect(const GHOST_Rect &r)
{
  if (r.m_l < m_l) {
    m_l = r.m_l;
  }
  if (r.m_r > m_r) {
    m_r = r.m_r;
  }
  if (r.m_t < m_t) {
    m_t = r.m_t;
  }
  if (r.m_b > m_b) {
    m_b = r.m_b;
  }
}

inline void GHOST_Rect::unionPoint(int32_t x, int32_t y)
{
  if (x < m_l) {
    m_l = x;
  }
  if (x > m_r) {
    m_r = x;
  }
  if (y < m_t) {
    m_t = y;
  }
  if (y > m_b) {
    m_b = y;
  }
}

inline void GHOST_Rect::wrapPoint(int32_t &x, int32_t &y, int32_t ofs, GHOST_TAxisFlag axis)
{
  int32_t w = getWidth();
  int32_t h = getHeight();

  /* highly unlikely but avoid eternal loop */
  if (w - ofs * 2 <= 0 || h - ofs * 2 <= 0) {
    return;
  }

  if (axis & GHOST_kAxisX) {
    while (x - ofs < m_l) {
      x += w - (ofs * 2);
    }
    while (x + ofs > m_r) {
      x -= w - (ofs * 2);
    }
  }
  if (axis & GHOST_kAxisY) {
    while (y - ofs < m_t) {
      y += h - (ofs * 2);
    }
    while (y + ofs > m_b) {
      y -= h - (ofs * 2);
    }
  }
}

inline void GHOST_Rect::clampPoint(int32_t &x, int32_t &y)
{
  if (x < m_l) {
    x = m_l;
  }
  else if (x > m_r) {
    x = m_r;
  }

  if (y < m_t) {
    y = m_t;
  }
  else if (y > m_b) {
    y = m_b;
  }
}

inline bool GHOST_Rect::isInside(int32_t x, int32_t y) const
{
  return (x >= m_l) && (x <= m_r) && (y >= m_t) && (y <= m_b);
}
