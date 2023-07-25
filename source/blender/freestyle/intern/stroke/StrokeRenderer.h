/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Classes to render a stroke with OpenGL
 */

#include <algorithm>
#include <map>
#include <string.h>
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

/** Class to load textures */
class TextureManager {
 public:
  TextureManager();
  virtual ~TextureManager();

  static TextureManager *getInstance()
  {
    return _pInstance;
  }

  void load();
  uint getBrushTextureIndex(string name, Stroke::MediumType iType = Stroke::OPAQUE_MEDIUM);

  inline bool hasLoaded() const
  {
    return _hasLoadedTextures;
  }

  inline uint getDefaultTextureId() const
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
  virtual uint loadBrush(string fileName, Stroke::MediumType = Stroke::OPAQUE_MEDIUM) = 0;

  typedef std::pair<string, Stroke::MediumType> BrushTexture;
  struct cmpBrushTexture {
    bool operator()(const BrushTexture &bt1, const BrushTexture &bt2) const
    {
      int r = strcmp(bt1.first.c_str(), bt2.first.c_str());
      if (r != 0) {
        return (r < 0);
      }
      else {
        return (bt1.second < bt2.second);
      }
    }
  };
  typedef std::map<BrushTexture, uint, cmpBrushTexture> brushesMap;

  static TextureManager *_pInstance;
  bool _hasLoadedTextures;
  brushesMap _brushesMap;
  static string _patterns_path;
  static string _brushes_path;
  uint _defaultTextureId;

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

/** Class to render a stroke. Creates a triangle strip and stores it strip is lazily created at the
 * first rendering */
class StrokeRenderer {
 public:
  virtual ~StrokeRenderer();

  /** Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const = 0;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const = 0;

  // initializes the texture manager
  // lazy, checks if it has already been done
  static bool loadTextures();

  // static uint getTextureIndex(uint index);
  static TextureManager *_textureManager;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:StrokeRenderer")
#endif
};

} /* namespace Freestyle */
