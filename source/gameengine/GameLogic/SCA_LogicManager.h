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
 * Regulates the top-level logic behaviour for one scene.
 */
#ifndef __KX_LOGICMANAGER
#define __KX_LOGICMANAGER

#ifdef WIN32
#pragma warning (disable:4786)
#endif 

#include <vector>
//#include "GEN_Map.h"
#include <set>
#include <map>
#include <list>

#include "GEN_Map.h"
#include "STR_HashedString.h"
#include "Value.h"

#include "KX_HashedPtr.h"

using namespace std;
typedef list<class SCA_IController*> controllerlist;

/** 
 * This manager handles sensor, controllers and actuators.
 * logic executes each frame the following way:
 * find triggering sensors
 * build list of controllers that are triggered by these triggering sensors
 * process all triggered controllers
 * during this phase actuators can be added to the active actuator list
 * process all active actuators
 * clear triggering sensors
 * clear triggered controllers
 * (actuators may be active during a longer timeframe)
*/

#include "SCA_ILogicBrick.h"

// todo: make this into a template, but first I want to think about what exactly to put in
class	SmartActuatorPtr
{
	SCA_IActuator*	m_actuator;
public:
	SmartActuatorPtr(SCA_IActuator* actua,int dummy);
	SmartActuatorPtr(const SmartActuatorPtr& other);
	virtual ~SmartActuatorPtr();
	bool operator <(const SmartActuatorPtr& other) const;
	bool operator ==(const SmartActuatorPtr& other) const;
	SCA_IActuator*	operator->() const;
	SCA_IActuator* operator*() const;

};

class	SmartControllerPtr
{
	SCA_IController*	m_controller;
public:
	SmartControllerPtr(const SmartControllerPtr& copy);
	SmartControllerPtr(SCA_IController* contr,int dummy);
	virtual ~SmartControllerPtr();
	bool	operator <(const SmartControllerPtr& other) const;
	bool	operator ==(const SmartControllerPtr& other) const;
	SCA_IController*	operator->() const;
	SCA_IController* 	operator*() const; 

};

class SCA_LogicManager
{
	vector<class SCA_EventManager*>		m_eventmanagers;
	
	vector<class SCA_ISensor*>			m_activatedsensors;
	set<class SmartActuatorPtr>			m_activeActuators;
	set<class SmartControllerPtr>		m_triggeredControllerSet;

	map<SCA_ISensor*,controllerlist >	m_sensorcontrollermapje;

	// need to find better way for this
	// also known as FactoryManager...
	GEN_Map<STR_HashedString,CValue*>	m_mapStringToGameObjects;
	GEN_Map<STR_HashedString,void*>		m_mapStringToMeshes;
	GEN_Map<STR_HashedString,void*>		m_mapStringToActions;

	GEN_Map<STR_HashedString,void*>		m_map_gamemeshname_to_blendobj;
	GEN_Map<CHashedPtr,void*>			m_map_blendobj_to_gameobj;

	vector<SmartActuatorPtr>			m_removedActuators;
public:
	SCA_LogicManager();
	virtual ~SCA_LogicManager();
	//void	SetKeyboardManager(SCA_KeyboardManager* keyboardmgr) { m_keyboardmgr=keyboardmgr;}
	void	RegisterEventManager(SCA_EventManager* eventmgr);
	void	RegisterToSensor(SCA_IController* controller,
							 class SCA_ISensor* sensor);
	void	RegisterToActuator(SCA_IController* controller,
							   class SCA_IActuator* actuator);
	
	void	BeginFrame(double curtime, double fixedtime);
	void	UpdateFrame(double curtime, bool frame);
	void	EndFrame();
	void	AddActivatedSensor(SCA_ISensor* sensor);
	void	AddActiveActuator(SCA_IActuator* sensor,class CValue* event);
	void	AddTriggeredController(SCA_IController* controller, SCA_ISensor* sensor);
	SCA_EventManager*	FindEventManager(int eventmgrtype);
	
	void	RemoveGameObject(const STR_String& gameobjname);

	/**
	* remove Logic Bricks from the running logicmanager
	*/
	void	RemoveSensor(SCA_ISensor* sensor);
	void	RemoveController(SCA_IController* controller);
	void	RemoveDestroyedActuator(SCA_IActuator* actuator);
	

	// for the scripting... needs a FactoryManager later (if we would have time... ;)
	void	RegisterMeshName(const STR_String& meshname,void* mesh);
	void	RegisterActionName(const STR_String& actname,void* action);

	void*	GetActionByName (const STR_String& actname);
	void*	GetMeshByName(const STR_String& meshname);

	void	RegisterGameObjectName(const STR_String& gameobjname,CValue* gameobj);
	class CValue*	GetGameObjectByName(const STR_String& gameobjname);

	void	RegisterGameMeshName(const STR_String& gamemeshname, void* blendobj);
	void*	FindBlendObjByGameMeshName(const STR_String& gamemeshname);

	void	RegisterGameObj(void* blendobj, CValue* gameobj);
	void	UnregisterGameObj(void* blendobj, CValue* gameobj);
	CValue*	FindGameObjByBlendObj(void* blendobj);
};

#endif //__KX_LOGICMANAGER

