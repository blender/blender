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


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef  TEXTSTROKERENDERER_H
# define TEXTSTROKERENDERER_H

# include "StrokeRenderer.h"
# include "../system/FreestyleConfig.h"
# include <fstream>

/**********************************/
/*                                */
/*                                */
/*         TextStrokeRenderer     */
/*                                */
/*                                */
/**********************************/

class LIB_STROKE_EXPORT TextStrokeRenderer : public StrokeRenderer
{
public:
  TextStrokeRenderer(const char * iFileName = 0);
  virtual ~TextStrokeRenderer();

  /*! Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

  /*! Closes the output file */
  void Close();

protected:
  mutable ofstream _ofstream;
};

#endif // TEXTSTROKERENDERER_H

