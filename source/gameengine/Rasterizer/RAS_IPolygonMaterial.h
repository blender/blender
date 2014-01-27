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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file RAS_IPolygonMaterial.h
 *  \ingroup bgerast
 */

#ifndef __RAS_IPOLYGONMATERIAL_H__
#define __RAS_IPOLYGONMATERIAL_H__

#include "STR_HashedString.h"

#include "MT_Vector3.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class RAS_IRasterizer;
struct MTFace;
struct Material;
struct Image;
struct Scene;
class SCA_IScene;
struct GameSettings;

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
	RAS_BLENDERGLSL =1024,
	RAS_CASTSHADOW	=2048,
	RAS_ONLYSHADOW	=4096,
};

/**
 * Polygon Material on which the material buckets are sorted
 *
 */
class RAS_IPolyMaterial
{
	//todo: remove these variables from this interface/protocol class
protected:
	STR_HashedString		m_texturename;
	STR_HashedString		m_materialname; //also needed for touchsensor  
	int						m_tile;
	int						m_tilexrep,m_tileyrep;
	int						m_drawingmode;
	int						m_alphablend;
	bool					m_alpha;
	bool					m_zsort;
	bool					m_light;
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
		BILLBOARD_SCREENALIGNED	= 512,  /* GEMAT_HALO */
		BILLBOARD_AXISALIGNED	= 1024, /* GEMAT_BILLBOARD */
		SHADOW			=2048   /* GEMAT_SHADOW */
	};

	RAS_IPolyMaterial();
	RAS_IPolyMaterial(const STR_String& texname,
					  const STR_String& matname,
					  int materialindex,
					  int tile,
					  int tilexrep,
					  int tileyrep,
					  int transp,
					  bool alpha,
					  bool zsort);
	void Initialize(const STR_String& texname,
					const STR_String& matname,
					int materialindex,
					int tile,
					int tilexrep,
					int tileyrep,
					int transp,
					bool alpha,
					bool zsort,
					bool light,
					bool image,
					struct GameSettings* game);

	virtual ~RAS_IPolyMaterial() {}
 
	/**
	 * Returns the caching information for this material,
	 * This can be used to speed up the rasterizing process.
	 * \return The caching information.
	 */
	virtual TCachingInfo GetCachingInfo(void) const { return 0; }

	/**
	 * Activates the material in the rasterizer.
	 * On entry, the cachingInfo contains info about the last activated material.
	 * On exit, the cachingInfo should contain updated info about this material.
	 * \param rasty			The rasterizer in which the material should be active.
	 * \param cachingInfo	The information about the material used to speed up rasterizing.
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
	virtual Image*      GetBlenderImage() const;
	virtual MTFace*		GetMTFace() const;
	virtual unsigned int* GetMCol() const;
	virtual Scene*		GetBlenderScene() const;
	virtual void		ReleaseMaterial();
	virtual void		GetMaterialRGBAColor(unsigned char *rgba) const;
	virtual bool		UsesLighting(RAS_IRasterizer *rasty) const;
	virtual bool		UsesObjectColor() const;
	virtual bool		CastsShadows() const;
	virtual bool		OnlyShadow() const;

	virtual void		Replace_IScene(SCA_IScene *val) {} /* overridden by KX_BlenderMaterial */

	/**
	 * \return the equivalent drawing mode for the material settings (equivalent to old TexFace tface->mode).
	 */
	int					ConvertFaceMode(struct GameSettings *game, bool image) const;

	/*
	 * PreCalculate texture gen
	 */
	virtual void OnConstruction() {}


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_IPolyMaterial")
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

#endif  /* __RAS_IPOLYGONMATERIAL_H__ */
