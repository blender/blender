/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef __RAS_IPOLYGONMATERIAL
#define __RAS_IPOLYGONMATERIAL

#include "STR_HashedString.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * Polygon Material on which the material buckets are sorted
 *
 */
#include "MT_Vector3.h"
#include "STR_HashedString.h"

class RAS_IRasterizer;

class RAS_IPolyMaterial
{
	//todo: remove these variables from this interface/protocol class
protected:
	STR_HashedString    m_texturename;
	STR_String          m_materialname; //also needed for touchsensor  
	int					m_tile;
	int					m_tilexrep,m_tileyrep;
	int					m_drawingmode;	// tface->mode
	int					m_transparant;
	int					m_lightlayer;
	bool				m_bIsTriangle;
	
public:

	MT_Vector3			m_diffuse;
	float				m_shininess;
	MT_Vector3			m_specular;
	float				m_specularity;
	
	/** Used to store caching information for materials. */
	typedef void* TCachingInfo;

	// care! these are taken from blender polygonflags, see file DNA_mesh_types.h for #define TF_BILLBOARD etc.
	enum MaterialFlags
	{
		BILLBOARD_SCREENALIGNED = 256,
		BILLBOARD_AXISALIGNED = 4096,
		SHADOW				  =8192
	};

	RAS_IPolyMaterial(const STR_String& texname,
					  bool ba,
					  const STR_String& matname,
					  int tile,
					  int tilexrep,
					  int tileyrep,
					  int mode,
					  int transparant,
					  int lightlayer,
					  bool bIsTriangle,
					  void* clientobject);
	virtual ~RAS_IPolyMaterial() {};
 
	/**
	 * Returns the caching information for this material,
	 * This can be used to speed up the rasterizing process.
	 * @return The caching information.
	 */
	virtual TCachingInfo GetCachingInfo(void) const { return 0; }

	/**
	 * Activates the material in the rasterizer.
	 * On entry, the cachingInfo contains info about the last activated material.
	 * On exit, the cachingInfo should contain updated info about this material.
	 * @param rasty			The rasterizer in which the material should be active.
	 * @param cachingInfo	The information about the material used to speed up rasterizing.
	 */
	virtual void Activate(RAS_IRasterizer* rasty, TCachingInfo& cachingInfo) const {}

	bool				Equals(const RAS_IPolyMaterial& lhs) const;
	int					GetLightLayer();
	bool				IsTransparant();
	bool				UsesTriangles();
	unsigned int		hash() const;
	int					GetDrawingMode();
	const STR_String&	GetMaterialName() const;
	const STR_String&	GetTextureName() const;
};

inline  bool operator ==( const RAS_IPolyMaterial & rhs,const RAS_IPolyMaterial & lhs)
{
	return ( rhs.Equals(lhs));
}

#endif //__RAS_IPOLYGONMATERIAL

