/**
 * $Id$
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

#include "RAS_IPolygonMaterial.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RAS_IPolyMaterial::RAS_IPolyMaterial(const STR_String& texname,
									 const STR_String& matname,
									 int tile,
									 int tilexrep,
									 int tileyrep,
									 int mode,
									 bool transparant,
									 bool zsort,
									 int lightlayer,
									 bool bIsTriangle,
									 void* clientobject=NULL) :

		m_texturename(texname),
		m_materialname(matname),
		m_tile(tile),
		m_tilexrep(tilexrep),
		m_tileyrep(tileyrep),
		m_drawingmode (mode),
		m_transparant(transparant),
		m_zsort(zsort),
		m_lightlayer(lightlayer),
		m_bIsTriangle(bIsTriangle),
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
	if(m_flag &RAS_BLENDERMAT)
	{
		return (
			this->m_multimode			==		lhs.m_multimode &&
			this->m_flag				==		lhs.m_flag		&&
			this->m_drawingmode			==		lhs.m_drawingmode &&
			this->m_lightlayer			==		lhs.m_lightlayer &&
			this->m_texturename.hash()	==		lhs.m_texturename.hash() &&
			this->m_materialname.hash() ==		lhs.m_materialname.hash()
		);
	}
	else
	{
		return (
				this->m_tile		==		lhs.m_tile &&
				this->m_tilexrep	==		lhs.m_tilexrep &&
				this->m_tileyrep	==		lhs.m_tileyrep &&
				this->m_transparant	==		lhs.m_transparant &&
				this->m_zsort		==		lhs.m_zsort &&
				this->m_drawingmode	==		lhs.m_drawingmode &&
				this->m_bIsTriangle	==		lhs.m_bIsTriangle &&
				this->m_lightlayer	==		lhs.m_lightlayer &&
				this->m_texturename.hash()	==		lhs.m_texturename.hash() &&
				this->m_materialname.hash() ==		lhs.m_materialname.hash()
		);
	}
}

bool RAS_IPolyMaterial::Less(const RAS_IPolyMaterial& rhs) const
{
	if (Equals(rhs))
		return false;
		
	return m_polymatid < rhs.m_polymatid;
}

int RAS_IPolyMaterial::GetLightLayer() const
{
	return m_lightlayer;
}

bool RAS_IPolyMaterial::IsTransparant() const
{
	return m_transparant;
}

bool RAS_IPolyMaterial::IsZSort() const
{
	return m_zsort;
}

bool RAS_IPolyMaterial::UsesTriangles() const
{
	return m_bIsTriangle;
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

const STR_String& RAS_IPolyMaterial::GetTextureName() const
{
	return m_texturename;
}

const unsigned int	RAS_IPolyMaterial::GetFlag() const
{
	return m_flag;
}

unsigned int RAS_IPolyMaterial::m_newpolymatid = 0;
