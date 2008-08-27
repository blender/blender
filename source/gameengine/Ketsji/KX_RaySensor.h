/**
 * Cast a ray and feel for objects
 *
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
 */

#ifndef __KX_RAYSENSOR_H
#define __KX_RAYSENSOR_H

#include "SCA_ISensor.h"
#include "MT_Point3.h"

struct KX_ClientObjectInfo;
class KX_RayCast;

class KX_RaySensor : public SCA_ISensor
{
	Py_Header;
	STR_String		m_propertyname;
	bool			m_bFindMaterial;
	double			m_distance;
	class KX_Scene* m_scene;
	bool			m_bTriggered;
	int				m_axis;
	bool			m_rayHit;
	MT_Point3		m_hitPosition;
	SCA_IObject*	m_hitObject;
	MT_Vector3		m_hitNormal;
	MT_Vector3		m_rayDirection;

public:
	KX_RaySensor(class SCA_EventManager* eventmgr,
					SCA_IObject* gameobj,
					const STR_String& propname,
					bool fFindMaterial,
					double distance,
					int axis,
					class KX_Scene* ketsjiScene,
					PyTypeObject* T = &Type);
	virtual ~KX_RaySensor();
	virtual CValue* GetReplica();

	virtual bool Evaluate(CValue* event);
	virtual bool IsPositiveTrigger();
	virtual void Init();

	bool RayHit(KX_ClientObjectInfo* client, KX_RayCast* result, void * const data);
	bool NeedRayCast(KX_ClientObjectInfo* client);
	
	KX_PYMETHOD_DOC(KX_RaySensor,GetHitObject);
	KX_PYMETHOD_DOC(KX_RaySensor,GetHitPosition);
	KX_PYMETHOD_DOC(KX_RaySensor,GetHitNormal);
	KX_PYMETHOD_DOC(KX_RaySensor,GetRayDirection);

	virtual PyObject* _getattr(const STR_String& attr);
	
};

#endif //__KX_RAYSENSOR_H

