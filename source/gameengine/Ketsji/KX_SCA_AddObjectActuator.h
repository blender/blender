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

/** \file KX_SCA_AddObjectActuator.h
 *  \ingroup ketsji
 *  \attention Previously existed as: \source\gameengine\GameLogic\SCA_AddObjectActuator.h
 * Please look here for revision history.
 */

#ifndef __KX_SCA_ADDOBJECTACTUATOR_H__
#define __KX_SCA_ADDOBJECTACTUATOR_H__

/* Actuator tree */
#include "SCA_IActuator.h"
#include "SCA_LogicManager.h"

#include "MT_Vector3.h"


class SCA_IScene;

class KX_SCA_AddObjectActuator : public SCA_IActuator
{
	Py_Header

	/// Time field: lifetime of the new object
	int	m_timeProp;

	/// Original object reference (object to replicate)  	
	SCA_IObject*	m_OriginalObject;

	/// Object will be added to the following scene
	SCA_IScene*	m_scene;

	/// Linear velocity upon creation of the object. 
	float  m_linear_velocity[3];
	/// Apply the velocity locally 
	bool m_localLinvFlag;
	
	/// Angular velocity upon creation of the object. 
	float  m_angular_velocity[3];
	/// Apply the velocity locally 
	bool m_localAngvFlag; 
	
	
	
	
	SCA_IObject*	m_lastCreatedObject;
	
public:

	/** 
	 * This class also has the default constructors
	 * available. Use with care!
	 */

	KX_SCA_AddObjectActuator(
		SCA_IObject *gameobj,
		SCA_IObject *original,
		int time,
		SCA_IScene* scene,
		const float *linvel,
		bool linv_local,
		const float *angvel,
		bool angv_local
	);

	~KX_SCA_AddObjectActuator(void);

		CValue* 
	GetReplica(
	);

	virtual void 
	ProcessReplica();

	virtual void Replace_IScene(SCA_IScene *val)
	{
		m_scene= val;
	};

	virtual bool 
	UnlinkObject(SCA_IObject* clientobj);

	virtual void 
	Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map);

	virtual bool 
	Update();

		SCA_IObject*	
	GetLastCreatedObject(
	) const;

	void	InstantAddObject();

#ifdef WITH_PYTHON

	KX_PYMETHOD_DOC_NOARGS(KX_SCA_AddObjectActuator,InstantAddObject);

	static PyObject* pyattr_get_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int pyattr_set_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject* pyattr_get_objectLastCreated(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	
#endif // WITH_PYTHON

}; /* end of class KX_SCA_AddObjectActuator : public KX_EditObjectActuator */

#endif

