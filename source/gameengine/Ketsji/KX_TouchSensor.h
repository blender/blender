/**
 * Senses touch and collision events
 *
 * $Id$
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

#ifndef __KX_TOUCHSENSOR
#define __KX_TOUCHSENSOR

#include "SCA_ISensor.h"
#include "ListValue.h"

#include "KX_ClientObjectInfo.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

class KX_TouchSensor : public SCA_ISensor
{
protected:
	Py_Header;

	/**
	 * The sensor should only look for objects with this property.
	 */
	STR_String				m_touchedpropname;	
	bool					m_bFindMaterial;
	class SCA_EventManager*	m_eventmgr;
	
	//class SM_Object*		m_sumoObj;
	//DT_ObjectHandle			m_solidHandle;
	//SM_ClientObjectInfo		m_client_info;
	//DT_RespTableHandle		m_resptable;


	bool					m_bCollision;
	bool					m_bTriggered;
	bool					m_bLastTriggered;
	SCA_IObject*		    m_hitObject;
	class CListValue*		m_colliders;
	
public:
	KX_TouchSensor(class SCA_EventManager* eventmgr,
		class KX_GameObject* gameobj,
		class SM_Object* sumoObj,
		bool fFindMaterial,
		const STR_String& touchedpropname,
		PyTypeObject* T=&Type) ;
	virtual ~KX_TouchSensor();

	virtual CValue* GetReplica() {
		KX_TouchSensor* replica = new KX_TouchSensor(*this);
		replica->m_colliders = new CListValue();
		replica->m_bCollision = false;
		replica->m_bTriggered= false;
		replica->m_hitObject = NULL;
		replica->m_bLastTriggered = false;
		// this will copy properties and so on...
		CValue::AddDataToReplica(replica);
		return replica;
	};
	virtual void SynchronizeTransform();
	virtual bool Evaluate(CValue* event);
	virtual void ReParent(SCA_IObject* parent);
	
/*	static void collisionResponse(void *client_data, 
								  void *object1,
								  void *object2,
								  const DT_CollData *coll_data)	{
		class KX_TouchSensor* sensor = (class KX_TouchSensor*) client_data;
		sensor->HandleCollision(object1,object2,coll_data);
	}
	

	
	void RegisterSumo();

  	virtual void HandleCollision(void* obj1,void* obj2,
						 const DT_CollData * coll_data); 


  //	SM_Object*	GetSumoObject() { return m_sumoObj; };

  */

	virtual bool IsPositiveTrigger() {
		bool result = m_bTriggered;
		if (m_invert) result = !result;
		return result;
	}

	
	void EndFrame();

	// todo: put some info for collision maybe

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */
	
	virtual PyObject*  _getattr(char *attr);

	/* 1. setProperty */
	KX_PYMETHOD_DOC(KX_TouchSensor,SetProperty);
	/* 2. getProperty */
	KX_PYMETHOD_DOC(KX_TouchSensor,GetProperty);
	/* 3. getHitObject */
	KX_PYMETHOD_DOC(KX_TouchSensor,GetHitObject);
	/* 4. getHitObject */
	KX_PYMETHOD_DOC(KX_TouchSensor,GetHitObjectList);
	/* 5. getTouchMaterial */
	KX_PYMETHOD_DOC(KX_TouchSensor,GetTouchMaterial);
	/* 6. setTouchMaterial */
	KX_PYMETHOD_DOC(KX_TouchSensor,SetTouchMaterial);
	
};

#endif //__KX_TOUCHSENSOR

