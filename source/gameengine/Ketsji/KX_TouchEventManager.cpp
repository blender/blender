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

#include "KX_TouchEventManager.h"
#include "SCA_ISensor.h"
#include "KX_TouchSensor.h"
#include "KX_GameObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "SM_Object.h"

KX_TouchEventManager::KX_TouchEventManager(class SCA_LogicManager* logicmgr,
	SM_Scene *scene)
	: SCA_EventManager(TOUCH_EVENTMGR),
	  m_logicmgr(logicmgr),
	  m_scene(scene)
{
	//m_scene->addTouchCallback(STATIC_RESPONSE, KX_TouchEventManager::collisionResponse, this);
	m_scene->addTouchCallback(OBJECT_RESPONSE, KX_TouchEventManager::collisionResponse, this);
	m_scene->addTouchCallback(SENSOR_RESPONSE, KX_TouchEventManager::collisionResponse, this);
}

DT_Bool KX_TouchEventManager::HandleCollision(void* object1,void* object2,
						 const DT_CollData * coll_data)
{
	SM_Object * obj1 = (SM_Object *) object1;
	SM_Object * obj2 = (SM_Object *) object2;

	for ( vector<SCA_ISensor*>::iterator it = m_sensors.begin(); !(it==m_sensors.end()); it++)
	{
		KX_GameObject* gameobj = ((KX_GameObject*)((KX_TouchSensor*)*it)->GetParent());
		KX_ClientObjectInfo *client_info = (KX_ClientObjectInfo *) obj1->getClientObject();
// Enable these printfs to create excesive debug info
//		printf("KX_TEM::HC: Sensor %s\tGO: %p o1: %s (%p)", (const char *) (*it)->GetName(), gameobj, (const char *) ((KX_GameObject *) client_info->m_clientobject)->GetName(), client_info->m_clientobject);
		if (client_info && client_info->m_clientobject == gameobj)
			((KX_TouchSensor*)*it)->HandleCollision(object1,object2,coll_data);

		client_info = (KX_ClientObjectInfo *) obj2->getClientObject();
//		printf(" o2: %s (%p)\n", (const char *) ((KX_GameObject *) client_info->m_clientobject)->GetName(), client_info->m_clientobject);
		if (client_info && client_info->m_clientobject == gameobj)
			 ((KX_TouchSensor*)*it)->HandleCollision(object1,object2,coll_data);

	}
	
	return DT_CONTINUE;
}

DT_Bool KX_TouchEventManager::collisionResponse(void *client_data, 
							void *object1,
							void *object2,
							const DT_CollData *coll_data)
{
	KX_TouchEventManager *touchmgr = (KX_TouchEventManager *) client_data;
	touchmgr->HandleCollision(object1, object2, coll_data);
	return DT_CONTINUE;
}

void KX_TouchEventManager::RegisterSensor(SCA_ISensor* sensor)
{
	KX_TouchSensor* touchsensor = static_cast<KX_TouchSensor*>(sensor);
	m_sensors.push_back(touchsensor);

	touchsensor->RegisterSumo(this);
}



void KX_TouchEventManager::EndFrame()
{
	vector<SCA_ISensor*>::iterator it;
	for ( it = m_sensors.begin();
	!(it==m_sensors.end());it++)
	{
		((KX_TouchSensor*)*it)->EndFrame();

	}
}



void KX_TouchEventManager::NextFrame(double curtime,double deltatime)
{
	if (m_sensors.size() > 0)
	{
		vector<SCA_ISensor*>::iterator it;
		
		for (it = m_sensors.begin();!(it==m_sensors.end());it++)
			static_cast<KX_TouchSensor*>(*it)->SynchronizeTransform();
		
		for (it = m_sensors.begin();!(it==m_sensors.end());it++)
			(*it)->Activate(m_logicmgr,NULL);
	}
}



void KX_TouchEventManager::RemoveSensor(class SCA_ISensor* sensor)
{
	std::vector<SCA_ISensor*>::iterator i =
	std::find(m_sensors.begin(), m_sensors.end(), sensor);
	if (!(i == m_sensors.end()))
	{
		std::swap(*i, m_sensors.back());
		m_sensors.pop_back();
	}
	// remove the sensor forever :)
	SCA_EventManager::RemoveSensor(sensor);
}

