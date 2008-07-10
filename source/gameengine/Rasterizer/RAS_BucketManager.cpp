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

#include <set>

RAS_BucketManager::RAS_BucketManager()
{

}

RAS_BucketManager::~RAS_BucketManager()
{
		RAS_BucketManagerClearAll();
}

/**
 * struct alphamesh holds a mesh, (m_ms) it's depth, (m_z) and the bucket it came from (m_bucket.)
 */
struct RAS_BucketManager::alphamesh
{
public:
	MT_Scalar m_z;
	RAS_MaterialBucket::T_MeshSlotList::iterator m_ms;
	RAS_MaterialBucket *m_bucket;
	alphamesh(MT_Scalar z, RAS_MaterialBucket::T_MeshSlotList::iterator &ms, RAS_MaterialBucket *bucket) :
		m_z(z),
		m_ms(ms),
		m_bucket(bucket)
	{}
};

struct RAS_BucketManager::backtofront
{
	bool operator()(const alphamesh &a, const alphamesh &b)
	{
		return a.m_z < b.m_z;
	}
};
	

void RAS_BucketManager::RenderAlphaBuckets(
	const MT_Transform& cameratrans, RAS_IRasterizer* rasty, RAS_IRenderTools* rendertools)
{
	BucketList::iterator bit;
	std::multiset<alphamesh, backtofront> alphameshset;
	RAS_MaterialBucket::T_MeshSlotList::iterator mit;
	
	/* Camera's near plane equation: cam_norm.dot(point) + cam_origin */
	const MT_Vector3 cam_norm(cameratrans.getBasis()[2]);
	const MT_Scalar cam_origin = cameratrans.getOrigin()[2];
	for (bit = m_AlphaBuckets.begin(); bit != m_AlphaBuckets.end(); ++bit)
	{
		(*bit)->ClearScheduledPolygons();
		for (mit = (*bit)->msBegin(); mit != (*bit)->msEnd(); ++mit)
		{
			if ((*mit).m_bVisible)
			{
				MT_Point3 pos((*mit).m_OpenGLMatrix[12], (*mit).m_OpenGLMatrix[13], (*mit).m_OpenGLMatrix[14]);
				alphameshset.insert(alphamesh(MT_dot(cam_norm, pos) + cam_origin, mit, *bit));
			}
		}
	}
	
	// It shouldn't be strictly necessary to disable depth writes; but
	// it is needed for compatibility.
	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_DISABLED);

	RAS_IRasterizer::DrawMode drawingmode;
	std::multiset< alphamesh, backtofront>::iterator msit = alphameshset.begin();
	for (; msit != alphameshset.end(); ++msit)
	{
		rendertools->SetClientObject((*(*msit).m_ms).m_clientObj);
		while ((*msit).m_bucket->ActivateMaterial(cameratrans, rasty, rendertools, drawingmode))
			(*msit).m_bucket->RenderMeshSlot(cameratrans, rasty, rendertools, *(*msit).m_ms, drawingmode);
	}
	
	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_ENABLED);
}

void RAS_BucketManager::Renderbuckets(
	const MT_Transform& cameratrans, RAS_IRasterizer* rasty, RAS_IRenderTools* rendertools)
{
	BucketList::iterator bucket;
	
	rasty->EnableTextures(false);
	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_ENABLED);
	
	// beginning each frame, clear (texture/material) caching information
	rasty->ClearCachingInfo();

	RAS_MaterialBucket::StartFrame();
	for (bucket = m_MaterialBuckets.begin(); bucket != m_MaterialBuckets.end(); ++bucket)
	{
		(*bucket)->ClearScheduledPolygons();
	}
	
	for (bucket = m_MaterialBuckets.begin(); bucket != m_MaterialBuckets.end(); ++bucket)
	{
		RAS_IPolyMaterial *tmp = (*bucket)->GetPolyMaterial();
		if(tmp->IsZSort() || tmp->GetFlag() &RAS_FORCEALPHA )
			rasty->SetAlphaTest(true);
		else
			rasty->SetAlphaTest(false);

		(*bucket)->Render(cameratrans,rasty,rendertools);
	}
	rasty->SetAlphaTest(false);

	RenderAlphaBuckets(cameratrans, rasty, rendertools);	
	RAS_MaterialBucket::EndFrame();
}

RAS_MaterialBucket* RAS_BucketManager::RAS_BucketManagerFindBucket(RAS_IPolyMaterial * material, bool &bucketCreated)
{
	bucketCreated = false;
	BucketList::iterator it;
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
	bucketCreated = true;
	if (bucket->IsTransparant())
		m_AlphaBuckets.push_back(bucket);
	else
		m_MaterialBuckets.push_back(bucket);
	
	return bucket;
}

void RAS_BucketManager::RAS_BucketManagerClearAll()
{
	BucketList::iterator it;
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
