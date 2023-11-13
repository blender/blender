/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define the drawing style of a node
 */

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class DrawingStyle {
 public:
  enum STYLE {
    FILLED,
    LINES,
    POINTS,
    INVISIBLE,
  };

  inline DrawingStyle()
  {
    Style = FILLED;
    LineWidth = 2.0f;
    PointSize = 2.0f;
    LightingEnabled = true;
  }

  inline explicit DrawingStyle(const DrawingStyle &iBrother);

  virtual ~DrawingStyle() {}

  /** operators */
  inline DrawingStyle &operator=(const DrawingStyle &ds);

  inline void setStyle(const STYLE iStyle)
  {
    Style = iStyle;
  }

  inline void setLineWidth(const float iLineWidth)
  {
    LineWidth = iLineWidth;
  }

  inline void setPointSize(const float iPointSize)
  {
    PointSize = iPointSize;
  }

  inline void setLightingEnabled(const bool on)
  {
    LightingEnabled = on;
  }

  inline STYLE style() const
  {
    return Style;
  }

  inline float lineWidth() const
  {
    return LineWidth;
  }

  inline float pointSize() const
  {
    return PointSize;
  }

  inline bool lightingEnabled() const
  {
    return LightingEnabled;
  }

 private:
  STYLE Style;
  float LineWidth;
  float PointSize;
  bool LightingEnabled;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:DrawingStyle")
#endif
};

DrawingStyle::DrawingStyle(const DrawingStyle &iBrother)
{
  Style = iBrother.Style;
  LineWidth = iBrother.LineWidth;
  PointSize = iBrother.PointSize;
  LightingEnabled = iBrother.LightingEnabled;
}

DrawingStyle &DrawingStyle::operator=(const DrawingStyle &ds)
{
  Style = ds.Style;
  LineWidth = ds.LineWidth;
  PointSize = ds.PointSize;
  LightingEnabled = ds.LightingEnabled;

  return *this;
}

} /* namespace Freestyle */
