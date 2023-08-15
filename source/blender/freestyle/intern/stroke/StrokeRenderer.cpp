/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Classes to render a stroke with OpenGL
 */

#include "StrokeRenderer.h"

#include "../geometry/GeomUtils.h"

#include "BLI_sys_types.h"

using namespace std;

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*         StrokeRenderer         */
/*                                */
/*                                */
/**********************************/

TextureManager *StrokeRenderer::_textureManager = nullptr;

StrokeRenderer::~StrokeRenderer() = default;

bool StrokeRenderer::loadTextures()
{
  _textureManager->load();
  return true;
}

/**********************************/
/*                                */
/*                                */
/*         TextureManager         */
/*                                */
/*                                */
/**********************************/

TextureManager *TextureManager::_pInstance = nullptr;

string TextureManager::_patterns_path;

string TextureManager::_brushes_path;

TextureManager::TextureManager()
{
  _hasLoadedTextures = false;
  _pInstance = this;
  _defaultTextureId = 0;
}

TextureManager::~TextureManager()
{
  if (!_brushesMap.empty()) {
    _brushesMap.clear();
  }
  _pInstance = nullptr;
}

void TextureManager::load()
{
  if (_hasLoadedTextures) {
    return;
  }
  loadStandardBrushes();
  _hasLoadedTextures = true;
}

uint TextureManager::getBrushTextureIndex(string name, Stroke::MediumType iType)
{
  BrushTexture bt(name, iType);
  brushesMap::iterator b = _brushesMap.find(bt);
  if (b == _brushesMap.end()) {
    uint texId = loadBrush(name, iType);
    _brushesMap[bt] = texId;
    return texId;
    // XXX!
    cerr << "brush file " << name << " not found" << endl;
    return 0;
  }

  return _brushesMap[bt];
}

void TextureManager::Options::setPatternsPath(const string &path)
{
  _patterns_path = path;
}

string TextureManager::Options::getPatternsPath()
{
  return _patterns_path;
}

void TextureManager::Options::setBrushesPath(const string &path)
{
  _brushes_path = path;
}

string TextureManager::Options::getBrushesPath()
{
  return _brushes_path;
}

} /* namespace Freestyle */
