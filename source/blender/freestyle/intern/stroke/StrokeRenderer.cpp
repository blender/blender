/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/stroke/StrokeRenderer.cpp
 *  \ingroup freestyle
 *  \brief Classes to render a stroke with OpenGL
 *  \author Fredo Durand
 *  \date 09/09/2002
 */

#include "StrokeRenderer.h"

#include "../geometry/GeomUtils.h"

using namespace std;

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*         StrokeRenderer         */
/*                                */
/*                                */
/**********************************/

LIB_STROKE_EXPORT
TextureManager *StrokeRenderer::_textureManager = 0;

StrokeRenderer::StrokeRenderer() {}

StrokeRenderer::~StrokeRenderer() {}

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


LIB_STROKE_EXPORT
TextureManager *TextureManager::_pInstance = 0;

LIB_STROKE_EXPORT
string TextureManager::_patterns_path;

LIB_STROKE_EXPORT
string TextureManager::_brushes_path;

TextureManager::TextureManager()
{
	_hasLoadedTextures = false;
	_pInstance = this;
	_defaultTextureId = 0;
}

TextureManager::~TextureManager()
{
	if (!_brushesMap.empty())
		_brushesMap.clear();
	_pInstance = 0;
}

void TextureManager::load()
{
	if (_hasLoadedTextures)
		return;
	loadStandardBrushes();
	_hasLoadedTextures = true;
}

unsigned TextureManager::getBrushTextureIndex(string name, Stroke::MediumType loadingMode)
{
	BrushTexture bt(name, loadingMode);
	brushesMap::iterator b = _brushesMap.find(bt);
	if (b == _brushesMap.end()) {
		unsigned texId = loadBrush(name, loadingMode);
		_brushesMap[bt] = texId;
		return texId;
		// XXX!
		cerr << "brush file " << name << " not found" << endl;
		return 0;
	}
	else {
		return _brushesMap[bt];
	}
}

void TextureManager::Options::setPatternsPath(const string& path)
{
	_patterns_path = path;
}

string TextureManager::Options::getPatternsPath()
{
	return _patterns_path;
}

void TextureManager::Options::setBrushesPath(const string& path)
{
	_brushes_path = path;
}

string TextureManager::Options::getBrushesPath()
{
	return _brushes_path;
}

} /* namespace Freestyle */
