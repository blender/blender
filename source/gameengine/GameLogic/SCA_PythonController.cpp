/**
 * Execute Python scripts
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

#include "SCA_PythonController.h"
#include "SCA_LogicManager.h"
#include "SCA_ISensor.h"
#include "SCA_IActuator.h"
#include "compile.h"
#include "eval.h"


// initialize static member variables
SCA_PythonController* SCA_PythonController::m_sCurrentController = NULL;


SCA_PythonController::SCA_PythonController(SCA_IObject* gameobj,
										   PyTypeObject* T)
	: SCA_IController(gameobj, T),
	m_pythondictionary(NULL),
	m_bytecode(NULL),
	m_bModified(true)
{
}



SCA_PythonController::~SCA_PythonController()
{
	if (m_bytecode)
	{
		//
		//printf("released python byte script\n");
		Py_DECREF(m_bytecode);
	}
}



CValue* SCA_PythonController::GetReplica()
{
	SCA_PythonController* replica = new SCA_PythonController(*this);
	replica->m_bytecode = NULL;
	replica->m_bModified = true;
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}



void SCA_PythonController::SetScriptText(const STR_String& text)
{ 
	m_scriptText = text;
	m_bModified = true;
}



void SCA_PythonController::SetScriptName(const STR_String& name)
{
	m_scriptName = name;
}



void SCA_PythonController::SetDictionary(PyObject*	pythondictionary)
{
	m_pythondictionary = pythondictionary;
}


static char* sPyGetCurrentController__doc__;


PyObject* SCA_PythonController::sPyGetCurrentController(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds)
{
	m_sCurrentController->AddRef();
	return m_sCurrentController;
}


static char* sPyAddActiveActuator__doc__;
	
  
PyObject* SCA_PythonController::sPyAddActiveActuator(
	  
		PyObject* self, 
		PyObject* args, 
		PyObject* kwds)
{
	
	PyObject* ob1;
	int activate;
	if (!PyArg_ParseTuple(args, "Oi", &ob1,&activate))
	{
		return NULL;
		
	}
	// for safety, todo: only allow for registered actuators (pointertable)
	// we don't want to crash gameengine/blender by python scripts
	
	CValue* ac = (CValue*)ob1;
	CValue* boolval = new CBoolValue(activate!=0);
	m_sCurrentLogicManager->AddActiveActuator((SCA_IActuator*)ac,boolval);
	boolval->Release();
	
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
	METH_VARARGS, SCA_PythonController::GetSensor_doc}
	,
	{NULL,NULL} //Sentinel
};



	/* XXX, function should be removed and PyDict_Copy used
	 * once we switch to all builds using Python 2.0 - zr */
static PyObject *myPyDict_Copy(PyObject *odict)
{
	PyObject *ndict= PyDict_New();
	PyObject *key, *val;
	int ppos= 0;
	
	while (PyDict_Next(odict, &ppos, &key, &val))
		PyDict_SetItem(ndict, key, val);
	
	return ndict;
}

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
		if (m_bytecode)
		{
			// store the
			int i=0;
			i+=2; // so compiler doesn't complain about unused variable
			PyRun_SimpleString("import GameLogic\n");
		} else
		{
			// didn't compile, so instead of compile, complain
			int i=0;
			i++; // so compiler doesn't complain about unused variable
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
	PyObject *excdict= myPyDict_Copy(m_pythondictionary);
	struct _object* resultobj = PyEval_EvalCode((PyCodeObject*)m_bytecode,
		excdict, 
		excdict
		);
	PyDict_Clear(excdict);
	Py_DECREF(excdict);
	
	if (resultobj)
	{
		Py_DECREF(resultobj);
	} else
	{
		// something is wrong, tell the user what went wrong
		printf("PYTHON SCRIPT ERROR:\n");
		PyRun_SimpleString(m_scriptText.Ptr());
	}

	m_sCurrentController = NULL;
}



PyObject* SCA_PythonController::_getattr(char* attr)
{
	_getattr_up(SCA_IController);
}



PyObject* SCA_PythonController::PyGetActuators(PyObject* self, 
								   PyObject* args, 
								   PyObject* kwds)
{
	int index;
	
	PyObject* resultlist = PyList_New(m_linkedactuators.size());
	for (index=0;index<m_linkedactuators.size();index++)
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
	
	int index;
	for (index=0;index<m_linkedsensors.size();index++)
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
	
	int index;
	for (index=0;index<m_linkedactuators.size();index++)
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
	int index;
	
	PyObject* resultlist = PyList_New(m_linkedsensors.size());
	for (index=0;index<m_linkedsensors.size();index++)
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

/* eof */
