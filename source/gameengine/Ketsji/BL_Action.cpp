/*
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
 * Contributor(s): Mitchell Stokes.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BL_Action.cpp
 *  \ingroup ketsji
 */

#include <cstdlib>
#include <stdio.h>

#include "BL_Action.h"
#include "BL_ArmatureObject.h"
#include "BL_DeformableGameObject.h"
#include "BL_ShapeDeformer.h"
#include "KX_IpoConvert.h"
#include "KX_GameObject.h"

#include "SG_Controller.h"

// These three are for getting the action from the logic manager
#include "KX_Scene.h"
#include "SCA_LogicManager.h"

extern "C" {
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "RNA_access.h"
#include "RNA_define.h"

// Needed for material IPOs
#include "BKE_material.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
}

#include "MEM_guardedalloc.h"
#include "BKE_library.h"
#include "BKE_global.h"

#include "BLI_threads.h" // for lock

/* Lock to solve animation thread issues.
 * A spin lock is better than a mutex in case of short wait 
 * because spin lock stop the thread by a loop contrary to mutex
 * which switch all memory, process.
 */ 
static SpinLock BL_ActionLock;

BL_Action::BL_Action(class KX_GameObject* gameobj)
:
	m_action(NULL),
	m_tmpaction(NULL),
	m_blendpose(NULL),
	m_blendinpose(NULL),
	m_obj(gameobj),
	m_startframe(0.f),
	m_endframe(0.f),
	m_localframe(0.f),
	m_blendin(0.f),
	m_blendframe(0.f),
	m_blendstart(0.f),
	m_speed(0.f),
	m_priority(0),
	m_playmode(ACT_MODE_PLAY),
	m_blendmode(ACT_BLEND_BLEND),
	m_ipo_flags(0),
	m_done(true),
	m_calc_localtime(true),
	m_initializedTime(false)
{
}

BL_Action::~BL_Action()
{
	if (m_blendpose)
		BKE_pose_free(m_blendpose);
	if (m_blendinpose)
		BKE_pose_free(m_blendinpose);
	ClearControllerList();

	if (m_tmpaction) {
		BKE_libblock_free(G.main, m_tmpaction);
		m_tmpaction = NULL;
	}
}

void BL_Action::ClearControllerList()
{
	// Clear out the controller list
	std::vector<SG_Controller*>::iterator it;
	for (it = m_sg_contr_list.begin(); it != m_sg_contr_list.end(); it++)
	{
		m_obj->GetSGNode()->RemoveSGController((*it));
		delete *it;
	}

	m_sg_contr_list.clear();
}

bool BL_Action::Play(const char* name,
					float start,
					float end,
					short priority,
					float blendin,
					short play_mode,
					float layer_weight,
					short ipo_flags,
					float playback_speed,
					short blend_mode)
{

	// Only start playing a new action if we're done, or if
	// the new action has a higher priority
	if (!IsDone() && priority > m_priority)
		return false;
	m_priority = priority;
	bAction* prev_action = m_action;

	KX_Scene* kxscene = m_obj->GetScene();

	// First try to load the action
	m_action = (bAction*)kxscene->GetLogicManager()->GetActionByName(name);
	if (!m_action)
	{
		printf("Failed to load action: %s\n", name);
		m_done = true;
		return false;
	}

	// If we have the same settings, don't play again
	// This is to resolve potential issues with pulses on sensors such as the ones
	// reported in bug #29412. The fix is here so it works for both logic bricks and Python.
	// However, this may eventually lead to issues where a user wants to override an already
	// playing action with the same action and settings. If this becomes an issue,
	// then this fix may have to be re-evaluated.
	if (!IsDone() && m_action == prev_action && m_startframe == start && m_endframe == end
			&& m_priority == priority && m_speed == playback_speed)
		return false;

	// Keep a copy of the action for threading purposes
	if (m_tmpaction) {
		BKE_libblock_free(G.main, m_tmpaction);
		m_tmpaction = NULL;
	}
	m_tmpaction = BKE_action_copy(G.main, m_action);

	// First get rid of any old controllers
	ClearControllerList();

	// Create an SG_Controller
	SG_Controller *sg_contr = BL_CreateIPO(m_action, m_obj, kxscene->GetSceneConverter());
	m_sg_contr_list.push_back(sg_contr);
	m_obj->GetSGNode()->AddSGController(sg_contr);
	sg_contr->SetObject(m_obj->GetSGNode());

	// World
	sg_contr = BL_CreateWorldIPO(m_action, kxscene->GetBlenderScene()->world, kxscene->GetSceneConverter());
	if (sg_contr) {
		m_sg_contr_list.push_back(sg_contr);
		m_obj->GetSGNode()->AddSGController(sg_contr);
		sg_contr->SetObject(m_obj->GetSGNode());
	}

	// Try obcolor
	sg_contr = BL_CreateObColorIPO(m_action, m_obj, kxscene->GetSceneConverter());
	if (sg_contr) {
		m_sg_contr_list.push_back(sg_contr);
		m_obj->GetSGNode()->AddSGController(sg_contr);
		sg_contr->SetObject(m_obj->GetSGNode());
	}

	// Now try materials
	if (m_obj->GetBlenderObject()->totcol==1) {
		Material *mat = give_current_material(m_obj->GetBlenderObject(), 1);
		if (mat) {
			sg_contr = BL_CreateMaterialIpo(m_action, mat, 0, m_obj, kxscene->GetSceneConverter());
			if (sg_contr) {
				m_sg_contr_list.push_back(sg_contr);
				m_obj->GetSGNode()->AddSGController(sg_contr);
				sg_contr->SetObject(m_obj->GetSGNode());
			}
		}
	} else {
		Material *mat;
		STR_HashedString matname;

		for (int matidx = 1; matidx <= m_obj->GetBlenderObject()->totcol; ++matidx) {
			mat = give_current_material(m_obj->GetBlenderObject(), matidx);
			if (mat) {
				matname = mat->id.name;
				sg_contr = BL_CreateMaterialIpo(m_action, mat, matname.hash(), m_obj, kxscene->GetSceneConverter());
				if (sg_contr) {
					m_sg_contr_list.push_back(sg_contr);
					m_obj->GetSGNode()->AddSGController(sg_contr);
					sg_contr->SetObject(m_obj->GetSGNode());
				}
			}
		}
	}

	// Extra controllers
	if (m_obj->GetGameObjectType() == SCA_IObject::OBJ_LIGHT)
	{
		sg_contr = BL_CreateLampIPO(m_action, m_obj, kxscene->GetSceneConverter());
		m_sg_contr_list.push_back(sg_contr);
		m_obj->GetSGNode()->AddSGController(sg_contr);
		sg_contr->SetObject(m_obj->GetSGNode());
	}
	else if (m_obj->GetGameObjectType() == SCA_IObject::OBJ_CAMERA)
	{
		sg_contr = BL_CreateCameraIPO(m_action, m_obj, kxscene->GetSceneConverter());
		m_sg_contr_list.push_back(sg_contr);
		m_obj->GetSGNode()->AddSGController(sg_contr);
		sg_contr->SetObject(m_obj->GetSGNode());
	}
	
	m_ipo_flags = ipo_flags;
	InitIPO();

	// Setup blendin shapes/poses
	if (m_obj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE)
	{
		BL_ArmatureObject *obj = (BL_ArmatureObject*)m_obj;
		obj->GetPose(&m_blendinpose);
	}
	else
	{
		BL_DeformableGameObject *obj = (BL_DeformableGameObject*)m_obj;
		BL_ShapeDeformer *shape_deformer = dynamic_cast<BL_ShapeDeformer*>(obj->GetDeformer());
		
		if (shape_deformer && shape_deformer->GetKey())
		{
			obj->GetShape(m_blendinshape);

			// Now that we have the previous blend shape saved, we can clear out the key to avoid any
			// further interference.
			KeyBlock *kb;
			for (kb=(KeyBlock *)shape_deformer->GetKey()->block.first; kb; kb=(KeyBlock *)kb->next)
				kb->curval = 0.f;
		}
	}

	// Now that we have an action, we have something we can play
	m_starttime = -1.f; // We get the start time on our first update
	m_startframe = m_localframe = start;
	m_endframe = end;
	m_blendin = blendin;
	m_playmode = play_mode;
	m_blendmode = blend_mode;
	m_blendframe = 0.f;
	m_blendstart = 0.f;
	m_speed = playback_speed;
	m_layer_weight = layer_weight;
	
	m_done = false;
	m_initializedTime = false;

	return true;
}

bool BL_Action::IsDone()
{
	return m_done;
}

void BL_Action::InitIPO()
{
	// Initialize the IPOs
	std::vector<SG_Controller*>::iterator it;
	for (it = m_sg_contr_list.begin(); it != m_sg_contr_list.end(); it++)
	{
		(*it)->SetOption(SG_Controller::SG_CONTR_IPO_RESET, true);
		(*it)->SetOption(SG_Controller::SG_CONTR_IPO_IPO_AS_FORCE, m_ipo_flags & ACT_IPOFLAG_FORCE);
		(*it)->SetOption(SG_Controller::SG_CONTR_IPO_IPO_ADD, m_ipo_flags & ACT_IPOFLAG_ADD);
		(*it)->SetOption(SG_Controller::SG_CONTR_IPO_LOCAL, m_ipo_flags & ACT_IPOFLAG_LOCAL);
	}
}

bAction *BL_Action::GetAction()
{
	return (IsDone()) ? NULL : m_action;
}

float BL_Action::GetFrame()
{
	return m_localframe;
}

const char *BL_Action::GetName()
{
	if (m_action != NULL) {
		return m_action->id.name + 2;
	}
	else {
		return "";
	}

	            
}

void BL_Action::SetFrame(float frame)
{
	// Clamp the frame to the start and end frame
	if (frame < min(m_startframe, m_endframe))
		frame = min(m_startframe, m_endframe);
	else if (frame > max(m_startframe, m_endframe))
		frame = max(m_startframe, m_endframe);
	
	m_localframe = frame;
	m_calc_localtime = false;
}

void BL_Action::SetPlayMode(short play_mode)
{
	m_playmode = play_mode;
}

void BL_Action::SetTimes(float start, float end)
{
	m_startframe = start;
	m_endframe = end;
}

void BL_Action::SetLocalTime(float curtime)
{
	float dt = (curtime-m_starttime)*(float)KX_KetsjiEngine::GetAnimFrameRate()*m_speed;

	if (m_endframe < m_startframe)
		dt = -dt;

	m_localframe = m_startframe + dt;
}

void BL_Action::ResetStartTime(float curtime)
{
	float dt = (m_localframe > m_startframe) ? m_localframe - m_startframe : m_startframe - m_localframe;

	m_starttime = curtime - dt / ((float)KX_KetsjiEngine::GetAnimFrameRate()*m_speed);
	SetLocalTime(curtime);
}

void BL_Action::IncrementBlending(float curtime)
{
	// Setup m_blendstart if we need to
	if (m_blendstart == 0.f)
		m_blendstart = curtime;
	
	// Bump the blend frame
	m_blendframe = (curtime - m_blendstart)*(float)KX_KetsjiEngine::GetAnimFrameRate();

	// Clamp
	if (m_blendframe>m_blendin)
		m_blendframe = m_blendin;
}


void BL_Action::BlendShape(Key* key, float srcweight, std::vector<float>& blendshape)
{
	vector<float>::const_iterator it;
	float dstweight;
	KeyBlock *kb;
	
	dstweight = 1.0F - srcweight;
	//printf("Dst: %f\tSrc: %f\n", srcweight, dstweight);
	for (it=blendshape.begin(), kb = (KeyBlock *)key->block.first; 
	     kb && it != blendshape.end();
	     kb = (KeyBlock *)kb->next, it++)
	{
		//printf("OirgKeys: %f\t%f\n", kb->curval, (*it));
		kb->curval = kb->curval * dstweight + (*it) * srcweight;
		//printf("NewKey: %f\n", kb->curval);
	}
	//printf("\n");
}

void BL_Action::Update(float curtime)
{
	// Don't bother if we're done with the animation
	if (m_done)
		return;

	curtime -= (float)KX_KetsjiEngine::GetSuspendedDelta();

	// Grab the start time here so we don't end up with a negative m_localframe when
	// suspending and resuming scenes.
	if (!m_initializedTime) {
		m_starttime = curtime;
		m_initializedTime = true;
	}

	if (m_calc_localtime)
		SetLocalTime(curtime);
	else
	{
		ResetStartTime(curtime);
		m_calc_localtime = true;
	}

	// Handle wrap around
	if (m_localframe < min(m_startframe, m_endframe) || m_localframe > max(m_startframe, m_endframe)) {
		switch (m_playmode) {
			case ACT_MODE_PLAY:
				// Clamp
				m_localframe = m_endframe;
				m_done = true;
				break;
			case ACT_MODE_LOOP:
				// Put the time back to the beginning
				m_localframe = m_startframe;
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
		BL_ArmatureObject *obj = (BL_ArmatureObject*)m_obj;

		if (m_layer_weight >= 0)
			obj->GetPose(&m_blendpose);

		// Extract the pose from the action
		obj->SetPoseByAction(m_tmpaction, m_localframe);

		// Handle blending between armature actions
		if (m_blendin && m_blendframe<m_blendin)
		{
			IncrementBlending(curtime);

			// Calculate weight
			float weight = 1.f - (m_blendframe/m_blendin);

			// Blend the poses
			obj->BlendInPose(m_blendinpose, weight, ACT_BLEND_BLEND);
		}


		// Handle layer blending
		if (m_layer_weight >= 0)
			obj->BlendInPose(m_blendpose, m_layer_weight, m_blendmode);

		obj->UpdateTimestep(curtime);
	}
	else
	{
		BL_DeformableGameObject *obj = (BL_DeformableGameObject*)m_obj;
		BL_ShapeDeformer *shape_deformer = dynamic_cast<BL_ShapeDeformer*>(obj->GetDeformer());

		// Handle shape actions if we have any
		if (shape_deformer && shape_deformer->GetKey())
		{
			Key *key = shape_deformer->GetKey();

			PointerRNA ptrrna;
			RNA_id_pointer_create(&key->id, &ptrrna);

			animsys_evaluate_action(&ptrrna, m_tmpaction, NULL, m_localframe);

			// Handle blending between shape actions
			if (m_blendin && m_blendframe < m_blendin)
			{
				IncrementBlending(curtime);

				float weight = 1.f - (m_blendframe/m_blendin);

				// We go through and clear out the keyblocks so there isn't any interference
				// from other shape actions
				KeyBlock *kb;
				for (kb=(KeyBlock *)key->block.first; kb; kb=(KeyBlock *)kb->next)
					kb->curval = 0.f;

				// Now blend the shape
				BlendShape(key, weight, m_blendinshape);
			}

			// Handle layer blending
			if (m_layer_weight >= 0)
			{
				obj->GetShape(m_blendshape);
				BlendShape(key, m_layer_weight, m_blendshape);
			}

			obj->SetActiveAction(NULL, 0, curtime);
		}
	}

	BLI_spin_lock(&BL_ActionLock);
	/* This function is not thread safe because of recursive scene graph transform
	 * updates on children. e.g: If an object and one of its children is animated,
	 * the both can write transform at the same time. A thread lock avoid problems. */
	m_obj->UpdateIPO(m_localframe, m_ipo_flags & ACT_IPOFLAG_CHILD);
	BLI_spin_unlock(&BL_ActionLock);

	if (m_done)
		ClearControllerList();
}

void BL_Action::InitLock()
{
	BLI_spin_init(&BL_ActionLock);
}

void BL_Action::EndLock()
{
	BLI_spin_end(&BL_ActionLock);
}
