/**
 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * The physics scene.
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

#include <algorithm>

void SM_Scene::add(SM_Object& object) {
	object.calcXform();
	m_objectList.push_back(&object);
	DT_AddObject(m_scene, object.getObjectHandle());
	if (object.isDynamic()) {
		DT_SetObjectResponse(m_respTable, object.getObjectHandle(),
							 SM_Object::boing, DT_SIMPLE_RESPONSE, this);
	}

	if (object.getDynamicParent()) {
		DT_SetPairResponse(m_respTable, object.getObjectHandle(),
						   object.getDynamicParent()->getObjectHandle(),
						   0, DT_NO_RESPONSE, 0);
	}
	SM_FhObject *fh_object = object.getFhObject();

	if (fh_object) {
		DT_AddObject(m_scene, fh_object->getObjectHandle());
		DT_SetObjectResponse(m_respTable, fh_object->getObjectHandle(),
							 SM_FhObject::ray_hit, DT_SIMPLE_RESPONSE, this);
	}
}	

void SM_Scene::remove(SM_Object& object) { 	
	T_ObjectList::iterator i =
		std::find(m_objectList.begin(), m_objectList.end(), &object);
	if (!(i == m_objectList.end()))
	{
		std::swap(*i, m_objectList.back());
		m_objectList.pop_back();
		DT_RemoveObject(m_scene, object.getObjectHandle());
		if (object.isDynamic()) {
			DT_ClearObjectResponse(m_respTable, object.getObjectHandle());
		}
		
		if (object.getDynamicParent()) {
			DT_ClearPairResponse(m_respTable, object.getObjectHandle(),
								 object.getDynamicParent()->getObjectHandle());
		}

		SM_FhObject *fh_object = object.getFhObject();
		
		if (fh_object) {
			DT_RemoveObject(m_scene, fh_object->getObjectHandle());
			DT_ClearObjectResponse(m_respTable,
								   fh_object->getObjectHandle());
		}
	} 
	else {
		// tried to remove an object that is not in the scene
		//assert(false);
	}
}	

void SM_Scene::proceed(MT_Scalar timeStep, MT_Scalar subSampling) {
	// Don't waste time...but it's OK to spill a little.
	if (timeStep < 0.001)
		return;

	// Divide the timeStep into a number of subsamples of size roughly 
	// equal to subSampling (might be a little smaller).
	int num_samples = (int)ceil(timeStep / subSampling);


	MT_Scalar subStep = timeStep / num_samples;
	T_ObjectList::iterator i;

	// Apply a forcefield (such as gravity)
	for (i = m_objectList.begin(); i != m_objectList.end(); ++i) {
		(*i)->applyForceField(m_forceField);
	}
	
	// Do the integration steps per object.
	int step;
	for (step = 0; step != num_samples; ++step) {

		for (i = m_objectList.begin(); i != m_objectList.end(); ++i) {
			(*i)->integrateForces(subStep);
		}

		// And second we update the object positions by performing
		// an integration step for each object
		for (i = m_objectList.begin(); i != m_objectList.end(); ++i) {
			(*i)->integrateMomentum(subStep);
		}
#if 0
		// I changed the order of the next 2 statements.
		// Originally objects were first integrated with a call
		// to proceed(). However if external objects were 
		// directly manipulating the velocities etc of physics 
		// objects then the physics environment would not be able 
		// to react before object positions were updated. --- Laurence.

		// So now first we let the physics scene respond to 
		// new forces, velocities set externally. 
#endif
		// The collsion and friction impulses are computed here. 
		DT_Test(m_scene, m_respTable);

	}


	// clear the user set velocities.
#if 0
	clearObjectCombinedVelocities();
#endif
	// Finish this timestep by saving al state information for the next
	// timestep and clearing the accumulated forces. 

	for (i = m_objectList.begin(); i != m_objectList.end(); ++i) {
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
	MT_Point3 local; 
 
	SM_Object *hit_object = (SM_Object *) 
		DT_RayTest(m_scene, ignore_client, from.getValue(), to.getValue(),
				   local.getValue(), normal.getValue());

	if (hit_object) {
		result = hit_object->getWorldCoord(local);
	}

	return hit_object;
}

void SM_Scene::clearObjectCombinedVelocities() {

	T_ObjectList::iterator i;

	for (i = m_objectList.begin(); i != m_objectList.end(); ++i) {

		(*i)->clearCombinedVelocities();

	}

}	





