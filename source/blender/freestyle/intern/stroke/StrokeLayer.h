/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a layer of strokes.
 */

#include <deque>

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class Stroke;
class StrokeRenderer;
class StrokeLayer {
 public:
  typedef std::deque<Stroke *> stroke_container;

 protected:
  stroke_container _strokes;

 public:
  StrokeLayer() {}

  StrokeLayer(const stroke_container &iStrokes)
  {
    _strokes = iStrokes;
  }

  StrokeLayer(const StrokeLayer &iBrother)
  {
    _strokes = iBrother._strokes;
  }

  virtual ~StrokeLayer();

  /** Render method */
  void ScaleThickness(float iFactor);
  void Render(const StrokeRenderer *iRenderer);
  void RenderBasic(const StrokeRenderer *iRenderer);

  /** clears the layer */
  void clear();

  /** accessors */
  inline stroke_container::iterator strokes_begin()
  {
    return _strokes.begin();
  }

  inline stroke_container::iterator strokes_end()
  {
    return _strokes.end();
  }

  inline int strokes_size() const
  {
    return _strokes.size();
  }

  inline bool empty() const
  {
    return _strokes.empty();
  }

  /** modifiers */
  inline void setStrokes(stroke_container &iStrokes)
  {
    _strokes = iStrokes;
  }

  inline void AddStroke(Stroke *iStroke)
  {
    _strokes.push_back(iStroke);
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:StrokeLayer")
#endif
};

} /* namespace Freestyle */
