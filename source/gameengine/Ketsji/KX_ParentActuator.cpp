/**
 * Set or remove an objects parent
 *
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include "KX_ParentActuator.h"
#include "KX_GameObject.h"
#include "KX_PythonInit.h"

#include "PyObjectPlus.h" 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_ParentActuator::KX_ParentActuator(SCA_IObject *gameobj, 
									 int mode,
									 bool addToCompound,
									 bool ghost,
									 SCA_IObject *ob)
	: SCA_IActuator(gameobj),
	  m_mode(mode),
	  m_addToCompound(addToCompound),
	  m_ghost(ghost),
	  m_ob(ob)
{
	if (m_ob)
		m_ob->RegisterActuator(this);
} 



KX_ParentActuator::~KX_ParentActuator()
{
	if (m_ob)
		m_ob->UnregisterActuator(this);
} 



CValue* KX_ParentActuator::GetReplica()
{
	KX_ParentActuator* replica = new KX_ParentActuator(*this);
	// replication just copy the m_base pointer => common random generator
	replica->ProcessReplica();
	return replica;
}

void KX_ParentActuator::ProcessReplica()
{
	if (m_ob)
		m_ob->RegisterActuator(this);
	SCA_IActuator::ProcessReplica();
}


bool KX_ParentActuator::UnlinkObject(SCA_IObject* clientobj)
{
	if (clientobj == m_ob)
	{
		// this object is being deleted, we cannot continue to track it.
		m_ob = NULL;
		return true;
	}
	return false;
}

void KX_ParentActuator::Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_ob];
	if (h_obj) {
		if (m_ob)
			m_ob->UnregisterActuator(this);
		m_ob = (SCA_IObject*)(*h_obj);
		m_ob->RegisterActuator(this);
	}
}



bool KX_ParentActuator::Update()
{
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events

	KX_GameObject *obj = (KX_GameObject*) GetParent();
	KX_Scene *scene = KX_GetActiveScene();
	switch (m_mode) {
		case KX_PARENT_SET:
			if (m_ob)
				obj->SetParent(scene, (KX_GameObject*)m_ob, m_addToCompound, m_ghost);
			break;
		case KX_PARENT_REMOVE:
			obj->RemoveParent(scene);
			break;
	};
	
	return false;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_ParentActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_ParentActuator",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_ParentActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_ParentActuator::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("object", KX_ParentActuator, pyattr_get_object, pyattr_set_object),
	KX_PYATTRIBUTE_INT_RW("mode", KX_PARENT_NODEF+1, KX_PARENT_MAX-1, true, KX_ParentActuator, m_mode),
	KX_PYATTRIBUTE_BOOL_RW("compound", KX_ParentActuator, m_addToCompound),
	KX_PYATTRIBUTE_BOOL_RW("ghost", KX_ParentActuator, m_ghost),
	{ NULL }	//Sentinel
};

PyObject* KX_ParentActuator::pyattr_get_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_ParentActuator* actuator = static_cast<KX_ParentActuator*>(self);
	if (!actuator->m_ob)	
		Py_RETURN_NONE;
	else
		return actuator->m_ob->GetProxy();
}

int KX_ParentActuator::pyattr_set_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_ParentActuator* actuator = static_cast<KX_ParentActuator*>(self);
	KX_GameObject *gameobj;
		
	if (!ConvertPythonToGameObject(value, &gameobj, true, "actuator.object = value: KX_ParentActuator"))
		return PY_SET_ATTR_FAIL; // ConvertPythonToGameObject sets the error
		
	if (actuator->m_ob != NULL)
		actuator->m_ob->UnregisterActuator(actuator);	

	actuator->m_ob = (SCA_IObject*) gameobj;
		
	if (actuator->m_ob)
		actuator->m_ob->RegisterActuator(actuator);
		
	return PY_SET_ATTR_SUCCESS;
}

/* eof */
