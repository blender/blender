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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "BL_Action.h"
#include "BL_ArmatureObject.h"
#include "KX_GameObject.h"

// These three are for getting the action from the logic manager
#include "KX_Scene.h"
#include "KX_PythonInit.h"
#include "SCA_LogicManager.h"

extern "C" {
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "RNA_access.h"
#include "RNA_define.h"
}

BL_Action::BL_Action(class KX_GameObject* gameobj,
					const char* name,
					float start,
					float end,
					float blendin,
					short play_mode,
					short blend_mode)
:
	m_obj(gameobj),
	m_startframe(start),
	m_endframe(end),
	m_blendin(blendin),
	m_playmode(play_mode),
	m_endtime(0.f),
	m_localtime(start),
	m_blendframe(0.f),
	m_blendstart(0.f),
	m_pose(NULL),
	m_blendpose(NULL),
	m_done(false)
{
	m_starttime = KX_GetActiveEngine()->GetFrameTime();
	m_action = (bAction*)KX_GetActiveScene()->GetLogicManager()->GetActionByName(name);

	if (!m_action) printf("Failed to load action: %s\n", name);
}

BL_Action::~BL_Action()
{
	if (m_pose)
		game_free_pose(m_pose);
	if (m_blendpose)
		game_free_pose(m_blendpose);
}

void BL_Action::SetLocalTime(float curtime)
{
	float dt = (curtime-m_starttime)*KX_KetsjiEngine::GetAnimFrameRate();

	if (m_endframe < m_startframe)
		dt = -dt;

	m_localtime = m_startframe + dt;
}

void BL_Action::Update(float curtime)
{
	curtime -= KX_KetsjiEngine::GetSuspendedDelta();

	SetLocalTime(curtime);

	// Handle wrap around
	if (m_localtime < m_startframe || m_localtime > m_endframe)
	{
		switch(m_playmode)
		{
		case ACT_MODE_PLAY:
			// Clamp
			m_localtime = m_endframe;
			m_done = true;
			break;
		case ACT_MODE_LOOP:
			// Put the time back to the beginning
			m_localtime = m_startframe;
			m_starttime = curtime;
			break;
		case ACT_MODE_PING_PONG:
			// Swap the start and end frames
			float temp = m_startframe;
			m_startframe = m_endframe;
			m_endframe = temp;

			m_starttime = curtime;

			break;
		}
	}

	if (m_obj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE)
	{
		bPose* prev_pose = NULL;
		BL_ArmatureObject *obj = (BL_ArmatureObject*)m_obj;
		obj->GetPose(&m_pose);

		// Save the old pose if we need to do some layer blending
		if (m_blendmode != ACT_BLEND_NONE)
			obj->GetMRDPose(&prev_pose);

		// Extract the pose from the action
		{
			struct PointerRNA id_ptr;
			Object *arm = obj->GetArmatureObject();
			bPose *temp = arm->pose;

			arm->pose = m_pose;
			RNA_id_pointer_create((ID*)arm, &id_ptr);
			animsys_evaluate_action(&id_ptr, m_action, NULL, m_localtime);

			arm->pose = temp;
		}

		// Handle blending between layers
		switch(m_blendmode)
		{
		case ACT_BLEND_MIX:
			game_blend_poses(m_pose, prev_pose, 0.5f);
			break;
		case ACT_BLEND_NONE:
		default:
			break;
		}

		// Handle blending between actions
		if (m_blendin && m_blendframe<m_blendin)
		{
			if (!m_blendpose)
			{
				obj->GetMRDPose(&m_blendpose);
				m_blendstart = curtime;
			}

			// Calculate weight
			float weight = 1.f - (m_blendframe/m_blendin);
			game_blend_poses(m_pose, m_blendpose, weight);

			// Bump the blend frame
			m_blendframe = (curtime - m_blendstart)*KX_KetsjiEngine::GetAnimFrameRate();

			// Clamp
			if (m_blendframe>m_blendin)
				m_blendframe = m_blendin;
		}
		else
		{
			if (m_blendpose)
			{
				game_free_pose(m_blendpose);
				m_blendpose = NULL;
			}
		}

		obj->SetPose(m_pose);

		obj->SetActiveAction(NULL, 0, curtime);

		if (prev_pose)
			game_free_pose(prev_pose);
	}
	else
	{
		printf("Only armature actions are currently supported\n");
	}
}
