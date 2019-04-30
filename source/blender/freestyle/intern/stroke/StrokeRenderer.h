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

#ifndef __FREESTYLE_STROKE_RENDERER_H__
#define __FREESTYLE_STROKE_RENDERER_H__

/** \file
 * \ingroup freestyle
 * \brief Classes to render a stroke with OpenGL
 */

#include <map>
#include <string.h>
#include <algorithm>
#include <utility>
#include <vector>

#include "Stroke.h"
#include "StrokeRep.h"

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*         TextureManager         */
/*                                */
/*                                */
/**********************************/

/*! Class to load textures */
class TextureManager {
 public:
  TextureManager();
  virtual ~TextureManager();

  static TextureManager *getInstance()
  {
    return _pInstance;
  }

  void load();
  unsigned getBrushTextureIndex(string name, Stroke::MediumType iType = Stroke::OPAQUE_MEDIUM);

  inline bool hasLoaded() const
  {
    return _hasLoadedTextures;
  }

  inline unsigned int getDefaultTextureId() const
  {
    return _defaultTextureId;
  }

  struct Options {
    static void setPatternsPath(const string &path);
    static string getPatternsPath();

    static void setBrushesPath(const string &path);
    static string getBrushesPath();
  };

 protected:
  virtual void loadStandardBrushes() = 0;
  virtual unsigned loadBrush(string fileName, Stroke::MediumType = Stroke::OPAQUE_MEDIUM) = 0;

  typedef std::pair<string, Stroke::MediumType> BrushTexture;
  struct cmpBrushTexture {
    bool operator()(const BrushTexture &bt1, const BrushTexture &bt2) const
    {
      int r = strcmp(bt1.first.c_str(), bt2.first.c_str());
      if (r != 0)
        return (r < 0);
      else
        return (bt1.second < bt2.second);
    }
  };
  typedef std::map<BrushTexture, unsigned, cmpBrushTexture> brushesMap;

  static TextureManager *_pInstance;
  bool _hasLoadedTextures;
  brushesMap _brushesMap;
  static string _patterns_path;
  static string _brushes_path;
  unsigned int _defaultTextureId;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:TextureManager")
#endif
};

/**********************************/
/*                                */
/*                                */
/*         StrokeRenderer         */
/*                                */
/*                                */
/**********************************/

/*! Class to render a stroke. Creates a triangle strip and stores it strip is lazily created at the
 * first rendering */
class StrokeRenderer {
 public:
  StrokeRenderer();
  virtual ~StrokeRenderer();

  /*! Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const = 0;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const = 0;

  // initializes the texture manager
  // lazy, checks if it has already been done
  static bool loadTextures();

  // static unsigned int getTextureIndex(unsigned int index);
  static TextureManager *_textureManager;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:StrokeRenderer")
#endif
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_STROKE_RENDERER_H__
