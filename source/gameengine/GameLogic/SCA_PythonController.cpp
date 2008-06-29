/**
 * Execute Python scripts
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

#include "SCA_PythonController.h"
#include "SCA_LogicManager.h"
#include "SCA_ISensor.h"
#include "SCA_IActuator.h"
#include "compile.h"
#include "eval.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// initialize static member variables
SCA_PythonController* SCA_PythonController::m_sCurrentController = NULL;


SCA_PythonController::SCA_PythonController(SCA_IObject* gameobj,
										   PyTypeObject* T)
	: SCA_IController(gameobj, T),
	m_bytecode(NULL),
	m_bModified(true),
	m_pythondictionary(NULL)
{
}

/*
//debugging
CValue*		SCA_PythonController::AddRef()
{
	//printf("AddRef refcount = %i\n",GetRefCount());
	return CValue::AddRef();
}
int			SCA_PythonController::Release()
{
	//printf("Release refcount = %i\n",GetRefCount());
	return CValue::Release();
}
*/



SCA_PythonController::~SCA_PythonController()
{
	if (m_bytecode)
	{
		//
		//printf("released python byte script\n");
		Py_DECREF(m_bytecode);
	}
	
	if (m_pythondictionary)
	{
		// break any circular references in the dictionary
		PyDict_Clear(m_pythondictionary);
		Py_DECREF(m_pythondictionary);
	}
}



CValue* SCA_PythonController::GetReplica()
{
	SCA_PythonController* replica = new SCA_PythonController(*this);
	// Copy the compiled bytecode if possible.
	Py_XINCREF(replica->m_bytecode);
	replica->m_bModified = replica->m_bytecode == NULL;
	
	// The replica->m_pythondictionary is stolen - replace with a copy.
	if (m_pythondictionary)
		replica->m_pythondictionary = PyDict_Copy(m_pythondictionary);
		
	/*
	// The other option is to incref the replica->m_pythondictionary -
	// the replica objects can then share data.
	if (m_pythondictionary)
		Py_INCREF(replica->m_pythondictionary);
	*/
	
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}



void SCA_PythonController::SetScriptText(const STR_String& text)
{ 
	m_scriptText = "import GameLogic\n" + text;
	m_bModified = true;
}



void SCA_PythonController::SetScriptName(const STR_String& name)
{
	m_scriptName = name;
}



void SCA_PythonController::SetDictionary(PyObject*	pythondictionary)
{
	if (m_pythondictionary)
	{
		PyDict_Clear(m_pythondictionary);
		Py_DECREF(m_pythondictionary);
	}
	m_pythondictionary = PyDict_Copy(pythondictionary); /* new reference */
}

#if 0
static char* sPyGetCurrentController__doc__;
#endif


PyObject* SCA_PythonController::sPyGetCurrentController(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds)
{
	m_sCurrentController->AddRef();
	return m_sCurrentController;
}

#if 0
static char* sPyAddActiveActuator__doc__;
#endif

PyObject* SCA_PythonController::sPyAddActiveActuator(
	  
		PyObject* self, 
		PyObject* args, 
		PyObject* kwds)
{
	
	PyObject* ob1;
	int activate;
	if (!PyArg_ParseTuple(args, "Oi", &ob1,&activate))
		return NULL;

	// for safety, todo: only allow for registered actuators (pointertable)
	// we don't want to crash gameengine/blender by python scripts
	std::vector<SCA_IActuator*> lacts =  m_sCurrentController->GetLinkedActuators();
	
	std::vector<SCA_IActuator*>::iterator it;
	bool found = false;
	CValue* act = (CValue*)ob1;

	for(it = lacts.begin(); it!= lacts.end(); it++) {
		if( static_cast<SCA_IActuator*>(act) == (*it) ) {
			found=true;
			break;
		}
	}
	if(found){
		CValue* boolval = new CBoolValue(activate!=0);
		m_sCurrentLogicManager->AddActiveActuator((SCA_IActuator*)act,boolval);
		boolval->Release();
	}
	Py_INCREF(Py_None);
	return Py_None;
}


char* SCA_PythonController::sPyGetCurrentController__doc__ = "getCurrentController()";
char* SCA_PythonController::sPyAddActiveActuator__doc__= "addActiveActuator(actuator,bool)";
char SCA_PythonController::GetActuators_doc[] = "getActuator";

PyTypeObject SCA_PythonController::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"SCA_PythonController",
		sizeof(SCA_PythonController),
		0,
		PyDestructor,
		0,
		__getattr,
		__setattr,
		0, //&MyPyCompare,
		__repr,
		0, //&cvalue_as_number,
		0,
		0,
		0,
		0
};

PyParentObject SCA_PythonController::Parents[] = {
	&SCA_PythonController::Type,
	&SCA_IController::Type,
	&CValue::Type,
	NULL
};
PyMethodDef SCA_PythonController::Methods[] = {
	{"getActuators", (PyCFunction) SCA_PythonController::sPyGetActuators, 
		METH_VARARGS, SCA_PythonController::GetActuators_doc},
	{"getActuator", (PyCFunction) SCA_PythonController::sPyGetActuator, 
	METH_VARARGS, SCA_PythonController::GetActuator_doc},
	{"getSensors", (PyCFunction) SCA_PythonController::sPyGetSensors, 
	METH_VARARGS, SCA_PythonController::GetSensors_doc},
	{"getSensor", (PyCFunction) SCA_PythonController::sPyGetSensor, 
	METH_VARARGS, SCA_PythonController::GetSensor_doc},
	{"getScript", (PyCFunction) SCA_PythonController::sPyGetScript, METH_VARARGS},
	{"setScript", (PyCFunction) SCA_PythonController::sPySetScript, METH_VARARGS},
	{"getState", (PyCFunction) SCA_PythonController::sPyGetState, METH_VARARGS},
	{NULL,NULL} //Sentinel
};


void SCA_PythonController::Trigger(SCA_LogicManager* logicmgr)
{
	m_sCurrentController = this;
	m_sCurrentLogicManager = logicmgr;
	
	if (m_bModified)
	{
		// if a script already exists, decref it before replace the pointer to a new script
		if (m_bytecode)
		{
			Py_DECREF(m_bytecode);
			m_bytecode=NULL;
		}
		// recompile the scripttext into bytecode
		m_bytecode = Py_CompileString(m_scriptText.Ptr(), m_scriptName.Ptr(), Py_file_input);
		if (!m_bytecode)
		{
			// didn't compile, so instead of compile, complain
			// something is wrong, tell the user what went wrong
			printf("PYTHON SCRIPT ERROR:\n");
			//PyRun_SimpleString(m_scriptText.Ptr());
			PyErr_Print();
			return;
		}
		m_bModified=false;
	}
	
		/*
		 * This part here with excdict is a temporary patch
		 * to avoid python/gameengine crashes when python
		 * inadvertently holds references to game objects
		 * in global variables.
		 * 
		 * The idea is always make a fresh dictionary, and
		 * destroy it right after it is used to make sure
		 * python won't hold any gameobject references.
		 * 
		 * Note that the PyDict_Clear _is_ necessary before
		 * the Py_DECREF() because it is possible for the
		 * variables inside the dictionary to hold references
		 * to the dictionary (ie. generate a cycle), so we
		 * break it by hand, then DECREF (which in this case
		 * should always ensure excdict is cleared).
		 */
/*	PyObject *excdict= myPyDict_Copy(m_pythondictionary);
	struct _object* resultobj = PyEval_EvalCode((PyCodeObject*)m_bytecode,
		excdict, 
		excdict
		);
	PyDict_Clear(excdict);
	Py_DECREF(excdict);*/


#if 1
	PyObject *excdict= PyDict_Copy(m_pythondictionary);
	PyObject* resultobj = PyEval_EvalCode((PyCodeObject*)m_bytecode,
		excdict, 
		excdict
		);
	PyDict_Clear(excdict);
	Py_DECREF(excdict);
#else

	PyObject* resultobj = PyEval_EvalCode((PyCodeObject*)m_bytecode,
		m_pythondictionary, 
		m_pythondictionary
		);

#endif

	if (resultobj)
	{
		Py_DECREF(resultobj);
	} else
	{
		// something is wrong, tell the user what went wrong
		printf("PYTHON SCRIPT ERROR:\n");
		PyErr_Print();
		//PyRun_SimpleString(m_scriptText.Ptr());
	}

	m_sCurrentController = NULL;
}



PyObject* SCA_PythonController::_getattr(const STR_String& attr)
{
	_getattr_up(SCA_IController);
}



PyObject* SCA_PythonController::PyGetActuators(PyObject* self, 
								   PyObject* args, 
								   PyObject* kwds)
{
	PyObject* resultlist = PyList_New(m_linkedactuators.size());
	for (unsigned int index=0;index<m_linkedactuators.size();index++)
	{
		PyList_SetItem(resultlist,index,m_linkedactuators[index]->AddRef());
	}

	return resultlist;
}

char SCA_PythonController::GetSensor_doc[] = 
"GetSensor (char sensorname) return linked sensor that is named [sensorname]\n";
PyObject*
SCA_PythonController::PyGetSensor(PyObject* self, 
								 PyObject* args, 
								 PyObject* kwds)
{

	char *scriptArg;

	if (!PyArg_ParseTuple(args, "s", &scriptArg)) {
		return NULL;
	}
	
	for (unsigned int index=0;index<m_linkedsensors.size();index++)
	{
		SCA_ISensor* sensor = m_linkedsensors[index];
		STR_String realname = sensor->GetName();
		if (realname == scriptArg)
		{
			return sensor->AddRef();
		}
	}
		
	PyErr_SetString(PyExc_AttributeError, "Unable to find requested sensor");
	return NULL;
}



char SCA_PythonController::GetActuator_doc[] = 
"GetActuator (char sensorname) return linked actuator that is named [actuatorname]\n";
PyObject*
SCA_PythonController::PyGetActuator(PyObject* self, 
								 PyObject* args, 
								 PyObject* kwds)
{

	char *scriptArg;

	if (!PyArg_ParseTuple(args, "s", &scriptArg)) {
		return NULL;
	}
	
	for (unsigned int index=0;index<m_linkedactuators.size();index++)
	{
		SCA_IActuator* actua = m_linkedactuators[index];
		STR_String realname = actua->GetName();
		if (realname == scriptArg)
		{
			return actua->AddRef();
		}
	}
		
	PyErr_SetString(PyExc_AttributeError, "Unable to find requested actuator");
	return NULL;
}


char SCA_PythonController::GetSensors_doc[]   = "getSensors returns a list of all attached sensors";
PyObject*
SCA_PythonController::PyGetSensors(PyObject* self, 
								 PyObject* args, 
								 PyObject* kwds)
{
	PyObject* resultlist = PyList_New(m_linkedsensors.size());
	for (unsigned int index=0;index<m_linkedsensors.size();index++)
	{
		PyList_SetItem(resultlist,index,m_linkedsensors[index]->AddRef());
	}
	
	return resultlist;
}

/* 1. getScript */
PyObject* SCA_PythonController::PyGetScript(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds)
{
	return PyString_FromString(m_scriptText);
}

/* 2. setScript */
PyObject* SCA_PythonController::PySetScript(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds)
{
	char *scriptArg;
	if (!PyArg_ParseTuple(args, "s", &scriptArg)) {
		return NULL;
	}
	
	/* set scripttext sets m_bModified to true, 
		so next time the script is needed, a reparse into byte code is done */

	this->SetScriptText(scriptArg);
	
	Py_Return;
}

/* 1. getScript */
PyObject* SCA_PythonController::PyGetState(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds)
{
	return PyInt_FromLong(m_statemask);
}

/* eof */
