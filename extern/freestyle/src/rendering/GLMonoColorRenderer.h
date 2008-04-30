//
//  Filename         : GLMonoColorRenderer.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to render 3D scene in 2 colors thanks to OpenGL
//  Date of creation : 07/02/2002
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

#ifndef  GLMONOCOLORRENDERER_H
# define GLMONOCOLORRENDERER_H

# include "../system/FreestyleConfig.h"
# include "GLRenderer.h"

class LIB_RENDERING_EXPORT GLMonoColorRenderer : public GLRenderer
{
 public:

  GLMonoColorRenderer() : GLRenderer() {
    _r = _g = _b = 0.f;
    _alpha = 1.f;
  }

  virtual ~GLMonoColorRenderer() {}

  VISIT_DECL(DrawingStyle)
  VISIT_DECL(Material)

  void setColor(float r, float g, float b, float alpha = 1.f);

private:

  float _r;
  float _g;
  float _b;
  float _alpha;
};

#endif // GLMONOCOLORRENDERER_H
