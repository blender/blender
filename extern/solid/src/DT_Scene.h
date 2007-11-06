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

#ifndef DT_SCENE_H
#define DT_SCENE_H

#include <vector>

#include "SOLID_broad.h"
#include "DT_Encounter.h"

class DT_Object;
class DT_RespTable;

class DT_Scene {
public:
    DT_Scene();
    ~DT_Scene();

    void addObject(DT_Object& object);
    void removeObject(DT_Object& object);

    int  handleCollisions(const DT_RespTable *respTable);

	void *rayCast(const void *ignore_client, 
				  const DT_Vector3 source, const DT_Vector3 target, 
				  DT_Scalar& lambda, DT_Vector3 normal) const;

private:
	typedef std::vector<std::pair<DT_Object *, BP_ProxyHandle> > T_ObjectList;

	BP_SceneHandle      m_broadphase;
	T_ObjectList        m_objectList;
    DT_EncounterTable   m_encounterTable;
};

#endif
