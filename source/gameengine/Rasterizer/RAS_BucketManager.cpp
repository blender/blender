/**
 * $Id$
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// don't show these anoying STL warnings
#pragma warning (disable:4786)
#endif

#include "GEN_Map.h"
#include "RAS_MaterialBucket.h"
#include "STR_HashedString.h"
#include "RAS_MeshObject.h"
#define KX_NUM_MATERIALBUCKETS 100
#include "RAS_IRasterizer.h"
#include "RAS_IRenderTools.h"

#include "RAS_BucketManager.h"



RAS_BucketManager::RAS_BucketManager()
{

}

RAS_BucketManager::~RAS_BucketManager()
{
		RAS_BucketManagerClearAll();
}


void RAS_BucketManager::Renderbuckets(
	const MT_Transform& cameratrans, RAS_IRasterizer* rasty, RAS_IRenderTools* rendertools)
{
	std::vector<RAS_MaterialBucket*>::iterator bucket;
	
	rasty->EnableTextures(false);
	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_ENABLED);
	
	// beginning each frame, clear (texture/material) caching information
	rasty->ClearCachingInfo();

	RAS_MaterialBucket::StartFrame();
	for (bucket = m_MaterialBuckets.begin(); bucket != m_MaterialBuckets.end(); bucket++)
	{
		(*bucket)->ClearScheduledPolygons();
	}
	for (bucket = m_AlphaBuckets.begin(); bucket != m_AlphaBuckets.end(); bucket++)
	{
		(*bucket)->ClearScheduledPolygons();
	}

	for (bucket = m_MaterialBuckets.begin(); bucket != m_MaterialBuckets.end(); bucket++)
		(*bucket)->Render(cameratrans,rasty,rendertools);
	
	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_DISABLED);
	
	for (bucket = m_AlphaBuckets.begin(); bucket != m_AlphaBuckets.end(); bucket++)
	{
		(*bucket)->Render(cameratrans,rasty,rendertools);
	}

	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_ENABLED);
	
	RAS_MaterialBucket::EndFrame();
}

RAS_MaterialBucket* RAS_BucketManager::RAS_BucketManagerFindBucket(RAS_IPolyMaterial * material)
{
	std::vector<RAS_MaterialBucket*>::iterator it;
	for (it = m_MaterialBuckets.begin(); it != m_MaterialBuckets.end(); it++)
	{
		if (*(*it)->GetPolyMaterial() == *material)
			return *it;
	}
	
	for (it = m_AlphaBuckets.begin(); it != m_AlphaBuckets.end(); it++)
	{
		if (*(*it)->GetPolyMaterial() == *material)
			return *it;
	}
	
	RAS_MaterialBucket *bucket = new RAS_MaterialBucket(material);
	if (bucket->IsTransparant())
		m_AlphaBuckets.push_back(bucket);
	else
		m_MaterialBuckets.push_back(bucket);
	
	return bucket;
}

void RAS_BucketManager::RAS_BucketManagerClearAll()
{
	std::vector<RAS_MaterialBucket*>::iterator it;
	for (it = m_MaterialBuckets.begin(); it != m_MaterialBuckets.end(); it++)
	{
		delete (*it);
	}
	for (it = m_AlphaBuckets.begin(); it != m_AlphaBuckets.end(); it++)
	{
		delete(*it);
	}
	
	m_MaterialBuckets.clear();
	m_AlphaBuckets.clear();
}
