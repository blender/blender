/** 
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __GPC_POLYGONMATERIAL
#define __GPC_POLYGONMATERIAL

#include "RAS_IPolygonMaterial.h"

namespace GPC_PolygonMaterial
{
	void SetMipMappingEnabled(bool enabled = false);
};

#if 0
class GPC_PolygonMaterial : public RAS_IPolyMaterial
{
	struct MTFace* m_tface;

public:
	GPC_PolygonMaterial(const STR_String& texname, bool ba, const STR_String& matname,
			int tile, int tileXrep, int tileYrep, int mode, bool transparant, bool zsort,
			int lightlayer, bool bIsTriangle, void* clientobject, void* tpage);
	
	virtual ~GPC_PolygonMaterial(void);

	/**
	 * Returns the caching information for this material,
	 * This can be used to speed up the rasterizing process.
	 * @return The caching information.
	 */
	virtual TCachingInfo GetCachingInfo(void) const;

	/**
	 * Activates the material in the (OpenGL) rasterizer.
	 * On entry, the cachingInfo contains info about the last activated material.
	 * On exit, the cachingInfo should contain updated info about this material.
	 * @param rasty			The rasterizer in which the material should be active.
	 * @param cachingInfo	The information about the material used to speed up rasterizing.
	 */
	virtual void Activate(RAS_IRasterizer* rasty, TCachingInfo& cachingInfo) const;

	/**
	 * Returns the Blender texture face structure that is used for this material.
	 * @return The material's texture face.
	 */
	MTFace* GetMTFace(void) const;

	static void SetMipMappingEnabled(bool enabled = false);
};


inline MTFace* GPC_PolygonMaterial::GetMTFace(void) const
{
	return m_tface;
}

inline GPC_PolygonMaterial::TCachingInfo GPC_PolygonMaterial::GetCachingInfo(void) const
{
	return GetMTFace();
}
#endif
#endif  // __GPC_POLYGONMATERIAL_H

