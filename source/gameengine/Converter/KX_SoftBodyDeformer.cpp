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
#pragma warning (disable : 4786)
#endif //WIN32

#include "MT_assert.h"

#include "KX_ConvertPhysicsObject.h"
#include "KX_SoftBodyDeformer.h"
#include "RAS_MeshObject.h"
#include "GEN_Map.h"
#include "GEN_HashedPtr.h"

#ifdef USE_BULLET

#include "CcdPhysicsEnvironment.h"
#include "CcdPhysicsController.h"
#include "BulletSoftBody/btSoftBody.h"

#include "KX_BulletPhysicsController.h"
#include "btBulletDynamicsCommon.h"

void KX_SoftBodyDeformer::Relink(GEN_Map<class GEN_HashedPtr, void*>*map)
{
	void **h_obj = (*map)[m_gameobj];

	if (h_obj) {
		m_gameobj = (BL_DeformableGameObject*)(*h_obj);
		m_pMeshObject = m_gameobj->GetMesh(0);
	} else {
		m_gameobj = NULL;
		m_pMeshObject = NULL;
	}
}

bool KX_SoftBodyDeformer::Apply(class RAS_IPolyMaterial *polymat)
{
	KX_BulletPhysicsController* ctrl = (KX_BulletPhysicsController*) m_gameobj->GetPhysicsController();
	if (!ctrl)
		return false;

	btSoftBody* softBody= ctrl->GetSoftBody();
	if (!softBody)
		return false;

	//printf("apply\n");
	RAS_MeshSlot::iterator it;
	RAS_MeshMaterial *mmat;
	RAS_MeshSlot *slot;
	size_t i;

	// update the vertex in m_transverts
	Update();

	// The vertex cache can only be updated for this deformer:
	// Duplicated objects with more than one ploymaterial (=multiple mesh slot per object)
	// share the same mesh (=the same cache). As the rendering is done per polymaterial
	// cycling through the objects, the entire mesh cache cannot be updated in one shot.
	mmat = m_pMeshObject->GetMeshMaterial(polymat);
	if(!mmat->m_slots[(void*)m_gameobj])
		return true;

	slot = *mmat->m_slots[(void*)m_gameobj];

	// for each array
	for(slot->begin(it); !slot->end(it); slot->next(it)) 
	{
		btSoftBody::tNodeArray&   nodes(softBody->m_nodes);

		int index = 0;
		for(i=it.startvertex; i<it.endvertex; i++,index++) {
			RAS_TexVert& v = it.vertex[i];
			btAssert(v.getSoftBodyIndex() >= 0);

			MT_Point3 pt (
				nodes[v.getSoftBodyIndex()].m_x.getX(),
				nodes[v.getSoftBodyIndex()].m_x.getY(),
				nodes[v.getSoftBodyIndex()].m_x.getZ());
			v.SetXYZ(pt);

			MT_Vector3 normal (
				nodes[v.getSoftBodyIndex()].m_n.getX(),
				nodes[v.getSoftBodyIndex()].m_n.getY(),
				nodes[v.getSoftBodyIndex()].m_n.getZ());
			v.SetNormal(normal);

		}
	}
	return true;
}

#endif
