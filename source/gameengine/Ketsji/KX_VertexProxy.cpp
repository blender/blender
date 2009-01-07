/**
 * $Id$
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "KX_VertexProxy.h"
#include "KX_MeshProxy.h"
#include "RAS_TexVert.h"

#include "KX_PyMath.h"

PyTypeObject KX_VertexProxy::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_VertexProxy",
	sizeof(KX_VertexProxy),
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

PyParentObject KX_VertexProxy::Parents[] = {
	&KX_VertexProxy::Type,
	&SCA_IObject::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_VertexProxy::Methods[] = {
{"getXYZ", (PyCFunction)KX_VertexProxy::sPyGetXYZ,METH_VARARGS},
{"setXYZ", (PyCFunction)KX_VertexProxy::sPySetXYZ,METH_VARARGS},
{"getUV", (PyCFunction)KX_VertexProxy::sPyGetUV,METH_VARARGS},
{"setUV", (PyCFunction)KX_VertexProxy::sPySetUV,METH_VARARGS},

{"getUV2", (PyCFunction)KX_VertexProxy::sPyGetUV2,METH_VARARGS},
{"setUV2", (PyCFunction)KX_VertexProxy::sPySetUV2,METH_VARARGS},

{"getRGBA", (PyCFunction)KX_VertexProxy::sPyGetRGBA,METH_VARARGS},
{"setRGBA", (PyCFunction)KX_VertexProxy::sPySetRGBA,METH_VARARGS},
{"getNormal", (PyCFunction)KX_VertexProxy::sPyGetNormal,METH_VARARGS},
{"setNormal", (PyCFunction)KX_VertexProxy::sPySetNormal,METH_VARARGS},
  {NULL,NULL} //Sentinel
};

PyObject*
KX_VertexProxy::_getattr(const STR_String& attr)
{
  if (attr == "XYZ")
  	return PyObjectFrom(MT_Vector3(m_vertex->getXYZ()));

  if (attr == "UV")
  	return PyObjectFrom(MT_Point2(m_vertex->getUV1()));

  if (attr == "colour" || attr == "color")
  {
  	const unsigned char *colp = m_vertex->getRGBA();
	MT_Vector4 color(colp[0], colp[1], colp[2], colp[3]);
	color /= 255.0;
  	return PyObjectFrom(color);
  }
  
  if (attr == "normal")
  {
	return PyObjectFrom(MT_Vector3(m_vertex->getNormal()));
  }

  // pos
  if (attr == "x")
  	return PyFloat_FromDouble(m_vertex->getXYZ()[0]);
  if (attr == "y")
  	return PyFloat_FromDouble(m_vertex->getXYZ()[1]);
  if (attr == "z")
  	return PyFloat_FromDouble(m_vertex->getXYZ()[2]);

  // Col
  if (attr == "r")
  	return PyFloat_FromDouble(m_vertex->getRGBA()[0]/255.0);
  if (attr == "g")
  	return PyFloat_FromDouble(m_vertex->getRGBA()[1]/255.0);
  if (attr == "b")
  	return PyFloat_FromDouble(m_vertex->getRGBA()[2]/255.0);
  if (attr == "a")
  	return PyFloat_FromDouble(m_vertex->getRGBA()[3]/255.0);

  // UV
  if (attr == "u")
  	return PyFloat_FromDouble(m_vertex->getUV1()[0]);
  if (attr == "v")
  	return PyFloat_FromDouble(m_vertex->getUV1()[1]);

  _getattr_up(SCA_IObject);
}

int    KX_VertexProxy::_setattr(const STR_String& attr, PyObject *pyvalue)
{
  if (PySequence_Check(pyvalue))
  {
  	if (attr == "XYZ")
	{
		MT_Point3 vec;
		if (PyVecTo(pyvalue, vec))
		{
			m_vertex->SetXYZ(vec);
			m_mesh->SetMeshModified(true);
			return 0;
		}
		return 1;
	}
	
	if (attr == "UV")
	{
		MT_Point2 vec;
		if (PyVecTo(pyvalue, vec))
		{
			m_vertex->SetUV(vec);
			m_mesh->SetMeshModified(true);
			return 0;
		}
		return 1;
	}
	
	if (attr == "colour" || attr == "color")
	{
		MT_Vector4 vec;
		if (PyVecTo(pyvalue, vec))
		{
			m_vertex->SetRGBA(vec);
			m_mesh->SetMeshModified(true);
			return 0;
		}
		return 1;
	}
	
	if (attr == "normal")
	{
		MT_Vector3 vec;
		if (PyVecTo(pyvalue, vec))
		{
			m_vertex->SetNormal(vec);
			m_mesh->SetMeshModified(true);
			return 0;
		}
		return 1;
	}
  }
  
  if (PyFloat_Check(pyvalue))
  {
  	float val = PyFloat_AsDouble(pyvalue);
  	// pos
	MT_Point3 pos(m_vertex->getXYZ());
  	if (attr == "x")
	{
		pos.x() = val;
		m_vertex->SetXYZ(pos);
		m_mesh->SetMeshModified(true);
		return 0;
	}
	
  	if (attr == "y")
	{
		pos.y() = val;
		m_vertex->SetXYZ(pos);
		m_mesh->SetMeshModified(true);
		return 0;
	}
	
	if (attr == "z")
	{
		pos.z() = val;
		m_vertex->SetXYZ(pos);
		m_mesh->SetMeshModified(true);
		return 0;
	}
	
	// uv
	MT_Point2 uv = m_vertex->getUV1();
	if (attr == "u")
	{
		uv[0] = val;
		m_vertex->SetUV(uv);
		m_mesh->SetMeshModified(true);
		return 0;
	}

	if (attr == "v")
	{
		uv[1] = val;
		m_vertex->SetUV(uv);
		m_mesh->SetMeshModified(true);
		return 0;
	}

	// uv
	MT_Point2 uv2 = m_vertex->getUV2();
	if (attr == "u2")
	{
		uv[0] = val;
		m_vertex->SetUV2(uv);
		m_mesh->SetMeshModified(true);
		return 0;
	}

	if (attr == "v2")
	{
		uv[1] = val;
		m_vertex->SetUV2(uv);
		m_mesh->SetMeshModified(true);
		return 0;
	}
	
	// col
	unsigned int icol = *((const unsigned int *)m_vertex->getRGBA());
	unsigned char *cp = (unsigned char*) &icol;
	val *= 255.0;
	if (attr == "r")
	{
		cp[0] = (unsigned char) val;
		m_vertex->SetRGBA(icol);
		m_mesh->SetMeshModified(true);
		return 0;
	}
	if (attr == "g")
	{
		cp[1] = (unsigned char) val;
		m_vertex->SetRGBA(icol);
		m_mesh->SetMeshModified(true);
		return 0;
	}
	if (attr == "b")
	{
		cp[2] = (unsigned char) val;
		m_vertex->SetRGBA(icol);
		m_mesh->SetMeshModified(true);
		return 0;
	}
	if (attr == "a")
	{
		cp[3] = (unsigned char) val;
		m_vertex->SetRGBA(icol);
		m_mesh->SetMeshModified(true);
		return 0;
	}
  }
  
  return SCA_IObject::_setattr(attr, pyvalue);
}

KX_VertexProxy::KX_VertexProxy(KX_MeshProxy*mesh, RAS_TexVert* vertex)
:	m_vertex(vertex),
	m_mesh(mesh)
{
}

KX_VertexProxy::~KX_VertexProxy()
{
}



// stuff for cvalue related things
CValue*		KX_VertexProxy::Calc(VALUE_OPERATOR, CValue *) { return NULL;}
CValue*		KX_VertexProxy::CalcFinal(VALUE_DATA_TYPE, VALUE_OPERATOR, CValue *) { return NULL;}	
STR_String	sVertexName="vertex";
const STR_String &	KX_VertexProxy::GetText() {return sVertexName;};
float		KX_VertexProxy::GetNumber() { return -1;}
STR_String	KX_VertexProxy::GetName() { return sVertexName;}
void		KX_VertexProxy::SetName(STR_String) { };
CValue*		KX_VertexProxy::GetReplica() { return NULL;}
void		KX_VertexProxy::ReplicaSetName(STR_String) {};


// stuff for python integration
	
PyObject* KX_VertexProxy::PyGetXYZ(PyObject*, 
			       PyObject*, 
			       PyObject*)
{
	return PyObjectFrom(MT_Point3(m_vertex->getXYZ()));
}

PyObject* KX_VertexProxy::PySetXYZ(PyObject*, 
			       PyObject* args, 
			       PyObject*)
{
	MT_Point3 vec;
	if (PyVecArgTo(args, vec))
	{
		m_vertex->SetXYZ(vec);
		m_mesh->SetMeshModified(true);
		Py_Return;
	}
	
	return NULL;
}

PyObject* KX_VertexProxy::PyGetNormal(PyObject*, 
			       PyObject*, 
			       PyObject*)
{
	return PyObjectFrom(MT_Vector3(m_vertex->getNormal()));
}

PyObject* KX_VertexProxy::PySetNormal(PyObject*, 
			       PyObject* args, 
			       PyObject*)
{
	MT_Vector3 vec;
	if (PyVecArgTo(args, vec))
	{
		m_vertex->SetNormal(vec);
		m_mesh->SetMeshModified(true);
		Py_Return;
	}
	
	return NULL;
}


PyObject* KX_VertexProxy::PyGetRGBA(PyObject*,
			       PyObject*, 
			       PyObject*)
{
	int *rgba = (int *) m_vertex->getRGBA();
	return PyInt_FromLong(*rgba);
}

PyObject* KX_VertexProxy::PySetRGBA(PyObject*, 
			       PyObject* args, 
			       PyObject*)
{
	float r, g, b, a;
	if (PyArg_ParseTuple(args, "(ffff)", &r, &g, &b, &a))
	{
		m_vertex->SetRGBA(MT_Vector4(r, g, b, a));
		m_mesh->SetMeshModified(true);
		Py_Return;
	}
	PyErr_Clear();
	
	int rgba;
	if (PyArg_ParseTuple(args,"i",&rgba))
	{
		m_vertex->SetRGBA(rgba);
		m_mesh->SetMeshModified(true);
		Py_Return;
	}
	
	return NULL;
}


PyObject* KX_VertexProxy::PyGetUV(PyObject*, 
			       PyObject*, 
			       PyObject*)
{
	return PyObjectFrom(MT_Vector2(m_vertex->getUV1()));
}

PyObject* KX_VertexProxy::PySetUV(PyObject*, 
			       PyObject* args, 
			       PyObject*)
{
	MT_Point2 vec;
	if (PyVecArgTo(args, vec))
	{
		m_vertex->SetUV(vec);
		m_mesh->SetMeshModified(true);
		Py_Return;
	}
	
	return NULL;
}

PyObject* KX_VertexProxy::PyGetUV2(PyObject*, 
			       PyObject*, 
			       PyObject*)
{
	return PyObjectFrom(MT_Vector2(m_vertex->getUV2()));
}

PyObject* KX_VertexProxy::PySetUV2(PyObject*, 
			       PyObject* args, 
			       PyObject*)
{
	MT_Point2 vec;
	unsigned int unit=0;
	PyObject* list=0;
	if(PyArg_ParseTuple(args, "Oi", &list, &unit))
	{
		if (PyVecTo(list, vec))
		{
			m_vertex->SetFlag((m_vertex->getFlag()|RAS_TexVert::SECOND_UV));
			m_vertex->SetUnit(unit);
			m_vertex->SetUV2(vec);
			m_mesh->SetMeshModified(true);
			Py_Return;
		}
	}
	return NULL;
}
