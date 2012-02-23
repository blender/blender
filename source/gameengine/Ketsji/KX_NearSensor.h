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
 */

/** \file KX_NearSensor.h
 *  \ingroup ketsji
 *  \brief Sense if other objects are near
 */

#ifndef __KX_NEARSENSOR_H__
#define __KX_NEARSENSOR_H__

#include "KX_TouchSensor.h"
#include "KX_ClientObjectInfo.h"

class KX_Scene;
struct PHY_CollData;

class KX_NearSensor : public KX_TouchSensor
{
	Py_Header
protected:
	float	m_Margin;
	float  m_ResetMargin;

	KX_ClientObjectInfo*	m_client_info;
public:
	KX_NearSensor(class SCA_EventManager* eventmgr,
	              class KX_GameObject* gameobj,
	              float margin,
	              float resetmargin,
	              bool bFindMaterial,
	              const STR_String& touchedpropname,
	              PHY_IPhysicsController*	ctrl);
/*
public:
	KX_NearSensor(class SCA_EventManager* eventmgr,
			class KX_GameObject* gameobj,
			double margin,
			double resetmargin,
			bool bFindMaterial,
			const STR_String& touchedpropname,
			class KX_Scene* scene);
*/
	virtual ~KX_NearSensor(); 
	virtual void SynchronizeTransform();
	virtual CValue* GetReplica();
	virtual void ProcessReplica();
	virtual void SetPhysCtrlRadius();
	virtual bool Evaluate();

	virtual void ReParent(SCA_IObject* parent);
	virtual bool	NewHandleCollision(void* obj1,void* obj2,
						 const PHY_CollData * coll_data); 
	virtual bool	BroadPhaseFilterCollision(void*obj1,void*obj2);
	virtual bool	BroadPhaseSensorFilterCollision(void*obj1,void*obj2) { return false; };
	virtual sensortype GetSensorType() { return ST_NEAR; }

#ifdef WITH_PYTHON

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	//No methods

	//This method is used to make sure the distance does not exceed the reset distance
	static int CheckResetDistance(void *self, const PyAttributeDef*)
	{
		KX_NearSensor* sensor = reinterpret_cast<KX_NearSensor*>(self);

		if (sensor->m_Margin > sensor->m_ResetMargin)
			sensor->m_ResetMargin = sensor->m_Margin;

		sensor->SetPhysCtrlRadius();
			
		return 0;
	}

#endif // WITH_PYTHON

};

#endif //__KX_NEARSENSOR_H__

