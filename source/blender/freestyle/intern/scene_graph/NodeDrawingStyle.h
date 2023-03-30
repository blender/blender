/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a Drawing Style to be applied to the underlying children. Inherits from
 * NodeGroup.
 */

#include "DrawingStyle.h"
#include "NodeGroup.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

class NodeDrawingStyle : public NodeGroup {
 public:
  inline NodeDrawingStyle() : NodeGroup() {}
  virtual ~NodeDrawingStyle() {}

  inline const DrawingStyle &drawingStyle() const
  {
    return _DrawingStyle;
  }

  inline void setDrawingStyle(const DrawingStyle &iDrawingStyle)
  {
    _DrawingStyle = iDrawingStyle;
  }

  /** Sets the style. Must be one of FILLED, LINES, POINTS, INVISIBLE. */
  inline void setStyle(const DrawingStyle::STYLE iStyle)
  {
    _DrawingStyle.setStyle(iStyle);
  }

  /** Sets the line width in the LINES style case */
  inline void setLineWidth(const float iLineWidth)
  {
    _DrawingStyle.setLineWidth(iLineWidth);
  }

  /** Sets the Point size in the POINTS style case */
  inline void setPointSize(const float iPointSize)
  {
    _DrawingStyle.setPointSize(iPointSize);
  }

  /** Enables or disables the lighting. true = enable */
  inline void setLightingEnabled(const bool iEnableLighting)
  {
    _DrawingStyle.setLightingEnabled(iEnableLighting);
  }

  /** Accept the corresponding visitor */
  virtual void accept(SceneVisitor &v);

  /** accessors */
  inline DrawingStyle::STYLE style() const
  {
    return _DrawingStyle.style();
  }

  inline float lineWidth() const
  {
    return _DrawingStyle.lineWidth();
  }

  inline float pointSize() const
  {
    return _DrawingStyle.pointSize();
  }

  inline bool lightingEnabled() const
  {
    return _DrawingStyle.lightingEnabled();
  }

 private:
  DrawingStyle _DrawingStyle;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:NodeDrawingStyle")
#endif
};

} /* namespace Freestyle */
