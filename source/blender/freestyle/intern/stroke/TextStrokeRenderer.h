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
 */

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
  TextStrokeRenderer(const char *iFileName = NULL);

  /*! Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

 protected:
  mutable ofstream _ofstream;
};

} /* namespace Freestyle */

#endif  // TEXTSTROKERENDERER_H
