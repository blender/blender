/**
 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * The physics scene.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#pragma warning(disable : 4786)  // shut off 255 char limit debug template warning
#endif

#include "SM_Scene.h"
#include "SM_Object.h"
#include "SM_FhObject.h"

#include "SM_Debug.h"

#include <algorithm>

SM_Scene::SM_Scene() : 
	m_scene(DT_CreateScene()),
	m_respTable(DT_CreateRespTable()),
	m_secondaryRespTable(DT_CreateRespTable()),
	m_fixRespTable(DT_CreateRespTable()),
	m_forceField(0.0, 0.0, 0.0)
{
	for (int i = 0 ; i < NUM_RESPONSE; i++)
	{
		m_ResponseClass[i] = DT_GenResponseClass(m_respTable);
		m_secondaryResponseClass[i] = DT_GenResponseClass(m_secondaryRespTable);
		m_fixResponseClass[i] = DT_GenResponseClass(m_fixRespTable);
	}
	
	/* Sensor */
	DT_AddPairResponse(m_respTable, m_ResponseClass[SENSOR_RESPONSE], m_ResponseClass[SENSOR_RESPONSE], 0, DT_NO_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[SENSOR_RESPONSE], m_ResponseClass[STATIC_RESPONSE], SM_Scene::boing, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[SENSOR_RESPONSE], m_ResponseClass[OBJECT_RESPONSE], SM_Scene::boing, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[SENSOR_RESPONSE], m_ResponseClass[FH_RESPONSE], 0, DT_NO_RESPONSE, this);
	
	/* Static */
	DT_AddPairResponse(m_respTable, m_ResponseClass[STATIC_RESPONSE], m_ResponseClass[SENSOR_RESPONSE], SM_Scene::boing, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[STATIC_RESPONSE], m_ResponseClass[STATIC_RESPONSE], 0, DT_NO_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[STATIC_RESPONSE], m_ResponseClass[OBJECT_RESPONSE], SM_Object::boing, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[STATIC_RESPONSE], m_ResponseClass[FH_RESPONSE], SM_FhObject::ray_hit, DT_SIMPLE_RESPONSE, this);
	
	/* Object */
	DT_AddPairResponse(m_respTable, m_ResponseClass[OBJECT_RESPONSE], m_ResponseClass[SENSOR_RESPONSE], SM_Scene::boing, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[OBJECT_RESPONSE], m_ResponseClass[STATIC_RESPONSE], SM_Object::boing, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[OBJECT_RESPONSE], m_ResponseClass[OBJECT_RESPONSE], SM_Object::boing, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[OBJECT_RESPONSE], m_ResponseClass[FH_RESPONSE], SM_FhObject::ray_hit, DT_SIMPLE_RESPONSE, this);
	
	/* Fh Object */
	DT_AddPairResponse(m_respTable, m_ResponseClass[FH_RESPONSE], m_ResponseClass[SENSOR_RESPONSE], 0, DT_NO_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[FH_RESPONSE], m_ResponseClass[STATIC_RESPONSE], SM_FhObject::ray_hit, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[FH_RESPONSE], m_ResponseClass[OBJECT_RESPONSE], SM_FhObject::ray_hit, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_respTable, m_ResponseClass[FH_RESPONSE], m_ResponseClass[FH_RESPONSE], 0, DT_NO_RESPONSE, this);
	
	/* Object (Fix Pass) */
	DT_AddPairResponse(m_fixRespTable, m_ResponseClass[OBJECT_RESPONSE], m_ResponseClass[SENSOR_RESPONSE], 0, DT_NO_RESPONSE, this);
	DT_AddPairResponse(m_fixRespTable, m_ResponseClass[OBJECT_RESPONSE], m_ResponseClass[STATIC_RESPONSE], SM_Object::fix, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_fixRespTable, m_ResponseClass[OBJECT_RESPONSE], m_ResponseClass[OBJECT_RESPONSE], SM_Object::fix, DT_SIMPLE_RESPONSE, this);
	DT_AddPairResponse(m_fixRespTable, m_ResponseClass[OBJECT_RESPONSE], m_ResponseClass[FH_RESPONSE], 0, DT_NO_RESPONSE, this);
}

void SM_Scene::addTouchCallback(int response_class, DT_ResponseCallback callback, void *user)
{
	DT_AddClassResponse(m_secondaryRespTable, m_secondaryResponseClass[response_class], callback, DT_SIMPLE_RESPONSE, user);
}

void SM_Scene::addSensor(SM_Object& object) 
{
	object.calcXform();
	m_objectList.push_back(&object);
	DT_AddObject(m_scene, object.getObjectHandle());
	DT_SetResponseClass(m_respTable, object.getObjectHandle(), m_ResponseClass[SENSOR_RESPONSE]);
	DT_SetResponseClass(m_secondaryRespTable, object.getObjectHandle(), m_secondaryResponseClass[SENSOR_RESPONSE]);
	DT_SetResponseClass(m_fixRespTable, object.getObjectHandle(), m_fixResponseClass[SENSOR_RESPONSE]);
}

void SM_Scene::add(SM_Object& object) {
	object.calcXform();
	m_objectList.push_back(&object);
	DT_AddObject(m_scene, object.getObjectHandle());
	if (object.isDynamic()) {
		DT_SetResponseClass(m_respTable, object.getObjectHandle(), m_ResponseClass[OBJECT_RESPONSE]);
		DT_SetResponseClass(m_secondaryRespTable, object.getObjectHandle(), m_secondaryResponseClass[OBJECT_RESPONSE]);
		DT_SetResponseClass(m_fixRespTable, object.getObjectHandle(), m_fixResponseClass[OBJECT_RESPONSE]);
	} else {
		DT_SetResponseClass(m_respTable, object.getObjectHandle(), m_ResponseClass[STATIC_RESPONSE]);
		DT_SetResponseClass(m_secondaryRespTable, object.getObjectHandle(), m_secondaryResponseClass[STATIC_RESPONSE]);
		DT_SetResponseClass(m_fixRespTable, object.getObjectHandle(), m_fixResponseClass[STATIC_RESPONSE]);
	}	

	SM_FhObject *fh_object = object.getFhObject();

	if (fh_object) {
		DT_AddObject(m_scene, fh_object->getObjectHandle());
		DT_SetResponseClass(m_respTable, fh_object->getObjectHandle(), m_ResponseClass[FH_RESPONSE]);
		DT_SetResponseClass(m_secondaryRespTable, fh_object->getObjectHandle(), m_secondaryResponseClass[FH_RESPONSE]);
		DT_SetResponseClass(m_fixRespTable, fh_object->getObjectHandle(), m_fixResponseClass[FH_RESPONSE]);
	}
}	

void SM_Scene::requestCollisionCallback(SM_Object &object)
{
	DT_SetResponseClass(m_respTable, object.getObjectHandle(), m_ResponseClass[OBJECT_RESPONSE]);
	DT_SetResponseClass(m_secondaryRespTable, object.getObjectHandle(), m_secondaryResponseClass[OBJECT_RESPONSE]);
//	DT_SetResponseClass(m_fixRespTable, object.getObjectHandle(), m_fixResponseClass[OBJECT_RESPONSE]);
}

void SM_Scene::remove(SM_Object& object) { 	
	T_ObjectList::iterator i =
		std::find(m_objectList.begin(), m_objectList.end(), &object);
	if (!(i == m_objectList.end()))
	{
		std::swap(*i, m_objectList.back());
		m_objectList.pop_back();
		DT_RemoveObject(m_scene, object.getObjectHandle());

		SM_FhObject *fh_object = object.getFhObject();
		
		if (fh_object) {
			DT_RemoveObject(m_scene, fh_object->getObjectHandle());
		}
	} 
	else {
		// tried to remove an object that is not in the scene
		//assert(false);
	}
}	

void SM_Scene::proceed(MT_Scalar timeStep, MT_Scalar subSampling) {
	// Don't waste time...but it's OK to spill a little.
	//if (timeStep < 0.001)
	//	return;

	// Divide the timeStep into a number of subsamples of size roughly 
	// equal to subSampling (might be a little smaller).
	int num_samples = (int)ceil(timeStep / subSampling);

	MT_Scalar subStep = timeStep / num_samples;
	T_ObjectList::iterator i;

	// Apply a forcefield (such as gravity)
	for (i = m_objectList.begin(); i != m_objectList.end(); ++i) {
		(*i)->applyForceField(m_forceField);
		//(*i)->integrateForces(subStep);
		//(*i)->integrateMomentum(subStep);
	}
	
	// Do the integration steps per object.
	int step;
	for (step = 0; step != num_samples; ++step) {


		for (i = m_objectList.begin(); i != m_objectList.end(); ++i) {
			(*i)->integrateForces(subStep);
			//(*i)->backup();
		// And second we update the object positions by performing
		// an integration step for each object
			(*i)->integrateMomentum(subStep);
		}
		// I changed the order of the next 2 statements.
		// Originally objects were first integrated with a call
		// to proceed(). However if external objects were 
		// directly manipulating the velocities etc of physics 
		// objects then the physics environment would not be able 
		// to react before object positions were updated. --- Laurence.

		// So now first we let the physics scene respond to 
		// new forces, velocities set externally. 
		// The collsion and friction impulses are computed here. 
		DT_Test(m_scene, m_respTable);
	}

	// clear the user set velocities.
#if 0
	clearObjectCombinedVelocities();
#endif
/*	if (DT_Test(m_scene, m_fixRespTable))
		for (i = m_objectList.begin(); i != m_objectList.end(); ++i)
			(*i)->relax();  */
	DT_Test(m_scene, m_fixRespTable);
	
	// Finish this timestep by saving al state information for the next
	// timestep and clearing the accumulated forces. 

	for (i = m_objectList.begin(); i != m_objectList.end(); ++i) {
		(*i)->relax();
		(*i)->proceedKinematic(timeStep);
		(*i)->saveReactionForce(timeStep);
		(*i)->clearForce();
	}

	// For each pair of object that collided, call the corresponding callback.
	// Additional collisions of a pair within the same time step are ignored.

	if (m_secondaryRespTable) {
		T_PairList::iterator p;
		for (p = m_pairList.begin(); p != m_pairList.end(); ++p) {
			DT_CallResponse(m_secondaryRespTable, 
							(*p).first->getObjectHandle(), 
							(*p).second->getObjectHandle(), 
							0);
		}
	}
	
	clearPairs();
}

SM_Object *SM_Scene::rayTest(void *ignore_client, 
							 const MT_Point3& from, const MT_Point3& to, 
							 MT_Point3& result, MT_Vector3& normal) const {
#ifdef SM_DEBUG_RAYCAST
	std::cout << "ray: { " << from << " } - { " << to << " }" << std::endl; 
#endif
 
	DT_Vector3 n, dfrom, dto;
	DT_Scalar param;
	from.getValue(dfrom);
	to.getValue(dto);
	SM_Object *hit_object = (SM_Object *) 
		DT_RayCast(m_scene, ignore_client, dfrom, dto, 1., &param, n);

	if (hit_object) {
		//result = hit_object->getWorldCoord(from + (to - from)*param);
		result = from + (to - from) * param;
		normal.setValue(n);
#ifdef SM_DEBUG_RAYCAST
		std::cout << "ray: { " << from << " } -> { " << to << " }: { " << result 
		  << " } (" << param << "), normal = { " << normal << " }" << std::endl;
#endif
	}

	return hit_object;
}

void SM_Scene::clearObjectCombinedVelocities() {

	T_ObjectList::iterator i;

	for (i = m_objectList.begin(); i != m_objectList.end(); ++i) {

		(*i)->clearCombinedVelocities();

	}

}	


void SM_Scene::setSecondaryRespTable(DT_RespTableHandle secondaryRespTable) {
	m_secondaryRespTable = secondaryRespTable;
}


DT_Bool SM_Scene::boing(
	void *client_data,  
	void *object1,
	void *object2,
	const DT_CollData *coll_data
){
	SM_Scene  *scene = (SM_Scene *)client_data; 
	SM_Object *obj1  = (SM_Object *)object1;  
	SM_Object *obj2  = (SM_Object *)object2;  
	
	scene->addPair(obj1, obj2); // Record this collision for client callbacks

#ifdef SM_DEBUG_BOING	
	printf("SM_Scene::boing\n");
#endif
	
	return DT_CONTINUE;
}

SM_Scene::~SM_Scene()
{ 
//	if (m_objectList.begin() != m_objectList.end()) 
//		std::cout << "SM_Scene::~SM_Scene: There are still objects in the Sumo scene!" << std::endl;
	for (T_ObjectList::iterator it = m_objectList.begin() ; it != m_objectList.end() ; it++)
		delete *it;
	
	DT_DestroyRespTable(m_respTable);
	DT_DestroyRespTable(m_secondaryRespTable);
	DT_DestroyRespTable(m_fixRespTable);
	DT_DestroyScene(m_scene);
}
