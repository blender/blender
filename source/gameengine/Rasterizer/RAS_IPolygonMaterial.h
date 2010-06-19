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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#ifndef __RAS_IPOLYGONMATERIAL
#define __RAS_IPOLYGONMATERIAL

#include "STR_HashedString.h"

/**
 * Polygon Material on which the material buckets are sorted
 *
 */
#include "MT_Vector3.h"
#include "STR_HashedString.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class RAS_IRasterizer;
struct MTFace;
struct Material;
struct Scene;
class SCA_IScene;

enum MaterialProps
{
	RAS_ZSORT		=1,
	RAS_TRANSPARENT =2,
	RAS_TRIANGLE	=4,
	RAS_MULTITEX	=8,
	RAS_MULTILIGHT	=16,
	RAS_BLENDERMAT	=32,
	RAS_GLSHADER	=64,
	RAS_AUTOGEN		=128,
	RAS_NORMAL		=256,
	RAS_DEFMULTI	=512,
	RAS_BLENDERGLSL =1024
};

/**
 * Material properties.
 */
class RAS_IPolyMaterial
{
	//todo: remove these variables from this interface/protocol class
protected:
	STR_HashedString		m_texturename;
	STR_HashedString		m_materialname; //also needed for touchsensor  
	int						m_tile;
	int						m_tilexrep,m_tileyrep;
	int						m_drawingmode;	// tface->mode
	int						m_transp;
	bool					m_alpha;
	bool					m_zsort;
	int						m_materialindex;
	
	unsigned int			m_polymatid;
	static unsigned int		m_newpolymatid;

	// will move...
	unsigned int			m_flag;//MaterialProps
	int						m_multimode; // sum of values
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

	RAS_IPolyMaterial();
	RAS_IPolyMaterial(const STR_String& texname,
					  const STR_String& matname,
					  int materialindex,
					  int tile,
					  int tilexrep,
					  int tileyrep,
					  int mode,
					  int transp,
					  bool alpha,
					  bool zsort);
	void Initialize(const STR_String& texname,
					const STR_String& matname,
					int materialindex,
					int tile,
					int tilexrep,
					int tileyrep,
					int mode,
					int transp,
					bool alpha,
					bool zsort);
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
	virtual bool Activate(RAS_IRasterizer* rasty, TCachingInfo& cachingInfo) const 
	{ 
		return false; 
	}
	virtual void ActivateMeshSlot(const class RAS_MeshSlot & ms, RAS_IRasterizer* rasty) const {}

	virtual bool				Equals(const RAS_IPolyMaterial& lhs) const;
	bool				Less(const RAS_IPolyMaterial& rhs) const;
	//int					GetLightLayer() const;
	bool				IsAlpha() const;
	bool				IsZSort() const;
	unsigned int		hash() const;
	int					GetDrawingMode() const;
	const STR_String&	GetMaterialName() const;
	dword				GetMaterialNameHash() const;
	const STR_String&	GetTextureName() const;
	unsigned int		GetFlag() const;
	int					GetMaterialIndex() const;

	virtual Material*   GetBlenderMaterial() const;
	virtual Scene*		GetBlenderScene() const;
	virtual void		ReleaseMaterial();
	virtual void		GetMaterialRGBAColor(unsigned char *rgba) const;
	virtual bool		UsesLighting(RAS_IRasterizer *rasty) const;
	virtual bool		UsesObjectColor() const;

	virtual void		Replace_IScene(SCA_IScene *val) {}; /* overridden by KX_BlenderMaterial */

	/*
	 * PreCalculate texture gen
	 */
	virtual void OnConstruction(int layer){}
		
		
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:RAS_IPolyMaterial"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

inline  bool operator ==( const RAS_IPolyMaterial & rhs,const RAS_IPolyMaterial & lhs)
{
	return ( rhs.Equals(lhs));
}

inline  bool operator < ( const RAS_IPolyMaterial & lhs, const RAS_IPolyMaterial & rhs)
{
	return lhs.Less(rhs);
}

#endif //__RAS_IPOLYGONMATERIAL

