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

#ifdef PHYSICS_NOT_YET

KX_TouchEventManager::KX_TouchEventManager(class SCA_LogicManager* logicmgr,
										   DT_RespTableHandle resphandle,
										   DT_SceneHandle scenehandle)
	: SCA_EventManager(TOUCH_EVENTMGR),
	  m_resphandle(resphandle),
	  m_scenehandle(scenehandle),
	  m_logicmgr(logicmgr) {}

void KX_TouchEventManager::RegisterSensor(SCA_ISensor* sensor)
{


	KX_TouchSensor* touchsensor = static_cast<KX_TouchSensor*>(sensor);
	m_sensors.push_back(touchsensor);

	touchsensor->RegisterSumo();//this,m_resphandle);

	//KX_GameObject* gameobj = ((KX_GameObject*)sensor->GetParent());
//	SM_Object* smobj = touchsensor->GetSumoObject();//gameobj->GetSumoObject();
//	if (smobj)
//	{
//		smobj->calcXform();
//		DT_AddObject(m_scenehandle,
//				 smobj->getObjectHandle()); 
//	}
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
			((KX_TouchSensor*)*it)->SynchronizeTransform();
		
		if (DT_Test(m_scenehandle,m_resphandle))
			int i = 0;
		
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
		//std::swap(*i, m_sensors.back());
		//m_sensors.pop_back();
		//SM_Object* smobj = ((KX_TouchSensor*)*i)->GetSumoObject();
		//DT_RemoveObject(m_scenehandle,
		//		 smobj->getObjectHandle()); 
	}
	// remove the sensor forever :)
	SCA_EventManager::RemoveSensor(sensor);
}

#endif
