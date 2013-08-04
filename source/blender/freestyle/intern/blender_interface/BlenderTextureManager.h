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

#ifndef __BLENDERTEXTUREMANAGER_H__
#define __BLENDERTEXTUREMANAGER_H__

/** \file blender/freestyle/intern/blender_interface/BlenderTextureManager.h
 *  \ingroup freestyle
 */

# include "../stroke/StrokeRenderer.h"
# include "../stroke/StrokeRep.h"
# include "../system/FreestyleConfig.h"

namespace Freestyle {

/*! Class to load textures */
class LIB_RENDERING_EXPORT BlenderTextureManager : public TextureManager
{
public:
	BlenderTextureManager();
	virtual ~BlenderTextureManager();

protected:
	virtual unsigned int loadBrush(string fileName, Stroke::MediumType=Stroke::OPAQUE_MEDIUM);

protected:
	virtual void loadStandardBrushes();

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BlenderTextureManager")
#endif

};

} /* namespace Freestyle */

#endif // __BLENDERTEXTUREMANAGER_H__
