//
//  Filename         : StrokeRenderer.h
//  Author(s)        : Fredo Durand
//  Purpose          : Classes to render a stroke with OpenGL
//  Date of creation : 09/09/2002
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

#ifndef  STROKE_RENDERER_H
# define STROKE_RENDERER_H

# include "Stroke.h"
# include <vector>
# include <map>
# include <utility>
# include "../system/FreestyleConfig.h"
# include "StrokeRep.h"
# include <string.h>


/**********************************/
/*                                */
/*                                */
/*         TextureManager         */
/*                                */
/*                                */
/**********************************/


/*! Class to load textures
 */
class LIB_STROKE_EXPORT TextureManager
{
public:

  TextureManager ();
  virtual ~TextureManager ();
  static TextureManager * getInstance() {return _pInstance;}
  void load () ;
  unsigned getBrushTextureIndex(string name, Stroke::MediumType iType = Stroke::OPAQUE_MEDIUM) ;

  inline bool hasLoaded() const {return _hasLoadedTextures;}
  inline unsigned int getDefaultTextureId() const {return _defaultTextureId;}

  struct LIB_STROKE_EXPORT Options
  {

    static void setPatternsPath(const string& path);
    static string getPatternsPath();

    static void setBrushesPath(const string& path);
    static string getBrushesPath();
  };

 protected:
  virtual void loadStandardBrushes() = 0;
  virtual unsigned loadBrush(string fileName, Stroke::MediumType = Stroke::OPAQUE_MEDIUM) = 0;
  
  typedef std::pair<string,Stroke::MediumType> BrushTexture;
  struct cmpBrushTexture{
    bool operator()(const BrushTexture& bt1, const BrushTexture& bt2) const{
      int r = strcmp(bt1.first.c_str(), bt2.first.c_str());
      if(r != 0)
        return (r<0);
      else
        return (bt1.second < bt2.second);
    }
  };
  typedef std::map<BrushTexture, unsigned, cmpBrushTexture> brushesMap;

  static TextureManager * _pInstance;
  bool                  _hasLoadedTextures;
  brushesMap            _brushesMap;
  static string		_patterns_path;
  static string		_brushes_path;
  unsigned int _defaultTextureId;
};


/**********************************/
/*                                */
/*                                */
/*         StrokeRenderer         */
/*                                */
/*                                */
/**********************************/

/*! Class to render a stroke. 
  Creates a triangle strip and stores it
  strip is lazily created at the first rendering
*/
class LIB_STROKE_EXPORT StrokeRenderer
{
 public:
  StrokeRenderer();
  virtual ~StrokeRenderer ();

  /*! Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const = 0;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const = 0;

  // initializes the texture manager
  // lazy, checks if it has already been done
  static bool loadTextures() ;
  
  //static unsigned int getTextureIndex(unsigned int index)  ;
  static TextureManager *_textureManager;
};


#endif // STROKE_RENDERER_H
