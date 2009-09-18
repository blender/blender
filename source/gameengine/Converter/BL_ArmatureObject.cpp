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

#include "BL_ArmatureObject.h"
#include "BL_ActionActuator.h"
#include "BLI_blenlib.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "GEN_Map.h"
#include "GEN_HashedPtr.h"
#include "MEM_guardedalloc.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MT_Matrix4x4.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

BL_ArmatureObject::BL_ArmatureObject(
				void* sgReplicationInfo, 
				SG_Callbacks callbacks, 
				Object *armature,
				Scene *scene)

:	KX_GameObject(sgReplicationInfo,callbacks),
	m_objArma(armature),
	m_framePose(NULL),
	m_scene(scene), // maybe remove later. needed for where_is_pose
	m_lastframe(0.0),
	m_activeAct(NULL),
	m_activePriority(999),
	m_lastapplyframe(0.0)
{
	m_armature = (bArmature *)armature->data;

	/* we make a copy of blender object's pose, and then always swap it with
	 * the original pose before calling into blender functions, to deal with
	 * replica's or other objects using the same blender object */
	m_pose = NULL;
	game_copy_pose(&m_pose, m_objArma->pose);
}

CValue* BL_ArmatureObject::GetReplica()
{
	BL_ArmatureObject* replica = new BL_ArmatureObject(*this);
	replica->ProcessReplica();
	return replica;
}

void BL_ArmatureObject::ProcessReplica()
{
	bPose *pose= m_pose;
	KX_GameObject::ProcessReplica();

	m_pose = NULL;
	game_copy_pose(&m_pose, pose);
}

BL_ArmatureObject::~BL_ArmatureObject()
{
	if (m_pose)
		game_free_pose(m_pose);
}

void BL_ArmatureObject::ApplyPose()
{
	m_armpose = m_objArma->pose;
	m_objArma->pose = m_pose;
	//m_scene->r.cfra++;
	if(m_lastapplyframe != m_lastframe) {
		where_is_pose(m_scene, m_objArma); // XXX
		m_lastapplyframe = m_lastframe;
	}
}

void BL_ArmatureObject::RestorePose()
{
	m_objArma->pose = m_armpose;
	m_armpose = NULL;
}

void BL_ArmatureObject::SetPose(bPose *pose)
{
	extract_pose_from_pose(m_pose, pose);
	m_lastapplyframe = -1.0;
}

bool BL_ArmatureObject::SetActiveAction(BL_ActionActuator *act, short priority, double curtime)
{
	if (curtime != m_lastframe){
		m_activePriority = 9999;
		m_lastframe= curtime;
		m_activeAct = NULL;
		// remember the pose at the start of the frame
		m_framePose = m_pose;
	}

	if (priority<=m_activePriority)
	{
		if (priority<m_activePriority)
			// this action overwrites the previous ones, start from initial pose to cancel their effects
			m_pose = m_framePose;
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
		
	if (!*pose) {
		/*	probably not to good of an idea to
			duplicate everying, but it clears up 
			a crash and memory leakage when 
			&BL_ActionActuator::m_pose is freed
		*/
		game_copy_pose(pose, m_pose);
	}
	else {
		if (*pose == m_pose)
			// no need to copy if the pointers are the same
			return;

		extract_pose_from_pose(*pose, m_pose);
	}
}

void BL_ArmatureObject::GetMRDPose(bPose **pose)
{
	/* If the caller supplies a null pose, create a new one. */
	/* Otherwise, copy the armature's pose channels into the caller-supplied pose */

	if (!*pose)
		game_copy_pose(pose, m_pose);
	else
		extract_pose_from_pose(*pose, m_pose);
}

short BL_ArmatureObject::GetActivePriority()
{
	return m_activePriority;
}

double BL_ArmatureObject::GetLastFrame()
{
	return m_lastframe;
}

bool BL_ArmatureObject::GetBoneMatrix(Bone* bone, MT_Matrix4x4& matrix)
{
	bPoseChannel *pchan;

	ApplyPose();
	pchan = get_pose_channel(m_objArma->pose, bone->name);
	if(pchan)
		matrix.setValue(&pchan->pose_mat[0][0]);
	RestorePose();

	return (pchan != NULL);
}

float BL_ArmatureObject::GetBoneLength(Bone* bone) const
{
	return (float)(MT_Point3(bone->head) - MT_Point3(bone->tail)).length();
}
