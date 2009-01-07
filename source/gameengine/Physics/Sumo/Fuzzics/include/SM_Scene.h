/**
 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * The physics scene.
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
#ifndef SM_SCENE_H
#define SM_SCENE_H

#ifdef WIN32
#pragma warning (disable : 4786)
#endif

#include <vector>
#include <set>
#include <utility> //needed for pair

#include <SOLID/SOLID.h>

#include "MT_Vector3.h"
#include "MT_Point3.h"

#include "SM_Object.h"

typedef enum
{
	FH_RESPONSE,
	SENSOR_RESPONSE,		/* Touch Sensors */
	CAMERA_RESPONSE,	/* Visibility Culling */
	OBJECT_RESPONSE,	/* Object Dynamic Geometry Response */
	STATIC_RESPONSE,	/* Static Geometry Response */
	
	NUM_RESPONSE
};

class SM_Scene {
public:
    SM_Scene();
	
    ~SM_Scene();
    
	DT_RespTableHandle getRespTableHandle() const {
		return m_respTable;
	}
	
	const MT_Vector3& getForceField() const {
		return m_forceField;
	}

	MT_Vector3& getForceField() {
		return m_forceField;
	}

	void setForceField(const MT_Vector3& forceField) {
		m_forceField = forceField;
	}

	void addTouchCallback(int response_class, DT_ResponseCallback callback, void *user);

    void addSensor(SM_Object& object);
    void add(SM_Object& object);
    void remove(SM_Object& object);

	void notifyCollision(SM_Object *obj1, SM_Object *obj2);

	void setSecondaryRespTable(DT_RespTableHandle secondaryRespTable); 
	DT_RespTableHandle getSecondaryRespTable() { return m_secondaryRespTable; }
	
	void requestCollisionCallback(SM_Object &object);

	void beginFrame();
	void endFrame();

	// Perform an integration step of duration 'timeStep'.
	// 'subSampling' is the maximum duration of a substep, i.e.,
	// The maximum time interval between two collision checks.
	// 'subSampling' can be used to control aliasing effects
	// (fast moving objects traversing through walls and such). 
	bool proceed(MT_Scalar curtime, MT_Scalar ticrate);
	void proceed(MT_Scalar subStep);

	/**
	 * Test whether any objects lie on the line defined by from and
	 * to. The search returns the first such bject starting at from,
	 * or NULL if there was none.
	 * @returns A reference to the object, or NULL if there was none.
	 * @param ignore_client Do not look for collisions with this
	 *        object. This can be useful to avoid self-hits if
	 *        starting from the location of an object.
	 * @param from The start point, in world coordinates, of the search.
	 * @param to The end point, in world coordinates, of the search.
	 * @param result A store to return the point where intersection
	 *        took place (if there was an intersection).
	 * @param normal A store to return the normal of the hit object on
	 *        the location of the intersection, if it took place.
	 */
	SM_Object *rayTest(void *ignore_client,
					   const MT_Point3& from, const MT_Point3& to, 
					   MT_Point3& result, MT_Vector3& normal) const;

private:

	// Clear the user set velocities.
	void clearObjectCombinedVelocities();
	// This is the callback for handling collisions of dynamic objects
	static 
		DT_Bool 
	boing(
		void *client_data,  
		void *object1,
		void *object2,
		const DT_CollData *coll_data
	);
	
	/** internal type */
	typedef std::vector<SM_Object *> T_ObjectList;

	/** Handle to the scene in SOLID */
	DT_SceneHandle      m_scene;
	/** Following response table contains the callbacks for the dynmics */
	DT_RespTableHandle  m_respTable;
	DT_ResponseClass    m_ResponseClass[NUM_RESPONSE];
	/**
	 * Following response table contains callbacks for the client (=
	 * game engine) */
	DT_RespTableHandle  m_secondaryRespTable;  // Handle 
	DT_ResponseClass    m_secondaryResponseClass[NUM_RESPONSE];
	
	/**
	 * Following resposne table contains callbacks for fixing the simulation
	 * ie making sure colliding objects do not intersect.
	 */
	DT_RespTableHandle  m_fixRespTable;
	DT_ResponseClass    m_fixResponseClass[NUM_RESPONSE];

	/** The acceleration from the force field */
	MT_Vector3          m_forceField;

	/**
	 * The list of objects that receive motion updates and do
	 * collision tests. */
	T_ObjectList        m_objectList;
	
	unsigned int        m_frames;
};

#endif

