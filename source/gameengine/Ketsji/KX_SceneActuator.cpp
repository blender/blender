/**
* Set scene/camera stuff
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

#include "SCA_IActuator.h"
#include "KX_SceneActuator.h"
#include <iostream>
#include "KX_Scene.h"
#include "KX_Camera.h"
#include "KX_KetsjiEngine.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_SceneActuator::KX_SceneActuator(SCA_IObject *gameobj, 
								   int mode,
								   KX_Scene *scene,
								   KX_KetsjiEngine* ketsjiEngine,
								   const STR_String& nextSceneName,
								   KX_Camera* camera,
								   PyTypeObject* T)
								   : SCA_IActuator(gameobj, T)
{
	m_mode = mode;
	m_scene  = scene;
	m_KetsjiEngine=ketsjiEngine;
	m_camera = camera;
	m_nextSceneName = nextSceneName;
	if (m_camera)
		m_camera->RegisterActuator(this);
} /* End of constructor */



KX_SceneActuator::~KX_SceneActuator()
{ 
	if (m_camera)
		m_camera->UnregisterActuator(this);
} /* end of destructor */



CValue* KX_SceneActuator::GetReplica()
{
	KX_SceneActuator* replica = new KX_SceneActuator(*this);
	replica->ProcessReplica();
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	
	return replica;
}

void KX_SceneActuator::ProcessReplica()
{
	if (m_camera)
		m_camera->RegisterActuator(this);
	SCA_IActuator::ProcessReplica();
}

bool KX_SceneActuator::UnlinkObject(SCA_IObject* clientobj)
{
	if (clientobj == (SCA_IObject*)m_camera)
	{
		// this object is being deleted, we cannot continue to track it.
		m_camera = NULL;
		return true;
	}
	return false;
}

void KX_SceneActuator::Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_camera];
	if (h_obj) {
		if (m_camera)
			m_camera->UnregisterActuator(this);
		m_camera = (KX_Camera*)(*h_obj);
		m_camera->RegisterActuator(this);
	}
}


bool KX_SceneActuator::Update()
{
	// bool result = false;	/*unused*/
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events

	switch (m_mode)
	{
	case KX_SCENE_RESTART:
		{
			m_KetsjiEngine->ReplaceScene(m_scene->GetName(),m_scene->GetName());
			break;
		}
	case KX_SCENE_SET_CAMERA:
		if (m_camera)
		{
			m_scene->SetActiveCamera(m_camera);
		}
		else
		{
			// if no camera is set and the parent object is a camera, use it as the camera
			SCA_IObject* parent = GetParent();
			if (parent->isA(&KX_Camera::Type))
			{
				m_scene->SetActiveCamera((KX_Camera*)parent);
			}
		}
		break;
	default:
		break;
	}
	
	if (!m_nextSceneName.Length())
		return false;
	
	switch (m_mode)
	{
	case KX_SCENE_SET_SCENE:
		{
			m_KetsjiEngine->ReplaceScene(m_scene->GetName(),m_nextSceneName);
			break;
		}
	case KX_SCENE_ADD_FRONT_SCENE:
		{
			bool overlay=true;
			m_KetsjiEngine->ConvertAndAddScene(m_nextSceneName,overlay);
			break;
		}
	case KX_SCENE_ADD_BACK_SCENE:
		{
			bool overlay=false;
			m_KetsjiEngine->ConvertAndAddScene(m_nextSceneName,overlay);
			break;
		}
	case KX_SCENE_REMOVE_SCENE:
		{
			m_KetsjiEngine->RemoveScene(m_nextSceneName);
			break;
		}
	case KX_SCENE_SUSPEND:
		{
			m_KetsjiEngine->SuspendScene(m_nextSceneName);
			break;
		}
	case KX_SCENE_RESUME:
		{
			m_KetsjiEngine->ResumeScene(m_nextSceneName);
			break;
		}
	default:
		; /* do nothing? this is an internal error !!! */
	}
	
	return false;
}



/*  returns a camera if the name is valid */
KX_Camera* KX_SceneActuator::FindCamera(char *camName)
{
	KX_SceneList* sl = m_KetsjiEngine->CurrentScenes();
	STR_String name = STR_String(camName);
	KX_SceneList::iterator it = sl->begin();
	KX_Camera* cam = NULL;

	while ((it != sl->end()) && (!cam))
	{
		cam = (*it)->FindCamera(name);
		it++;
	}

	return cam;
}



KX_Scene* KX_SceneActuator::FindScene(char * sceneName)
{
	return m_KetsjiEngine->FindScene(sceneName);
}




/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_SceneActuator::Type = {
	PyObject_HEAD_INIT(NULL)
		0,
		"KX_SceneActuator",
		sizeof(PyObjectPlus_Proxy),
		0,
		py_base_dealloc,
		0,
		0,
		0,
		0,
		py_base_repr,
		0,0,0,0,0,0,
		py_base_getattro,
		py_base_setattro,
		0,0,0,0,0,0,0,0,0,
		Methods
};



PyParentObject KX_SceneActuator::Parents[] =
{
	&KX_SceneActuator::Type,
		&SCA_IActuator::Type,
		&SCA_ILogicBrick::Type,
		&CValue::Type,
		NULL
};



PyMethodDef KX_SceneActuator::Methods[] =
{
	//Deprecated functions ------>
	{"setUseRestart", (PyCFunction) KX_SceneActuator::sPySetUseRestart, METH_VARARGS, (PY_METHODCHAR)SetUseRestart_doc},
	{"setScene",      (PyCFunction) KX_SceneActuator::sPySetScene, METH_VARARGS, (PY_METHODCHAR)SetScene_doc},
	{"setCamera",     (PyCFunction) KX_SceneActuator::sPySetCamera, METH_VARARGS, (PY_METHODCHAR)SetCamera_doc},
	{"getUseRestart", (PyCFunction) KX_SceneActuator::sPyGetUseRestart, METH_VARARGS, (PY_METHODCHAR)GetUseRestart_doc},
	{"getScene",      (PyCFunction) KX_SceneActuator::sPyGetScene, METH_VARARGS, (PY_METHODCHAR)GetScene_doc},
	{"getCamera",     (PyCFunction) KX_SceneActuator::sPyGetCamera, METH_VARARGS, (PY_METHODCHAR)GetCamera_doc},
	//<----- Deprecated
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_SceneActuator::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RW("scene",0,32,true,KX_SceneActuator,m_nextSceneName),
	KX_PYATTRIBUTE_RW_FUNCTION("camera",KX_SceneActuator,pyattr_get_camera,pyattr_set_camera),
	{ NULL }	//Sentinel
};

PyObject* KX_SceneActuator::py_getattro(PyObject *attr)
{
	py_getattro_up(SCA_IActuator);
}

int KX_SceneActuator::py_setattro(PyObject *attr, PyObject *value)
{
	py_setattro_up(SCA_IActuator);
}

PyObject* KX_SceneActuator::pyattr_get_camera(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SceneActuator* actuator = static_cast<KX_SceneActuator*>(self);
	if (!actuator->m_camera)
		Py_RETURN_NONE;
	
	return actuator->m_camera->GetProxy();
}

int KX_SceneActuator::pyattr_set_camera(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SceneActuator* actuator = static_cast<KX_SceneActuator*>(self);
	KX_Camera *camOb;
	
	if(value==Py_None)
	{
		if (actuator->m_camera)
			actuator->m_camera->UnregisterActuator(actuator);
		
		actuator->m_camera= NULL;
		return 0;
	}
	
	if (PyObject_TypeCheck(value, &KX_Camera::Type)) 
	{
		KX_Camera *camOb= static_cast<KX_Camera*>BGE_PROXY_REF(value);
		
		if(camOb==NULL)
		{
			PyErr_SetString(PyExc_RuntimeError, BGE_PROXY_ERROR_MSG);
			return 1;
		}
		
		if (actuator->m_camera)
			actuator->m_camera->UnregisterActuator(actuator);
		
		actuator->m_camera = camOb;
		actuator->m_camera->RegisterActuator(actuator);
		return 0;
	}

	if (PyString_Check(value))
	{
		char *camName = PyString_AsString(value);

		camOb = actuator->FindCamera(camName);
		if (camOb) 
		{
			if (actuator->m_camera)
				actuator->m_camera->UnregisterActuator(actuator);
			actuator->m_camera = camOb;
			actuator->m_camera->RegisterActuator(actuator);
			return 0;
		}
		PyErr_SetString(PyExc_TypeError, "not a valid camera name");
		return 1;
	}
	PyErr_SetString(PyExc_TypeError, "expected a string or a camera object reference");
	return 1;
}


/* 2. setUseRestart--------------------------------------------------------- */
const char KX_SceneActuator::SetUseRestart_doc[] = 
"setUseRestart(flag)\n"
"\t- flag: 0 or 1.\n"
"\tSet flag to 1 to restart the scene.\n" ;
PyObject* KX_SceneActuator::PySetUseRestart(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	ShowDeprecationWarning("setUseRestart()", "(no replacement)");
	int boolArg;
	
	if (!PyArg_ParseTuple(args, "i:setUseRestart", &boolArg))
	{
		return NULL;
	}
	
	m_restart = boolArg != 0;
	
	Py_RETURN_NONE;
}



/* 3. getUseRestart:                                                         */
const char KX_SceneActuator::GetUseRestart_doc[] = 
"getUseRestart()\n"
"\tReturn whether the scene will be restarted.\n" ;
PyObject* KX_SceneActuator::PyGetUseRestart(PyObject* self, 
											PyObject* args, 
											PyObject* kwds)
{
	ShowDeprecationWarning("getUseRestart()", "(no replacement)");
	return PyInt_FromLong(!(m_restart == 0));
}



/* 4. set scene------------------------------------------------------------- */
const char KX_SceneActuator::SetScene_doc[] = 
"setScene(scene)\n"
"\t- scene: string\n"
"\tSet the name of scene the actuator will switch to.\n" ;
PyObject* KX_SceneActuator::PySetScene(PyObject* self, 
									   PyObject* args, 
									   PyObject* kwds)
{
	ShowDeprecationWarning("setScene()", "the scene property");
	/* one argument: a scene, ignore the rest */
	char *scene_name;

	if(!PyArg_ParseTuple(args, "s:setScene", &scene_name))
	{
		return NULL;
	}

	/* Scene switch is done by name. */
	m_nextSceneName = scene_name;

	Py_RETURN_NONE;
}



/* 5. getScene:                                                              */
const char KX_SceneActuator::GetScene_doc[] = 
"getScene()\n"
"\tReturn the name of the scene the actuator wants to switch to.\n" ;
PyObject* KX_SceneActuator::PyGetScene(PyObject* self, 
									   PyObject* args, 
									   PyObject* kwds)
{
	ShowDeprecationWarning("getScene()", "the scene property");
	return PyString_FromString(m_nextSceneName);
}



/* 6. set camera------------------------------------------------------------ */
const char KX_SceneActuator::SetCamera_doc[] = 
"setCamera(camera)\n"
"\t- camera: string\n"
"\tSet the camera to switch to.\n" ;
PyObject* KX_SceneActuator::PySetCamera(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	ShowDeprecationWarning("setCamera()", "the camera property");
	PyObject *cam;
	if (PyArg_ParseTuple(args, "O!:setCamera", &KX_Camera::Type, &cam))
	{
		KX_Camera *new_camera;
		
		new_camera = static_cast<KX_Camera*>BGE_PROXY_REF(cam);
		if(new_camera==NULL)
		{
			PyErr_SetString(PyExc_RuntimeError, BGE_PROXY_ERROR_MSG);
			return NULL;
		}
		
		if (m_camera)
			m_camera->UnregisterActuator(this);
		
		m_camera= new_camera;
		
		m_camera->RegisterActuator(this);
		Py_RETURN_NONE;
	}
	PyErr_Clear();

	/* one argument: a scene, ignore the rest */
	char *camName;
	if(!PyArg_ParseTuple(args, "s:setCamera", &camName))
	{
		return NULL;
	}

	KX_Camera *camOb = FindCamera(camName);
	if (camOb) 
	{
		if (m_camera)
			m_camera->UnregisterActuator(this);
		m_camera = camOb;
		m_camera->RegisterActuator(this);
	}

	Py_RETURN_NONE;
}



/* 7. getCamera:                                                             */
const char KX_SceneActuator::GetCamera_doc[] = 
"getCamera()\n"
"\tReturn the name of the camera to switch to.\n" ;
PyObject* KX_SceneActuator::PyGetCamera(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	ShowDeprecationWarning("getCamera()", "the camera property");
	if (m_camera) {
		return PyString_FromString(m_camera->GetName());
	}
	else {
		Py_RETURN_NONE;
	}
}
/* eof */
