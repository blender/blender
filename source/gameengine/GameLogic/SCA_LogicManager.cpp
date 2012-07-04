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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Regulates the top-level logic behavior for one scene.
 */

/** \file gameengine/GameLogic/SCA_LogicManager.cpp
 *  \ingroup gamelogic
 */

#include "Value.h"
#include "SCA_LogicManager.h"
#include "SCA_ISensor.h"
#include "SCA_IController.h"
#include "SCA_IActuator.h"
#include "SCA_EventManager.h"
#include "SCA_PythonController.h"
#include <set>


SCA_LogicManager::SCA_LogicManager()
{
}



SCA_LogicManager::~SCA_LogicManager()
{
	for (vector<SCA_EventManager*>::iterator it = m_eventmanagers.begin();!(it==m_eventmanagers.end());++it)
	{
		delete (*it);
	}
	m_eventmanagers.clear();
	assert(m_activeActuators.Empty());
}

#if 0
// this kind of fixes bug 398 but breakes games, so better leave it out for now.
// a removed object's gameobject (and logicbricks and stuff) didn't get released
// because it was still in the m_mapStringToGameObjects map.
void SCA_LogicManager::RemoveGameObject(const STR_String& gameobjname)
{
	int numgameobj = m_mapStringToGameObjects.size();
	for (int i = 0; i < numgameobj; i++)
	{
		CValue** gameobjptr = m_mapStringToGameObjects.at(i);
		assert(gameobjptr);

		if (gameobjptr)
		{
			if ((*gameobjptr)->GetName() == gameobjname)
				(*gameobjptr)->Release();
		}
	}

	m_mapStringToGameObjects.remove(gameobjname);
}
#endif


void SCA_LogicManager::RegisterEventManager(SCA_EventManager* eventmgr)
{
	m_eventmanagers.push_back(eventmgr);
}



void SCA_LogicManager::RegisterGameObjectName(const STR_String& gameobjname,
											  CValue* gameobj)
{
	STR_HashedString mn = gameobjname;
	m_mapStringToGameObjects.insert(mn,gameobj);
}



void SCA_LogicManager::RegisterGameMeshName(const STR_String& gamemeshname, void* blendobj)
{
	STR_HashedString mn = gamemeshname;
	m_map_gamemeshname_to_blendobj.insert(mn, blendobj);
}



void SCA_LogicManager::RegisterGameObj(void* blendobj, CValue* gameobj) 
{
	m_map_blendobj_to_gameobj.insert(CHashedPtr(blendobj), gameobj);
}

void SCA_LogicManager::UnregisterGameObj(void* blendobj, CValue* gameobj) 
{
	void **obp = m_map_blendobj_to_gameobj[CHashedPtr(blendobj)];
	if (obp && (CValue*)(*obp) == gameobj)
		m_map_blendobj_to_gameobj.remove(CHashedPtr(blendobj));
}

CValue* SCA_LogicManager::GetGameObjectByName(const STR_String& gameobjname)
{
	STR_HashedString mn = gameobjname;
	CValue** gameptr = m_mapStringToGameObjects[mn];
	
	if (gameptr)
		return *gameptr;

	return NULL;
}


CValue* SCA_LogicManager::FindGameObjByBlendObj(void* blendobj) 
{
	void **obp= m_map_blendobj_to_gameobj[CHashedPtr(blendobj)];
	return obp?(CValue*)(*obp):NULL;
}



void* SCA_LogicManager::FindBlendObjByGameMeshName(const STR_String& gamemeshname) 
{
	STR_HashedString mn = gamemeshname;
	void **obp= m_map_gamemeshname_to_blendobj[mn];
	return obp?*obp:NULL;
}



void SCA_LogicManager::RemoveSensor(SCA_ISensor* sensor)
{
	sensor->UnlinkAllControllers();
	sensor->UnregisterToManager();
}

void SCA_LogicManager::RemoveController(SCA_IController* controller)
{
	controller->UnlinkAllSensors();
	controller->UnlinkAllActuators();
	controller->Deactivate();
}


void SCA_LogicManager::RemoveActuator(SCA_IActuator* actuator)
{
	actuator->UnlinkAllControllers();
	actuator->Deactivate();
	actuator->SetActive(false);
}



void SCA_LogicManager::RegisterToSensor(SCA_IController* controller,SCA_ISensor* sensor)
{
	sensor->LinkToController(controller);
	controller->LinkToSensor(sensor);
}



void SCA_LogicManager::RegisterToActuator(SCA_IController* controller,SCA_IActuator* actua)
{
	actua->LinkToController(controller);
	controller->LinkToActuator(actua);
}



void SCA_LogicManager::BeginFrame(double curtime, double fixedtime)
{
	for (vector<SCA_EventManager*>::const_iterator ie=m_eventmanagers.begin(); !(ie==m_eventmanagers.end()); ie++)
		(*ie)->NextFrame(curtime, fixedtime);

	for (SG_QList* obj = (SG_QList*)m_triggeredControllerSet.Remove();
		obj != NULL;
		obj = (SG_QList*)m_triggeredControllerSet.Remove())
	{
		for (SCA_IController* contr = (SCA_IController*)obj->QRemove();
			contr != NULL;
			contr = (SCA_IController*)obj->QRemove())
		{
			contr->Trigger(this);
			contr->ClrJustActivated();
		}
	}
}



void SCA_LogicManager::UpdateFrame(double curtime, bool frame)
{
	for (vector<SCA_EventManager*>::const_iterator ie=m_eventmanagers.begin(); !(ie==m_eventmanagers.end()); ie++)
		(*ie)->UpdateFrame();

	SG_DList::iterator<SG_QList> io(m_activeActuators);
	for (io.begin(); !io.end(); )
	{
		SG_QList* ahead = *io;
		// increment now so that we can remove the current element
		++io;
		SG_QList::iterator<SCA_IActuator> ia(*ahead);
		for (ia.begin(); !ia.end();  )
		{
			SCA_IActuator* actua = *ia;
			// increment first to allow removal of inactive actuators.
			++ia;
			if (!actua->Update(curtime, frame))
			{
				// this actuator is not active anymore, remove
				actua->QDelink(); 
				actua->SetActive(false); 
			} else if (actua->IsNoLink())
			{
				// This actuator has no more links but it still active
				// make sure it will get a negative event on next frame to stop it
				// Do this check after Update() rather than before to make sure
				// that all the actuators that are activated at same time than a state
				// actuator have a chance to execute. 
				bool event = false;
				actua->RemoveAllEvents();
				actua->AddEvent(event);
			}
		}
		if (ahead->QEmpty())
		{
			// no more active controller, remove from main list
			ahead->Delink();
		}
	}
}



void* SCA_LogicManager::GetActionByName (const STR_String& actname)
{
	STR_HashedString an = actname;
	void** actptr = m_mapStringToActions[an];

	if (actptr)
		return *actptr;

	return NULL;
}



void* SCA_LogicManager::GetMeshByName(const STR_String& meshname)
{
	STR_HashedString mn = meshname;
	void** meshptr = m_mapStringToMeshes[mn];

	if (meshptr)
		return *meshptr;

	return NULL;
}



void SCA_LogicManager::RegisterMeshName(const STR_String& meshname,void* mesh)
{
	STR_HashedString mn = meshname;
	m_mapStringToMeshes.insert(mn,mesh);
}

void SCA_LogicManager::UnregisterMeshName(const STR_String& meshname,void* mesh)
{
	STR_HashedString mn = meshname;
	m_mapStringToMeshes.remove(mn);
}


void SCA_LogicManager::RegisterActionName(const STR_String& actname,void* action)
{
	STR_HashedString an = actname;
	m_mapStringToActions.insert(an, action);
}



void SCA_LogicManager::EndFrame()
{
	for (vector<SCA_EventManager*>::const_iterator ie=m_eventmanagers.begin();
	!(ie==m_eventmanagers.end());ie++)
	{
		(*ie)->EndFrame();
	}
}


void SCA_LogicManager::AddTriggeredController(SCA_IController* controller, SCA_ISensor* sensor)
{
	controller->Activate(m_triggeredControllerSet);

#ifdef WITH_PYTHON

	// so that the controller knows which sensor has activited it
	// only needed for python controller
	// Note that this is safe even if the controller is subclassed.
	if (controller->GetType() == &SCA_PythonController::Type)
	{
		SCA_PythonController* pythonController = (SCA_PythonController*)controller;
		pythonController->AddTriggeredSensor(sensor);
	}
#endif
}

SCA_EventManager* SCA_LogicManager::FindEventManager(int eventmgrtype)
{
	// find an eventmanager of a certain type
	SCA_EventManager* eventmgr = NULL;

	for (vector<SCA_EventManager*>::const_iterator i=
	m_eventmanagers.begin();!(i==m_eventmanagers.end());i++)
	{
		SCA_EventManager* emgr = *i;
		if (emgr->GetType() == eventmgrtype)
		{
			eventmgr = emgr;
			break;
		}
	}
	return eventmgr;
}
