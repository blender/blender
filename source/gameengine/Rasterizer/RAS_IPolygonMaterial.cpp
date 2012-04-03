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

/** \file gameengine/Rasterizer/RAS_IPolygonMaterial.cpp
 *  \ingroup bgerast
 */


#include "RAS_IPolygonMaterial.h"
#include "RAS_IRasterizer.h"

#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"

void  RAS_IPolyMaterial::Initialize( 
				const STR_String& texname,
				const STR_String& matname,
				int materialindex,
				int tile,
				int tilexrep,
				int tileyrep,
				int alphablend,
				bool alpha,
				bool zsort,
				bool light,
				bool image,
				struct GameSettings* game)
{
	m_texturename = texname;
	m_materialname = matname;
	m_materialindex = materialindex;
	m_tile = tile;
	m_tilexrep = tilexrep;
	m_tileyrep = tileyrep;
	m_alphablend = alphablend;
	m_alpha = alpha;
	m_zsort = zsort;
	m_light = light;
	m_polymatid = m_newpolymatid++;
	m_flag = 0;
	m_multimode = 0;
	m_shininess = 35.0;
	m_specular.setValue(0.5,0.5,0.5);
	m_specularity = 1.0;
	m_diffuse.setValue(0.5,0.5,0.5);
	m_drawingmode = ConvertFaceMode(game, image);
}

RAS_IPolyMaterial::RAS_IPolyMaterial() 
		: m_texturename("__Dummy_Texture_Name__"),
		m_materialname("__Dummy_Material_Name__"),
		m_tile(0),
		m_tilexrep(0),
		m_tileyrep(0),
		m_drawingmode (0),
		m_alphablend(0),
		m_alpha(false),
		m_zsort(false),
		m_light(false),
		m_materialindex(0),
		m_polymatid(0),
		m_flag(0),
		m_multimode(0)
{
	m_shininess = 35.0;
	m_specular = MT_Vector3(0.5,0.5,0.5);
	m_specularity = 1.0;
	m_diffuse = MT_Vector3(0.5,0.5,0.5);
}

RAS_IPolyMaterial::RAS_IPolyMaterial(const STR_String& texname,
									 const STR_String& matname,
									 int materialindex,
									 int tile,
									 int tilexrep,
									 int tileyrep,
									 int alphablend,
									 bool alpha,
									 bool zsort)
		: m_texturename(texname),
		m_materialname(matname),
		m_tile(tile),
		m_tilexrep(tilexrep),
		m_tileyrep(tileyrep),
		m_alphablend(alphablend),
		m_alpha(alpha),
		m_zsort(zsort),
		m_materialindex(materialindex),
		m_polymatid(m_newpolymatid++),
		m_flag(0),
		m_multimode(0)
{
	m_shininess = 35.0;
	m_specular = MT_Vector3(0.5,0.5,0.5);
	m_specularity = 1.0;
	m_diffuse = MT_Vector3(0.5,0.5,0.5);
}


bool RAS_IPolyMaterial::Equals(const RAS_IPolyMaterial& lhs) const
{
	if (m_flag &RAS_BLENDERMAT)
	{
		bool test = (
			this->m_multimode			==		lhs.m_multimode &&
			this->m_flag				==		lhs.m_flag		&&
			this->m_drawingmode			==		lhs.m_drawingmode &&
			this->m_alphablend			==		lhs.m_alphablend &&
			this->m_texturename.hash()		==		lhs.m_texturename.hash() &&
			this->m_materialname.hash() 		==		lhs.m_materialname.hash()
		);

		return test;
	}
	else
	{
		return (
				this->m_tile		==		lhs.m_tile &&
				this->m_tilexrep	==		lhs.m_tilexrep &&
				this->m_tileyrep	==		lhs.m_tileyrep &&
				this->m_alphablend	==		lhs.m_alphablend &&
				this->m_alpha		==		lhs.m_alpha &&
				this->m_zsort		==		lhs.m_zsort &&
				this->m_light		==		lhs.m_light &&
				this->m_drawingmode	==		lhs.m_drawingmode &&
				this->m_texturename.hash()	==		lhs.m_texturename.hash() &&
				this->m_materialname.hash() ==		lhs.m_materialname.hash()
		);
	}
}

int RAS_IPolyMaterial::ConvertFaceMode(struct GameSettings *game, bool image) const
{
	if (!game) return (image?GEMAT_TEX:0);

	int modefinal = 0;

	int orimode   = game->face_orientation;
	int alpha_blend = game->alpha_blend;
	int flags = game->flag & (GEMAT_TEXT | GEMAT_BACKCULL);

	modefinal = orimode | alpha_blend | flags;
	modefinal |= (image ? GEMAT_TEX : 0);

	return modefinal;
}

void RAS_IPolyMaterial::GetMaterialRGBAColor(unsigned char *rgba) const
{
	*rgba++ = 0xFF;
	*rgba++ = 0xFF;
	*rgba++ = 0xFF;
	*rgba++ = 0xFF;
}

bool RAS_IPolyMaterial::Less(const RAS_IPolyMaterial& rhs) const
{
	if (Equals(rhs))
		return false;
		
	return m_polymatid < rhs.m_polymatid;
}

bool RAS_IPolyMaterial::IsAlpha() const
{
	return m_alpha || m_zsort;
}

bool RAS_IPolyMaterial::IsZSort() const
{
	return m_zsort;
}

unsigned int RAS_IPolyMaterial::hash() const
{
	return m_texturename.hash();
}

int RAS_IPolyMaterial::GetDrawingMode() const
{
	return m_drawingmode;
}

const STR_String& RAS_IPolyMaterial::GetMaterialName() const
{ 
	return m_materialname;
}

dword RAS_IPolyMaterial::GetMaterialNameHash() const
{
	return m_materialname.hash();
}

const STR_String& RAS_IPolyMaterial::GetTextureName() const
{
	return m_texturename;
}

int RAS_IPolyMaterial::GetMaterialIndex() const
{
	return m_materialindex;
}

Material *RAS_IPolyMaterial::GetBlenderMaterial() const
{
	return NULL;
}

Image *RAS_IPolyMaterial::GetBlenderImage() const
{
	return NULL;
}

Scene* RAS_IPolyMaterial::GetBlenderScene() const
{
	return NULL;
}

void RAS_IPolyMaterial::ReleaseMaterial()
{
}

unsigned int	RAS_IPolyMaterial::GetFlag() const
{
	return m_flag;
}

bool RAS_IPolyMaterial::UsesLighting(RAS_IRasterizer *rasty) const
{
	bool dolights = false;

	if (m_flag & RAS_BLENDERMAT)
		dolights = (m_flag &RAS_MULTILIGHT)!=0;
	else if (rasty->GetDrawingMode() < RAS_IRasterizer::KX_SOLID);
	else if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_SHADOW);
	else
		dolights = m_light;
	
	return dolights;
}

bool RAS_IPolyMaterial::CastsShadows() const
{
	return (m_flag & RAS_CASTSHADOW) != 0;
}

bool RAS_IPolyMaterial::UsesObjectColor() const
{
	return !(m_flag & RAS_BLENDERGLSL);
}

unsigned int RAS_IPolyMaterial::m_newpolymatid = 0;

