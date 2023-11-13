/* SPDX-FileCopyrightText: 2010-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a layer of strokes.
 */

#include "StrokeLayer.h"
#include "Canvas.h"
#include "Stroke.h"

namespace Freestyle {

StrokeLayer::~StrokeLayer()
{
  clear();
}

void StrokeLayer::ScaleThickness(float iFactor)
{
  for (StrokeLayer::stroke_container::iterator s = _strokes.begin(), send = _strokes.end();
       s != send;
       ++s)
  {
    (*s)->ScaleThickness(iFactor);
  }
}

void StrokeLayer::Render(const StrokeRenderer *iRenderer)
{
  for (StrokeLayer::stroke_container::iterator s = _strokes.begin(), send = _strokes.end();
       s != send;
       ++s)
  {
    (*s)->Render(iRenderer);
  }
}

void StrokeLayer::RenderBasic(const StrokeRenderer *iRenderer)
{
  for (StrokeLayer::stroke_container::iterator s = _strokes.begin(), send = _strokes.end();
       s != send;
       ++s)
  {
    (*s)->RenderBasic(iRenderer);
  }
}

void StrokeLayer::clear()
{
  for (stroke_container::iterator s = _strokes.begin(), send = _strokes.end(); s != send; ++s) {
    delete *s;
  }
  _strokes.clear();
}

} /* namespace Freestyle */
