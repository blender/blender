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
// this will be put in a class later on

#ifndef __RAS_BUCKETMANAGER
#define __RAS_BUCKETMANAGER

#include "MT_Transform.h"
#include "RAS_MaterialBucket.h"
#include "GEN_Map.h"

#include <vector>

class RAS_BucketManager
{
	//GEN_Map<class RAS_IPolyMaterial,class RAS_MaterialBucket*> m_MaterialBuckets;
	
	typedef std::vector<class RAS_MaterialBucket*> BucketList;
	BucketList m_MaterialBuckets;
	BucketList m_AlphaBuckets;

	/**
	 * struct alphamesh holds a mesh, (m_ms) it's depth, (m_z) and the bucket it came from (m_bucket.)
	 */
	struct alphamesh
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
	
	struct backtofront
	{
		bool operator()(const alphamesh &a, const alphamesh &b)
		{
			return a.m_z < b.m_z;
		}
	};
	

public:
	RAS_BucketManager();
	virtual ~RAS_BucketManager();

	void RenderAlphaBuckets(const MT_Transform& cameratrans, 
		RAS_IRasterizer* rasty, RAS_IRenderTools* rendertools);
	void Renderbuckets(const MT_Transform & cameratrans,
							RAS_IRasterizer* rasty,
							class RAS_IRenderTools* rendertools);

	RAS_MaterialBucket* RAS_BucketManagerFindBucket(RAS_IPolyMaterial * material);
	

private:
	void RAS_BucketManagerClearAll();

};

#endif //__RAS_BUCKETMANAGER

