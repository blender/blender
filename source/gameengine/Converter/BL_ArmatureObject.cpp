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

#include "BL_ArmatureObject.h"
#include "BL_ActionActuator.h"
#include "BLI_blenlib.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "GEN_Map.h"
#include "GEN_HashedPtr.h"
#include "MEM_guardedalloc.h"
#include "DNA_action_types.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

CValue* BL_ArmatureObject::GetReplica()
{
	BL_ArmatureObject* replica = new BL_ArmatureObject(*this);
	
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	ProcessReplica(replica);
	return replica;
}

void BL_ArmatureObject::ProcessReplica(BL_ArmatureObject *replica)
{
	KX_GameObject::ProcessReplica(replica);

}

BL_ArmatureObject::~BL_ArmatureObject()
{
	if (m_mrdPose){
		clear_pose(m_mrdPose);
		MEM_freeN(m_mrdPose);
	}
}

void BL_ArmatureObject::ApplyPose()
{
	if (m_pose){
		apply_pose_armature(m_armature, m_pose, 1);
		if (!m_mrdPose)
			copy_pose (&m_mrdPose, m_pose, 0);
		else
			get_pose_from_pose(&m_mrdPose, m_pose);
	}
}

void BL_ArmatureObject::SetPose(bPose *pose)
{
	m_pose = pose;
}

bool BL_ArmatureObject::SetActiveAction(BL_ActionActuator *act, short priority, double curtime)
{
	if (curtime != m_lastframe){
		m_activePriority = 9999;
		m_lastframe= curtime;
		m_activeAct = NULL;
	}

	if (priority<=m_activePriority)
	{
		if (m_activeAct && (m_activeAct!=act))
			m_activeAct->SetBlendTime(0.0);	/* Reset the blend timer */
		m_activeAct = act;
		m_activePriority = priority;
		m_lastframe = curtime;
	
		return true;
	}
	else{
		act->SetBlendTime(0.0);
		return false;
	}
	
}

BL_ActionActuator * BL_ArmatureObject::GetActiveAction()
{
	return m_activeAct;
}

void BL_ArmatureObject::GetPose(bPose **pose)
{
	/* If the caller supplies a null pose, create a new one. */
	/* Otherwise, copy the armature's pose channels into the caller-supplied pose */
	if (!*pose)
		copy_pose(pose, m_pose, 0);
	else
		get_pose_from_pose(pose, m_pose);

}

void BL_ArmatureObject::GetMRDPose(bPose **pose)
{
	/* If the caller supplies a null pose, create a new one. */
	/* Otherwise, copy the armature's pose channels into the caller-supplied pose */

	if (!m_mrdPose){
		copy_pose (&m_mrdPose, m_pose, 0);
	}

	if (!*pose)
		copy_pose(pose, m_mrdPose, 0);
	else
		get_pose_from_pose(pose, m_mrdPose);

}

short BL_ArmatureObject::GetActivePriority()
{
	return m_activePriority;
}

double BL_ArmatureObject::GetLastFrame()
{
	return m_lastframe;
}
