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
#include "PyObjectPlus.h"

#ifndef DISABLE_PYTHON
#include "compile.h"
#include "eval.h"
#endif // DISABLE_PYTHON

#include <algorithm>


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// initialize static member variables
SCA_PythonController* SCA_PythonController::m_sCurrentController = NULL;


SCA_PythonController::SCA_PythonController(SCA_IObject* gameobj, int mode)
	: SCA_IController(gameobj),
#ifndef DISABLE_PYTHON
	m_bytecode(NULL),
	m_function(NULL),
#endif
	m_function_argc(0),
	m_bModified(true),
	m_debug(false),
	m_mode(mode)
#ifndef DISABLE_PYTHON
	, m_pythondictionary(NULL)
#endif

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

#ifndef DISABLE_PYTHON
	//printf("released python byte script\n");
	
	Py_XDECREF(m_bytecode);
	Py_XDECREF(m_function);
	
	if (m_pythondictionary) {
		// break any circular references in the dictionary
		PyDict_Clear(m_pythondictionary);
		Py_DECREF(m_pythondictionary);
	}
#endif
}



CValue* SCA_PythonController::GetReplica()
{
	SCA_PythonController* replica = new SCA_PythonController(*this);

#ifndef DISABLE_PYTHON
	/* why is this needed at all??? - m_bytecode is NULL'd below so this doesnt make sense
	 * but removing it crashes blender (with YoFrankie). so leave in for now - Campbell */
	Py_XINCREF(replica->m_bytecode);
	
	Py_XINCREF(replica->m_function); // this is ok since its not set to NULL
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
#endif
	
	// this will copy properties and so on...
	replica->ProcessReplica();

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


#ifndef DISABLE_PYTHON
void SCA_PythonController::SetNamespace(PyObject*	pythondictionary)
{
	if (m_pythondictionary)
	{
		PyDict_Clear(m_pythondictionary);
		Py_DECREF(m_pythondictionary);
	}
	m_pythondictionary = PyDict_Copy(pythondictionary); /* new reference */
	
	/* Without __file__ set the sys.argv[0] is used for the filename
	 * which ends up with lines from the blender binary being printed in the console */
	PyDict_SetItemString(m_pythondictionary, "__file__", PyUnicode_FromString(m_scriptName.Ptr()));
	
}
#endif

int SCA_PythonController::IsTriggered(class SCA_ISensor* sensor)
{
	if (std::find(m_triggeredSensors.begin(), m_triggeredSensors.end(), sensor) != 
		m_triggeredSensors.end())
		return 1;
	return 0;
}

#ifndef DISABLE_PYTHON

/* warning, self is not the SCA_PythonController, its a PyObjectPlus_Proxy */
PyObject* SCA_PythonController::sPyGetCurrentController(PyObject *self)
{
	if(m_sCurrentController==NULL)
	{
		PyErr_SetString(PyExc_SystemError, "GameLogic.getCurrentController(), this function is being run outside the python controllers context, or blenders internal state is corrupt.");
		return NULL;
	}
	return m_sCurrentController->GetProxy();
}

SCA_IActuator* SCA_PythonController::LinkedActuatorFromPy(PyObject *value)
{
	// for safety, todo: only allow for registered actuators (pointertable)
	// we don't want to crash gameengine/blender by python scripts
	std::vector<SCA_IActuator*> lacts =  m_sCurrentController->GetLinkedActuators();
	std::vector<SCA_IActuator*>::iterator it;
	
	if (PyUnicode_Check(value)) {
		/* get the actuator from the name */
		char *name= _PyUnicode_AsString(value);
		for(it = lacts.begin(); it!= lacts.end(); ++it) {
			if( name == (*it)->GetName() ) {
				return *it;
			}
		}
	}
	else if (PyObject_TypeCheck(value, &SCA_IActuator::Type)) {
		PyObjectPlus *value_plus= BGE_PROXY_REF(value);
		for(it = lacts.begin(); it!= lacts.end(); ++it) {
			if( static_cast<SCA_IActuator*>(value_plus) == (*it) ) {
				return *it;
			}
		}
	}
	
	/* set the exception */
	PyObject *value_str = PyObject_Repr(value); /* new ref */
	PyErr_Format(PyExc_ValueError, "'%s' not in this python controllers actuator list", _PyUnicode_AsString(value_str));
	Py_DECREF(value_str);
	
	return false;
}

const char* SCA_PythonController::sPyGetCurrentController__doc__ = "getCurrentController()";

PyTypeObject SCA_PythonController::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_PythonController",
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
	&SCA_IController::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_PythonController::Methods[] = {
	{"activate", (PyCFunction) SCA_PythonController::sPyActivate, METH_O},
	{"deactivate", (PyCFunction) SCA_PythonController::sPyDeActivate, METH_O},
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_PythonController::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("script", SCA_PythonController, pyattr_get_script, pyattr_set_script),
	KX_PYATTRIBUTE_INT_RO("mode", SCA_PythonController, m_mode),
	{ NULL }	//Sentinel
};

void SCA_PythonController::ErrorPrint(const char *error_msg)
{
	// didn't compile, so instead of compile, complain
	// something is wrong, tell the user what went wrong
	printf("%s - controller \"%s\":\n", error_msg, GetName().Ptr());
	//PyRun_SimpleString(m_scriptText.Ptr());
	PyErr_Print();
	
	/* Added in 2.48a, the last_traceback can reference Objects for example, increasing
	 * their user count. Not to mention holding references to wrapped data.
	 * This is especially bad when the PyObject for the wrapped data is free'd, after blender 
	 * has alredy dealocated the pointer */
	PySys_SetObject( (char *)"last_traceback", NULL);
	PyErr_Clear(); /* just to be sure */
}

bool SCA_PythonController::Compile()
{	
	//printf("py script modified '%s'\n", m_scriptName.Ptr());
	m_bModified= false;
	
	// if a script already exists, decref it before replace the pointer to a new script
	if (m_bytecode) {
		Py_DECREF(m_bytecode);
		m_bytecode=NULL;
	}
	
	// recompile the scripttext into bytecode
	m_bytecode = Py_CompileString(m_scriptText.Ptr(), m_scriptName.Ptr(), Py_file_input);
	
	if (m_bytecode) {
		return true;
	} else {
		ErrorPrint("Python error compiling script");
		return false;
	}
}

bool SCA_PythonController::Import()
{
	//printf("py module modified '%s'\n", m_scriptName.Ptr());
	m_bModified= false;
	
	/* incase we re-import */
	Py_XDECREF(m_function);
	m_function= NULL;
	
	vector<STR_String> py_function_path = m_scriptText.Explode('.');
	
	if(py_function_path.size() < 2) {
		printf("Python module name formatting error \"%s\":\n\texpected \"SomeModule.Func\", got \"%s\"\n", GetName().Ptr(), m_scriptText.Ptr());
		return false;
	}
	
	PyObject *mod = PyImport_ImportModule((char *)py_function_path[0].Ptr());
	/* Dont reload yet, do this within the loop so packages reload too */
	
	if(mod==NULL) {
		ErrorPrint("Python module not found");
		return false;
	}
	/* 'mod' will be DECREF'd as 'base' 
	 * 'm_function' will be left holding a reference that the controller owns */
	
	PyObject *base= mod;
	
	for(unsigned int i=1; i < py_function_path.size(); i++) {
		if(m_debug && PyModule_Check(base)) { /* base could be a class */
			Py_DECREF(base); /* getting a new one so dont hold a ref to the old one */
			base= PyImport_ReloadModule(base);
			if (base==NULL) {
				m_function= NULL;
				break;
			}
		}
		
		m_function = PyObject_GetAttrString(base, py_function_path[i].Ptr());
		Py_DECREF(base);
		base = m_function; /* for the next loop if there is on */
		
		if(m_function==NULL) {
			break;
		}
	}
	
	if(m_function==NULL) {
		if(PyErr_Occurred())
			ErrorPrint("Python controller found the module but could not access the function");
		else
			printf("Python module error \"%s\":\n \"%s\" module found but function missing\n", GetName().Ptr(), m_scriptText.Ptr());
		return false;
	}
	
	if(!PyCallable_Check(m_function)) {
		Py_DECREF(m_function);
		printf("Python module function error \"%s\":\n \"%s\" not callable\n", GetName().Ptr(), m_scriptText.Ptr());
		return false;
	}
	
	m_function_argc = 0; /* rare cases this could be a function that isnt defined in python, assume zero args */
	if (PyFunction_Check(m_function)) {
		PyObject *py_arg_count = PyObject_GetAttrString(PyFunction_GET_CODE(m_function), "co_argcount");
		if(py_arg_count) {
			m_function_argc = PyLong_AsLong(py_arg_count);
			Py_DECREF(py_arg_count);
		}
		else {
			PyErr_Clear(); /* unlikely to fail but just incase */
		}
	}
	
	if(m_function_argc > 1) {
		Py_DECREF(m_function);
		printf("Python module function has \"%s\":\n \"%s\" takes %d args, should be zero or 1 controller arg\n", GetName().Ptr(), m_scriptText.Ptr(), m_function_argc);
		return false;
	}
	
	return true;
}


void SCA_PythonController::Trigger(SCA_LogicManager* logicmgr)
{
	m_sCurrentController = this;
	m_sCurrentLogicManager = logicmgr;
	
	PyObject *excdict=		NULL;
	PyObject* resultobj=	NULL;
	
	switch(m_mode) {
	case SCA_PYEXEC_SCRIPT:
	{
		if (m_bModified)
			if (Compile()==false) // sets m_bModified to false
				return;
		if (!m_bytecode)
			return;
		
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

		excdict= PyDict_Copy(m_pythondictionary);
		resultobj = PyEval_EvalCode((PyCodeObject*)m_bytecode, excdict, excdict);
		/* PyRun_SimpleString(m_scriptText.Ptr()); */
		break;
	}
	case SCA_PYEXEC_MODULE:
	{
		if (m_bModified || m_debug)
			if (Import()==false) // sets m_bModified to false
				return;
		if (!m_function)
			return;
		
		PyObject *args= NULL;
		
		if(m_function_argc==1) {
			args = PyTuple_New(1);
			PyTuple_SET_ITEM(args, 0, GetProxy());
		}
		
		resultobj = PyObject_CallObject(m_function, args);
		Py_XDECREF(args);
		break;
	}
	
	} /* end switch */
	
	
	
	/* Free the return value and print the error */
	if (resultobj)
	{
		Py_DECREF(resultobj);
	}
	else
	{
		// something is wrong, tell the user what went wrong
		printf("Python script error from controller \"%s\":\n", GetName().Ptr());
		PyErr_Print();
		
		/* Added in 2.48a, the last_traceback can reference Objects for example, increasing
		 * their user count. Not to mention holding references to wrapped data.
		 * This is especially bad when the PyObject for the wrapped data is free'd, after blender 
		 * has alredy dealocated the pointer */
		PySys_SetObject( (char *)"last_traceback", NULL);
		PyErr_Clear(); /* just to be sure */
	}
	
	if(excdict) /* Only for SCA_PYEXEC_SCRIPT types */
	{
		/* clear after PyErrPrint - seems it can be using
		 * something in this dictionary and crash? */
		PyDict_Clear(excdict);
		Py_DECREF(excdict);
	}	
	
	m_triggeredSensors.clear();
	m_sCurrentController = NULL;
}

PyObject* SCA_PythonController::PyActivate(PyObject *value)
{
	if(m_sCurrentController != this) {
		PyErr_SetString(PyExc_SystemError, "Cannot add an actuator from a non-active controller");
		return NULL;
	}
	
	SCA_IActuator* actu = LinkedActuatorFromPy(value);
	if(actu==NULL)
		return NULL;
	
	m_sCurrentLogicManager->AddActiveActuator((SCA_IActuator*)actu, true);
	Py_RETURN_NONE;
}

PyObject* SCA_PythonController::PyDeActivate(PyObject *value)
{
	if(m_sCurrentController != this) {
		PyErr_SetString(PyExc_SystemError, "Cannot add an actuator from a non-active controller");
		return NULL;
	}
	
	SCA_IActuator* actu = LinkedActuatorFromPy(value);
	if(actu==NULL)
		return NULL;
	
	m_sCurrentLogicManager->AddActiveActuator((SCA_IActuator*)actu, false);
	Py_RETURN_NONE;
}

PyObject* SCA_PythonController::pyattr_get_script(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	//SCA_PythonController* self= static_cast<SCA_PythonController*>(static_cast<SCA_IController*>(static_cast<SCA_ILogicBrick*>(static_cast<CValue*>(static_cast<PyObjectPlus*>(self_v)))));
	// static_cast<void *>(dynamic_cast<Derived *>(obj)) - static_cast<void *>(obj)

	SCA_PythonController* self= static_cast<SCA_PythonController*>(self_v);
	return PyUnicode_FromString(self->m_scriptText);
}



int SCA_PythonController::pyattr_set_script(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	SCA_PythonController* self= static_cast<SCA_PythonController*>(self_v);
	
	char *scriptArg = _PyUnicode_AsString(value);
	
	if (scriptArg==NULL) {
		PyErr_SetString(PyExc_TypeError, "controller.script = string: Python Controller, expected a string script text");
		return PY_SET_ATTR_FAIL;
	}

	/* set scripttext sets m_bModified to true, 
		so next time the script is needed, a reparse into byte code is done */
	self->SetScriptText(scriptArg);
		
	return PY_SET_ATTR_SUCCESS;
}

#else // DISABLE_PYTHON

void SCA_PythonController::Trigger(SCA_LogicManager* logicmgr)
{
	/* intentionally blank */
}

#endif // DISABLE_PYTHON

/* eof */
