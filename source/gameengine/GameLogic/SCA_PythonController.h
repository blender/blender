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

#ifndef KX_PYTHONCONTROLLER_H
#define KX_PYTHONCONTROLLER_H
	      
#include "SCA_IController.h"
#include "SCA_LogicManager.h"
#include "BoolValue.h"

class SCA_IObject;
class SCA_PythonController : public SCA_IController
{
	Py_Header;
	struct _object *		m_bytecode;
	bool					m_bModified;

 protected:
	STR_String				m_scriptText;
	STR_String				m_scriptName;
	PyObject*				m_pythondictionary;

 public: 
	static SCA_PythonController* m_sCurrentController; // protected !!!

	//for debugging
	//virtual	CValue*		AddRef();
	//virtual int			Release();												// Release a reference to this value (when reference count reaches 0, the value is removed from the heap)

	SCA_PythonController(SCA_IObject* gameobj,PyTypeObject* T = &Type);
	virtual ~SCA_PythonController();

	virtual CValue* GetReplica();
	virtual void  Trigger(class SCA_LogicManager* logicmgr);
  
	void	SetScriptText(const STR_String& text);
	void	SetScriptName(const STR_String& name);
	void	SetDictionary(PyObject*	pythondictionary);

	static char* sPyGetCurrentController__doc__;
	static PyObject* sPyGetCurrentController(PyObject* self);
	static char* sPyAddActiveActuator__doc__;
	static PyObject* sPyAddActiveActuator(PyObject* self, 
										  PyObject* args);
	virtual PyObject* _getattr(const STR_String& attr);

	KX_PYMETHOD_DOC_NOARGS(SCA_PythonController,GetSensors);
	KX_PYMETHOD_DOC_NOARGS(SCA_PythonController,GetActuators);
	KX_PYMETHOD_DOC_O(SCA_PythonController,GetSensor);
	KX_PYMETHOD_DOC_O(SCA_PythonController,GetActuator);
	KX_PYMETHOD_O(SCA_PythonController,SetScript);
	KX_PYMETHOD_NOARGS(SCA_PythonController,GetScript);
	KX_PYMETHOD_NOARGS(SCA_PythonController,GetState);
	

};

#endif //KX_PYTHONCONTROLLER_H

