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
	int numbuckets = m_MaterialBuckets.size();
		
	//default_gl_light();

	int i;
	
	rasty->EnableTextures(false);
	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_ENABLED);
	
	// beginning each frame, clear (texture/material) caching information
	rasty->ClearCachingInfo();

	RAS_MaterialBucket::StartFrame();

	for (i=0;i<numbuckets;i++)
	{
		RAS_MaterialBucket** bucketptr = m_MaterialBuckets.at(i);
		if (bucketptr)
		{
			(*bucketptr)->ClearScheduledPolygons();
		}
	}

	vector<RAS_MaterialBucket*> alphabuckets;

	// if no visibility method is define, everything is drawn
	
	for (i=0;i<numbuckets;i++)
	{
		RAS_MaterialBucket** bucketptr = m_MaterialBuckets.at(i);
		if (bucketptr)
		{
			if (!(*bucketptr)->IsTransparant())
			{
				(*bucketptr)->Render(cameratrans,rasty,rendertools);
			} else
			{
				alphabuckets.push_back(*bucketptr);
			}
		}
	}
	
	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_DISABLED);
	
	int numalphabuckets = alphabuckets.size();
	for (vector<RAS_MaterialBucket*>::const_iterator it=alphabuckets.begin();
	!(it==alphabuckets.end());it++)
	{
		(*it)->Render(cameratrans,rasty,rendertools);
	}

	alphabuckets.clear();	


	RAS_MaterialBucket::EndFrame();

	rasty->SetDepthMask(RAS_IRasterizer::KX_DEPTHMASK_ENABLED);
}

RAS_MaterialBucket* RAS_BucketManager::RAS_BucketManagerFindBucket(RAS_IPolyMaterial * material)
{

	RAS_MaterialBucket** bucketptr = 	m_MaterialBuckets[*material];
	RAS_MaterialBucket* bucket=NULL;
	if (!bucketptr)
	{
		bucket = new RAS_MaterialBucket(material);
		m_MaterialBuckets.insert(*material,bucket);

	} else
	{
		bucket = *bucketptr;
	}

	return bucket;
}

void RAS_BucketManager::RAS_BucketManagerClearAll()
{

	int numbuckets = m_MaterialBuckets.size();
	for (int i=0;i<numbuckets;i++)
	{
		RAS_MaterialBucket** bucketptr = m_MaterialBuckets.at(i);
		if (bucketptr)
		{
			delete (*bucketptr);
			*bucketptr=NULL;

		}
	}
	m_MaterialBuckets.clear();
	
}
