//
//  Filename         : PSStrokeRenderer.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to define the Postscript rendering of a stroke
//  Date of creation : 10/26/2004
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

#ifndef  PSSTROKERENDERER_H
# define PSSTROKERENDERER_H

# include "StrokeRenderer.h"
# include "../system/FreestyleConfig.h"
# include <fstream>

/**********************************/
/*                                */
/*                                */
/*         PSStrokeRenderer       */
/*                                */
/*                                */
/**********************************/

class LIB_STROKE_EXPORT PSStrokeRenderer : public StrokeRenderer
{
public:
  PSStrokeRenderer(const char * iFileName = 0);
  virtual ~PSStrokeRenderer();

  /*! Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

  /*! Closes the output PS file */
  void Close();

protected:
  mutable ofstream _ofstream;
};

#endif // PSSTROKERENDERER_H

