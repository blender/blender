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

#include "BL_DeformableGameObject.h"
#include "BL_ShapeDeformer.h"
#include "BL_ShapeActionActuator.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

BL_DeformableGameObject::~BL_DeformableGameObject()
{
	if (m_pDeformer)
		delete m_pDeformer;		//	__NLA : Temporary until we decide where to put this
}

void BL_DeformableGameObject::ProcessReplica(KX_GameObject* replica)
{
	BL_MeshDeformer *deformer;
	KX_GameObject::ProcessReplica(replica);

	if (m_pDeformer) {
		deformer = (BL_MeshDeformer*)m_pDeformer->GetReplica(replica);
		((BL_DeformableGameObject*)replica)->m_pDeformer = deformer;
	}

}

CValue*		BL_DeformableGameObject::GetReplica()
{

	BL_DeformableGameObject* replica = new BL_DeformableGameObject(*this);//m_float,GetName());
	
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	ProcessReplica(replica);
	return replica;
}

bool BL_DeformableGameObject::SetActiveAction(BL_ShapeActionActuator *act, short priority, double curtime)
{
	if (curtime != m_lastframe){
		m_activePriority = 9999;
		m_lastframe= curtime;
		m_activeAct = NULL;
	}

	if (priority<=m_activePriority)
	{
		if (m_activeAct && (m_activeAct!=act))
			m_activeAct->SetBlendTime(0.0f);	/* Reset the blend timer */
		m_activeAct = act;
		m_activePriority = priority;
		m_lastframe = curtime;
	
		return true;
	}
	else{
		act->SetBlendTime(0.0f);
		return false;
	}
}

bool BL_DeformableGameObject::GetShape(vector<float> &shape)
{
	shape.clear();
	if (m_pDeformer)
	{
		Mesh* mesh = ((BL_MeshDeformer*)m_pDeformer)->GetMesh();
		// this check is normally superfluous: a shape deformer can only be created if the mesh
		// has relative keys
		if (mesh && mesh->key && mesh->key->type==KEY_RELATIVE) 
		{
			KeyBlock *kb;
			for (kb = (KeyBlock*)mesh->key->block.first; kb; kb = (KeyBlock*)kb->next)
			{
				shape.push_back(kb->curval);
			}
		}
	}
	return !shape.empty();
}

