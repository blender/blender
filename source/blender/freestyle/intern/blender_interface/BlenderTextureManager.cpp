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

/** \file blender/freestyle/intern/blender_interface/BlenderTextureManager.cpp
 *  \ingroup freestyle
 */

#include "BlenderTextureManager.h"

#include "BKE_global.h"

namespace Freestyle {

BlenderTextureManager::BlenderTextureManager()
: TextureManager()
{
	//_brushes_path = Config::getInstance()...
}

BlenderTextureManager::~BlenderTextureManager()
{
}

void BlenderTextureManager::loadStandardBrushes()
{
#if 0
	getBrushTextureIndex(TEXTURES_DIR "/brushes/charcoalAlpha.bmp", Stroke::HUMID_MEDIUM);
	getBrushTextureIndex(TEXTURES_DIR "/brushes/washbrushAlpha.bmp", Stroke::HUMID_MEDIUM);
	getBrushTextureIndex(TEXTURES_DIR "/brushes/oil.bmp", Stroke::HUMID_MEDIUM);
	getBrushTextureIndex(TEXTURES_DIR "/brushes/oilnoblend.bmp", Stroke::HUMID_MEDIUM);
	getBrushTextureIndex(TEXTURES_DIR "/brushes/charcoalAlpha.bmp", Stroke::DRY_MEDIUM);
	getBrushTextureIndex(TEXTURES_DIR "/brushes/washbrushAlpha.bmp", Stroke::DRY_MEDIUM);
	getBrushTextureIndex(TEXTURES_DIR "/brushes/opaqueDryBrushAlpha.bmp", Stroke::OPAQUE_MEDIUM);
	getBrushTextureIndex(TEXTURES_DIR "/brushes/opaqueBrushAlpha.bmp", Stroke::OPAQUE_MEDIUM);
	_defaultTextureId = getBrushTextureIndex("smoothAlpha.bmp", Stroke::OPAQUE_MEDIUM);
#endif
}

unsigned int BlenderTextureManager::loadBrush(string sname, Stroke::MediumType mediumType)
{
#if 0
	GLuint texId;
	glGenTextures(1, &texId);
	bool found = false;
	vector<string> pathnames;
	string path; //soc
	StringUtils::getPathName(TextureManager::Options::getBrushesPath(), sname, pathnames);
	for (vector<string>::const_iterator j = pathnames.begin(); j != pathnames.end(); j++) {
		path = j->c_str();
		//soc if (QFile::exists(path)) {
		if (BLI_exists( const_cast<char *>(path.c_str()))) {
			found = true;
			break;
		}
	}
	if (!found)
		return 0;
	// Brush texture
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Loading brush texture..." << endl;
	}
	switch (mediumType) {
	case Stroke::DRY_MEDIUM:
		//soc prepareTextureLuminance((const char*)path.toAscii(), texId);
		prepareTextureLuminance(path, texId);
		break;
	case Stroke::HUMID_MEDIUM:
	case Stroke::OPAQUE_MEDIUM:
	default:
		//soc prepareTextureAlpha((const char*)path.toAscii(), texId);
		prepareTextureAlpha(path, texId);
		break;
	}
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Done." << endl << endl;
	}

	return texId;
#else
	return 0;
#endif
}

} /* namespace Freestyle */
