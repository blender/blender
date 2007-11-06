/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#include "DT_Scene.h"
#include "DT_Object.h"
#include "DT_Convex.h"

//#define DEBUG

static void beginOverlap(void *client_data, void *object1, void *object2) 
{
	DT_Encounter e((DT_Object *)object1, (DT_Object *)object2);
	DT_EncounterTable *encounterTable = static_cast<DT_EncounterTable *>(client_data);

#ifdef DEBUG	
	std::cout << "Begin: " << e << std::endl; 
#endif

	encounterTable->insert(e);
}


static void endOverlap(void *client_data, void *object1, void *object2) 
{
	DT_Encounter e((DT_Object *)object1, (DT_Object *)object2);
	DT_EncounterTable *encounterTable = static_cast<DT_EncounterTable *>(client_data);

#ifdef DEBUG
	std::cout << "End:   " << e << std::endl; 
#endif
	
	assert(encounterTable->find(e) != encounterTable->end()); 
	encounterTable->erase(e);
}

struct DT_RayCastData {
	DT_RayCastData(const void *ignore) 
	  : m_ignore(ignore) 
	{}

	const void  *m_ignore;
	MT_Vector3  m_normal;
};

static bool objectRayCast(void *client_data, 
						  void *object,  
						  const DT_Vector3 source,
						  const DT_Vector3 target,
						  DT_Scalar *lambda) 
{
	DT_RayCastData *data = static_cast<DT_RayCastData *>(client_data); 
	if (((DT_Object *)object)->getClientObject() != data->m_ignore)
	{
		MT_Scalar param = MT_Scalar(*lambda);
		
		if (((DT_Object *)object)->ray_cast(MT_Point3(source), MT_Point3(target),
											param, data->m_normal))
		{
			*lambda = param;
			return true;
		}
	}
	return false;
}

DT_Scene::DT_Scene() 
  : m_broadphase(BP_CreateScene(&m_encounterTable, &beginOverlap, &endOverlap))
{}

DT_Scene::~DT_Scene()
{
	BP_DestroyScene(m_broadphase);
}

void DT_Scene::addObject(DT_Object &object)
{
	const MT_BBox& bbox = object.getBBox();
	DT_Vector3 min, max;
	bbox.getMin().getValue(min);
	bbox.getMax().getValue(max);
    BP_ProxyHandle proxy = BP_CreateProxy(m_broadphase, &object, min, max);
	
#ifdef DEBUG
	DT_EncounterTable::iterator it;	
	std::cout << "Add " << &object << ':';
	for (it = m_encounterTable.begin(); it != m_encounterTable.end(); ++it) {
		std::cout << ' ' << (*it);
	}
	std::cout << std::endl;
#endif
	object.addProxy(proxy);
    m_objectList.push_back(std::make_pair(&object, proxy));
}



void DT_Scene::removeObject(DT_Object& object)
{
    T_ObjectList::iterator it = m_objectList.begin();

    while (it != m_objectList.end() && &object != (*it).first)
	{
        ++it;
    }

    if (it != m_objectList.end())
	{
		object.removeProxy((*it).second);
        BP_DestroyProxy(m_broadphase, (*it).second);
		m_objectList.erase(it);

#ifdef DEBUG
		std::cout << "Remove " << &object << ':';
		DT_EncounterTable::iterator it;	
		for (it = m_encounterTable.begin(); it != m_encounterTable.end(); ++it)
		{
			std::cout << ' ' << (*it);
			assert((*it).first() != &object &&
				   (*it).second() != &object);
		}
		std::cout << std::endl;
#endif
    }
}



int DT_Scene::handleCollisions(const DT_RespTable *respTable)
{
    int count = 0;

    assert(respTable);

	DT_EncounterTable::iterator it;	
	for (it = m_encounterTable.begin(); it != m_encounterTable.end(); ++it)
	{
		if ((*it).exactTest(respTable, count))
		{
			break;
        }
	
    }
    return count;
}

void *DT_Scene::rayCast(const void *ignore_client,
						const DT_Vector3 source, const DT_Vector3 target, 
						DT_Scalar& lambda, DT_Vector3 normal) const 
{
	DT_RayCastData data(ignore_client);
	DT_Object *object = (DT_Object *)BP_RayCast(m_broadphase, 
												&objectRayCast, 
												&data, 
												source, target,
												&lambda);
	if (object)
	{
		data.m_normal.getValue(normal);
		return object->getClientObject();
	}
	
	return 0;
}
