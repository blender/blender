//
//  Filename         : GLStrokeRenderer.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to define the GL stroke renderer.
//  Date of creation : 05/03/2003
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

#ifndef  GLSTROKERENDERER_H
# define GLSTROKERENDERER_H

# include "../system/FreestyleConfig.h"
# include "../stroke/StrokeRenderer.h"
# include "../stroke/StrokeRep.h"


# ifdef WIN32
#  include <windows.h>
# endif
# ifdef __MACH__
#  include <OpenGL/gl.h>
# else
#  include <GL/gl.h>
# endif

/**********************************/
/*                                */
/*                                */
/*         GLTextureManager       */
/*                                */
/*                                */
/**********************************/

/*! Class to load textures
 */
class LIB_RENDERING_EXPORT GLTextureManager : public TextureManager
{
 public:
  GLTextureManager ();
  virtual ~GLTextureManager ();
protected:
  virtual unsigned loadBrush(string fileName, Stroke::MediumType = Stroke::OPAQUE_MEDIUM);
  
 protected:
  virtual void loadPapers();
  virtual void loadStandardBrushes();
  bool prepareTextureAlpha (string name, GLuint itexname);
  bool prepareTextureLuminance (string name, GLuint itexname);
  bool prepareTextureLuminanceAndAlpha (string name, GLuint itexname);
  bool preparePaper (const char *name, GLuint itexname);
};



/**********************************/
/*                                */
/*                                */
/*         GLStrokeRenderer       */
/*                                */
/*                                */
/**********************************/

class LIB_RENDERING_EXPORT GLStrokeRenderer : public StrokeRenderer
{
public:
  GLStrokeRenderer();
  virtual ~GLStrokeRenderer();

  /*! Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

protected:
  //void renderNoTexture(StrokeRep *iStrokeRep) const;
};

#endif // GLSTROKERENDERER_H
