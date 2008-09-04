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

#ifdef WIN32
// don't show these anoying STL warnings
#pragma warning (disable:4786)
#endif

#include "GEN_Map.h"
#include "RAS_MaterialBucket.h"
#include "STR_HashedString.h"
#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"
#include "RAS_IRenderTools.h"

#include "RAS_BucketManager.h"

#include <algorithm>
#include <set>

/* sorting */

struct RAS_BucketManager::sortedmeshslot
{
public:
	MT_Scalar m_z;					/* depth */
	RAS_MeshSlot *m_ms;				/* mesh slot */
	RAS_MaterialBucket *m_bucket;	/* buck mesh slot came from */

	sortedmeshslot() {}

	void set(RAS_MeshSlot *ms, RAS_MaterialBucket *bucket, const MT_Vector3& pnorm)
	{
		// would be good to use the actual bounding box center instead
		MT_Point3 pos(ms->m_OpenGLMatrix[12], ms->m_OpenGLMatrix[13], ms->m_OpenGLMatrix[14]);

		m_z = MT_dot(pnorm, pos);
		m_ms = ms;
		m_bucket = bucket;
	}
};

struct RAS_BucketManager::backtofront
{
	bool operator()(const sortedmeshslot &a, const sortedmeshslot &b)
	{
		return (a.m_z < b.m_z) || (a.m_z == b.m_z && a.m_ms < b.m_ms);
	}
};

struct RAS_BucketManager::fronttoback
{
	bool operator()(const sortedmeshslot &a, const sortedmeshslot &b)
	{
		return (a.m_z > b.m_z) || (a.m_z == b.m_z && a.m_ms > b.m_ms);
	}
};

/* bucket manager */

RAS_BucketManager::RAS_BucketManager()
{

}

RAS_BucketManager::~RAS_BucketManager()
{
	BucketList::iterator it;

	for (it = m_SolidBuckets.begin(); it != m_SolidBuckets.end(); it++)
		delete (*it);

	for (it = m_AlphaBuckets.begin(); it != m_AlphaBuckets.end(); it++)
		delete(*it);
	
	m_SolidBuckets.clear();
	m_AlphaBuckets.clear();
}

void RAS_BucketManager::OrderBuckets(const MT_Transform& cameratrans, BucketList& buckets, vector<sortedmeshslot>& slots, bool alpha)
{
	BucketList::iterator bit;
	list<RAS_MeshSlot>::iterator mit;
	size_t size = 0, i = 0;

	/* Camera's near plane equation: pnorm.dot(point) + pval,
	 * but we leave out pval since it's constant anyway */
	const MT_Vector3 pnorm(cameratrans.getBasis()[2]);

	for (bit = buckets.begin(); bit != buckets.end(); ++bit)
		for (mit = (*bit)->msBegin(); mit != (*bit)->msEnd(); ++mit)
			if (!mit->IsCulled())
				size++;

	slots.resize(size);

	for (bit = buckets.begin(); bit != buckets.end(); ++bit)
		for (mit = (*bit)->msBegin(); mit != (*bit)->msEnd(); ++mit)
			if (!mit->IsCulled())
				slots[i++].set(&*mit, *bit, pnorm);
		
	if(alpha)
		sort(slots.begin(), slots.end(), backtofront());
	else
		sort(slots.begin(), slots.end(), fronttoback());
}

//static int TOTASLOT = 0;
//static int TOTSLOT = 0;

void RAS_BucketManager::RenderAlphaBuckets(
	const MT_Transform& cameratrans, RAS_IRasterizer* rasty, RAS_IRenderTools* rendertools)
{
	vector<sortedmeshslot> slots;
	vector<sortedmeshslot>::iterator sit;

	// Having depth masks disabled/enabled gives different artifacts in
	// case no sorting is done or is done inexact. For compatibility, we
	// disable it.
	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_DISABLED);

	OrderBuckets(cameratrans, m_AlphaBuckets, slots, true);
	
	for(sit=slots.begin(); sit!=slots.end(); ++sit) {
		rendertools->SetClientObject(rasty, sit->m_ms->m_clientObj);

		while(sit->m_bucket->ActivateMaterial(cameratrans, rasty, rendertools)) {
			sit->m_bucket->RenderMeshSlot(cameratrans, rasty, rendertools, *(sit->m_ms));
			//TOTASLOT++;
		}
	}

	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_ENABLED);
}

void RAS_BucketManager::RenderSolidBuckets(
	const MT_Transform& cameratrans, RAS_IRasterizer* rasty, RAS_IRenderTools* rendertools)
{
	BucketList::iterator bit;
	list<RAS_MeshSlot>::iterator mit;

	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_ENABLED);

	for (bit = m_SolidBuckets.begin(); bit != m_SolidBuckets.end(); ++bit) {
		for (mit = (*bit)->msBegin(); mit != (*bit)->msEnd(); ++mit) {
			if (mit->IsCulled())
				continue;

			rendertools->SetClientObject(rasty, mit->m_clientObj);

			while ((*bit)->ActivateMaterial(cameratrans, rasty, rendertools)) {
				(*bit)->RenderMeshSlot(cameratrans, rasty, rendertools, *mit);
				//TOTSLOT++;
			}
		}
	}
	
#if 0
	vector<sortedmeshslot> slots;
	vector<sortedmeshslot>::iterator sit;

	OrderBuckets(cameratrans, m_SolidBuckets, slots, false);

	for(sit=slots.begin(); sit!=slots.end(); ++sit) {
		rendertools->SetClientObject(rasty, sit->m_ms->m_clientObj);

		while(sit->m_bucket->ActivateMaterial(cameratrans, rasty, rendertools))
			sit->m_bucket->RenderMeshSlot(cameratrans, rasty, rendertools, *(sit->m_ms));
	}
#endif
}

void RAS_BucketManager::Renderbuckets(
	const MT_Transform& cameratrans, RAS_IRasterizer* rasty, RAS_IRenderTools* rendertools)
{
	// beginning each frame, clear (texture/material) caching information
	rasty->ClearCachingInfo();

	//TOTASLOT = 0;
	//TOTSLOT = 0;

	RenderSolidBuckets(cameratrans, rasty, rendertools);	
	RenderAlphaBuckets(cameratrans, rasty, rendertools);	

	//printf("total slots = %d = %d + %d\n", TOTSLOT + TOTASLOT, TOTSLOT, TOTASLOT);

	rendertools->SetClientObject(rasty, NULL);
}

RAS_MaterialBucket* RAS_BucketManager::FindBucket(RAS_IPolyMaterial * material, bool &bucketCreated)
{
	BucketList::iterator it;

	bucketCreated = false;

	for (it = m_SolidBuckets.begin(); it != m_SolidBuckets.end(); it++)
		if (*(*it)->GetPolyMaterial() == *material)
			return *it;
	
	for (it = m_AlphaBuckets.begin(); it != m_AlphaBuckets.end(); it++)
		if (*(*it)->GetPolyMaterial() == *material)
			return *it;
	
	RAS_MaterialBucket *bucket = new RAS_MaterialBucket(material);
	bucketCreated = true;

	if (bucket->IsAlpha())
		m_AlphaBuckets.push_back(bucket);
	else
		m_SolidBuckets.push_back(bucket);
	
	return bucket;
}

void RAS_BucketManager::OptimizeBuckets(MT_Scalar distance)
{
	BucketList::iterator bit;
	
	distance = 10.0;

	for (bit = m_SolidBuckets.begin(); bit != m_SolidBuckets.end(); ++bit)
		(*bit)->Optimize(distance);
	for (bit = m_AlphaBuckets.begin(); bit != m_AlphaBuckets.end(); ++bit)
		(*bit)->Optimize(distance);
}

void RAS_BucketManager::ReleaseDisplayLists(RAS_IPolyMaterial *mat)
{
	BucketList::iterator bit;
	list<RAS_MeshSlot>::iterator mit;

	for (bit = m_SolidBuckets.begin(); bit != m_SolidBuckets.end(); ++bit) {
		if (mat == NULL || (mat == (*bit)->GetPolyMaterial())) {
			for (mit = (*bit)->msBegin(); mit != (*bit)->msEnd(); ++mit) {
				if(mit->m_DisplayList) {
					mit->m_DisplayList->Release();
					mit->m_DisplayList = NULL;
				}
			}
		}
	}
	
	for (bit = m_AlphaBuckets.begin(); bit != m_AlphaBuckets.end(); ++bit) {
		if (mat == NULL || (mat == (*bit)->GetPolyMaterial())) {
			for (mit = (*bit)->msBegin(); mit != (*bit)->msEnd(); ++mit) {
				if(mit->m_DisplayList) {
					mit->m_DisplayList->Release();
					mit->m_DisplayList = NULL;
				}
			}
		}
	}
}

