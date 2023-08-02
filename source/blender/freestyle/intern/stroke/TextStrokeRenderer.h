/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

//
//  Filename         : TextStrokeRenderer.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to define the text rendering of a stroke
//                     Format:
//                     x y width height // bbox
//                     //list of vertices :
//                     t x y z t1 t2 r g b alpha ...
//                      ...
//  Date of creation : 01/14/2005
//
///////////////////////////////////////////////////////////////////////////////

#ifndef TEXTSTROKERENDERER_H
#define TEXTSTROKERENDERER_H

#include <fstream>

#include "StrokeRenderer.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*         TextStrokeRenderer     */
/*                                */
/*                                */
/**********************************/

class TextStrokeRenderer : public StrokeRenderer {
 public:
  TextStrokeRenderer(const char *iFileName = nullptr);

  /** Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

 protected:
  mutable ofstream _ofstream;
};

} /* namespace Freestyle */

#endif  // TEXTSTROKERENDERER_H
