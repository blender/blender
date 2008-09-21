//
// Replace the mesh for this actuator's parent
//
// $Id$
//
// ***** BEGIN GPL LICENSE BLOCK *****
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
// All rights reserved.
//
// The Original Code is: all of this file.
//
// Contributor(s): none yet.
//
// ***** END GPL LICENSE BLOCK *****

//
// Previously existed as:

// \source\gameengine\GameLogic\SCA_ReplaceMeshActuator.cpp

// Please look here for revision history.

#include "KX_SCA_ReplaceMeshActuator.h"

#include "PyObjectPlus.h" 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */

	PyTypeObject 

KX_SCA_ReplaceMeshActuator::

Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_SCA_ReplaceMeshActuator",
	sizeof(KX_SCA_ReplaceMeshActuator),
	0,
	PyDestructor,
	0,
	__getattr,
	__setattr,
	0, 
	__repr,
	0, 
	0,
	0,
	0,
	0
};

PyParentObject KX_SCA_ReplaceMeshActuator::Parents[] = {
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};



PyMethodDef KX_SCA_ReplaceMeshActuator::Methods[] = {
	{"setMesh", (PyCFunction) KX_SCA_ReplaceMeshActuator::sPySetMesh, METH_O, SetMesh_doc},
	
	KX_PYMETHODTABLE(KX_SCA_ReplaceMeshActuator, instantReplaceMesh),
   	KX_PYMETHODTABLE(KX_SCA_ReplaceMeshActuator, getMesh),
	{NULL,NULL} //Sentinel
};



PyObject* KX_SCA_ReplaceMeshActuator::_getattr(const STR_String& attr)
{
  _getattr_up(SCA_IActuator);
}



/* 1. setMesh */
const char KX_SCA_ReplaceMeshActuator::SetMesh_doc[] = 
	"setMesh(name)\n"
	"\t- name: string or None\n"
	"\tSet the mesh that will be substituted for the current one.\n";

PyObject* KX_SCA_ReplaceMeshActuator::PySetMesh(PyObject* self, PyObject* value)
{
	if (value == Py_None) {
		m_mesh = NULL;
	} else {
		char* meshname = PyString_AsString(value);
		if (!meshname) {
			PyErr_SetString(PyExc_ValueError, "Expected the name of a mesh or None");
			return NULL;
		}
		void* mesh = SCA_ILogicBrick::m_sCurrentLogicManager->GetMeshByName(STR_String(meshname));
		
		if (mesh==NULL) {
			PyErr_SetString(PyExc_ValueError, "The mesh name given does not exist");
			return NULL;
		}
		m_mesh= (class RAS_MeshObject*)mesh;
	}
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_SCA_ReplaceMeshActuator, getMesh,
"getMesh() -> string\n"
"Returns the name of the mesh to be substituted.\n"
)
{
	if (!m_mesh)
		Py_RETURN_NONE;

	return PyString_FromString(const_cast<char *>(m_mesh->GetName().ReadPtr()));
}


KX_PYMETHODDEF_DOC(KX_SCA_ReplaceMeshActuator, instantReplaceMesh,
"instantReplaceMesh() : immediately replace mesh without delay\n")
{
	InstantReplaceMesh();
	Py_RETURN_NONE;
}

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_SCA_ReplaceMeshActuator::KX_SCA_ReplaceMeshActuator(SCA_IObject *gameobj,
													   class RAS_MeshObject *mesh,
													   SCA_IScene* scene,
													   PyTypeObject* T) : 

	SCA_IActuator(gameobj, T),
	m_mesh(mesh),
	m_scene(scene)
{
} /* End of constructor */



KX_SCA_ReplaceMeshActuator::~KX_SCA_ReplaceMeshActuator()
{ 
	// there's nothing to be done here, really....
} /* end of destructor */



bool KX_SCA_ReplaceMeshActuator::Update()
{
	// bool result = false;	/*unused*/
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events

	if (m_mesh) m_scene->ReplaceMesh(GetParent(),m_mesh);

	return false;
}



CValue* KX_SCA_ReplaceMeshActuator::GetReplica()
{
	KX_SCA_ReplaceMeshActuator* replica = 
		new KX_SCA_ReplaceMeshActuator(*this);

	if (replica == NULL)
		return NULL;

	replica->ProcessReplica();

	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
};

void KX_SCA_ReplaceMeshActuator::InstantReplaceMesh()
{
	if (m_mesh) m_scene->ReplaceMesh(GetParent(),m_mesh);
}

/* eof */
