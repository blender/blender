/**
 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * The physics scene.
 */

#ifndef SM_SCENE_H
#define SM_SCENE_H

#pragma warning (disable : 4786)

#include <vector>
#include <set>
#include <utility> //needed for pair

#include "solid.h"

#include "MT_Vector3.h"
#include "MT_Point3.h"

class SM_Object;

class SM_Scene {
public:
    SM_Scene() : 
		m_scene(DT_CreateScene()),
		m_respTable(DT_CreateRespTable()),
		m_secondaryRespTable(0),
		m_forceField(0.0, 0.0, 0.0)
		{}
	
    ~SM_Scene() { 
		DT_DeleteRespTable(m_respTable);
		DT_DeleteScene(m_scene);
	}

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

    void add(SM_Object& object);
    void remove(SM_Object& object);

	void addPair(SM_Object *obj1, SM_Object *obj2) {
		m_pairList.insert(std::make_pair(obj1, obj2));
	}

	void clearPairs() {
		m_pairList.clear();
	}

	void setSecondaryRespTable(DT_RespTableHandle secondaryRespTable) {
		m_secondaryRespTable = secondaryRespTable;
	}


	// Perform an integration step of duration 'timeStep'.
	// 'subSampling' is the maximum duration of a substep, i.e.,
	// The maximum time interval between two collision checks.
	// 'subSampling' can be used to control aliasing effects
	// (fast moving objects traversing through walls and such). 
	void proceed(MT_Scalar timeStep, MT_Scalar subSampling);

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

	/** internal type */
	typedef std::vector<SM_Object *> T_ObjectList;
	/** internal type */
	typedef std::set<std::pair<SM_Object *, SM_Object *> > T_PairList;

	/** Handle to the scene in SOLID */
	DT_SceneHandle      m_scene;
	/** Following response table contains the callbacks for the dynmics */
	DT_RespTableHandle  m_respTable;
	/**
	 * Following response table contains callbacks for the client (=
	 * game engine) */
	DT_RespTableHandle  m_secondaryRespTable;  // Handle 

	/** The acceleration from the force field */
	MT_Vector3          m_forceField;

	/**
	 * The list of objects that receive motion updates and do
	 * collision tests. */
	T_ObjectList        m_objectList;

	/**
	 * A list with pairs of objects that collided the previous
	 * timestep. The list is built during the proceed(). During that
	 * time, it is not valid. */
	T_PairList          m_pairList;
};

#endif
