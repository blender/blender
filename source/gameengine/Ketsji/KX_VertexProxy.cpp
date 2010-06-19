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

#ifndef DISABLE_PYTHON

#include "KX_VertexProxy.h"
#include "KX_MeshProxy.h"
#include "RAS_TexVert.h"

#include "KX_PyMath.h"

PyTypeObject KX_VertexProxy::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_VertexProxy",
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
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_VertexProxy::Methods[] = {
{"getXYZ", (PyCFunction)KX_VertexProxy::sPyGetXYZ,METH_NOARGS},
{"setXYZ", (PyCFunction)KX_VertexProxy::sPySetXYZ,METH_O},
{"getUV", (PyCFunction)KX_VertexProxy::sPyGetUV,METH_NOARGS},
{"setUV", (PyCFunction)KX_VertexProxy::sPySetUV,METH_O},

{"getUV2", (PyCFunction)KX_VertexProxy::sPyGetUV2,METH_NOARGS},
{"setUV2", (PyCFunction)KX_VertexProxy::sPySetUV2,METH_VARARGS},

{"getRGBA", (PyCFunction)KX_VertexProxy::sPyGetRGBA,METH_NOARGS},
{"setRGBA", (PyCFunction)KX_VertexProxy::sPySetRGBA,METH_O},
{"getNormal", (PyCFunction)KX_VertexProxy::sPyGetNormal,METH_NOARGS},
{"setNormal", (PyCFunction)KX_VertexProxy::sPySetNormal,METH_O},
  {NULL,NULL} //Sentinel
};

PyAttributeDef KX_VertexProxy::Attributes[] = {
	//KX_PYATTRIBUTE_TODO("DummyProps"),

	KX_PYATTRIBUTE_DUMMY("x"),
	KX_PYATTRIBUTE_DUMMY("y"),
	KX_PYATTRIBUTE_DUMMY("z"),

	KX_PYATTRIBUTE_DUMMY("r"),
	KX_PYATTRIBUTE_DUMMY("g"),
	KX_PYATTRIBUTE_DUMMY("b"),
	KX_PYATTRIBUTE_DUMMY("a"),

	KX_PYATTRIBUTE_DUMMY("u"),
	KX_PYATTRIBUTE_DUMMY("v"),

	KX_PYATTRIBUTE_DUMMY("u2"),
	KX_PYATTRIBUTE_DUMMY("v2"),

	KX_PYATTRIBUTE_DUMMY("XYZ"),
	KX_PYATTRIBUTE_DUMMY("UV"),

	KX_PYATTRIBUTE_DUMMY("color"),
	KX_PYATTRIBUTE_DUMMY("colour"),

	KX_PYATTRIBUTE_DUMMY("normal"),

	{ NULL }	//Sentinel
};

#if 0
PyObject*
KX_VertexProxy::py_getattro(PyObject *attr)
{
  char *attr_str= _PyUnicode_AsString(attr);
  if (attr_str[1]=='\0') { // Group single letters
    // pos
    if (attr_str[0]=='x')
    	return PyFloat_FromDouble(m_vertex->getXYZ()[0]);
    if (attr_str[0]=='y')
    	return PyFloat_FromDouble(m_vertex->getXYZ()[1]);
    if (attr_str[0]=='z')
    	return PyFloat_FromDouble(m_vertex->getXYZ()[2]);

    // Col
    if (attr_str[0]=='r')
    	return PyFloat_FromDouble(m_vertex->getRGBA()[0]/255.0);
    if (attr_str[0]=='g')
    	return PyFloat_FromDouble(m_vertex->getRGBA()[1]/255.0);
    if (attr_str[0]=='b')
    	return PyFloat_FromDouble(m_vertex->getRGBA()[2]/255.0);
    if (attr_str[0]=='a')
    	return PyFloat_FromDouble(m_vertex->getRGBA()[3]/255.0);

    // UV
    if (attr_str[0]=='u')
    	return PyFloat_FromDouble(m_vertex->getUV1()[0]);
    if (attr_str[0]=='v')
    	return PyFloat_FromDouble(m_vertex->getUV1()[1]);
  }


  if (!strcmp(attr_str, "XYZ"))
  	return PyObjectFrom(MT_Vector3(m_vertex->getXYZ()));

  if (!strcmp(attr_str, "UV"))
  	return PyObjectFrom(MT_Point2(m_vertex->getUV1()));

  if (!strcmp(attr_str, "color") || !strcmp(attr_str, "colour"))
  {
  	const unsigned char *colp = m_vertex->getRGBA();
	MT_Vector4 color(colp[0], colp[1], colp[2], colp[3]);
	color /= 255.0;
  	return PyObjectFrom(color);
  }

  if (!strcmp(attr_str, "normal"))
  {
	return PyObjectFrom(MT_Vector3(m_vertex->getNormal()));
  }

  py_getattro_up(CValue);
}
#endif


#if 0
int    KX_VertexProxy::py_setattro(PyObject *attr, PyObject *pyvalue)
{
  char *attr_str= _PyUnicode_AsString(attr);
  if (PySequence_Check(pyvalue))
  {
	if (!strcmp(attr_str, "XYZ"))
	{
		MT_Point3 vec;
		if (PyVecTo(pyvalue, vec))
		{
			m_vertex->SetXYZ(vec);
			m_mesh->SetMeshModified(true);
			return PY_SET_ATTR_SUCCESS;
		}
		return PY_SET_ATTR_FAIL;
	}

	if (!strcmp(attr_str, "UV"))
	{
		MT_Point2 vec;
		if (PyVecTo(pyvalue, vec))
		{
			m_vertex->SetUV(vec);
			m_mesh->SetMeshModified(true);
			return PY_SET_ATTR_SUCCESS;
		}
		return PY_SET_ATTR_FAIL;
	}

	if (!strcmp(attr_str, "color") || !strcmp(attr_str, "colour"))
	{
		MT_Vector4 vec;
		if (PyVecTo(pyvalue, vec))
		{
			m_vertex->SetRGBA(vec);
			m_mesh->SetMeshModified(true);
			return PY_SET_ATTR_SUCCESS;
		}
		return PY_SET_ATTR_FAIL;
	}

	if (!strcmp(attr_str, "normal"))
	{
		MT_Vector3 vec;
		if (PyVecTo(pyvalue, vec))
		{
			m_vertex->SetNormal(vec);
			m_mesh->SetMeshModified(true);
			return PY_SET_ATTR_SUCCESS;
		}
		return PY_SET_ATTR_FAIL;
	}
  }

  if (PyFloat_Check(pyvalue))
  {
  	float val = PyFloat_AsDouble(pyvalue);
  	// pos
	MT_Point3 pos(m_vertex->getXYZ());
  	if (!strcmp(attr_str, "x"))
	{
		pos.x() = val;
		m_vertex->SetXYZ(pos);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}

  	if (!strcmp(attr_str, "y"))
	{
		pos.y() = val;
		m_vertex->SetXYZ(pos);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}

	if (!strcmp(attr_str, "z"))
	{
		pos.z() = val;
		m_vertex->SetXYZ(pos);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}

	// uv
	MT_Point2 uv = m_vertex->getUV1();
	if (!strcmp(attr_str, "u"))
	{
		uv[0] = val;
		m_vertex->SetUV(uv);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}

	if (!strcmp(attr_str, "v"))
	{
		uv[1] = val;
		m_vertex->SetUV(uv);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}

	// uv
	MT_Point2 uv2 = m_vertex->getUV2();
	if (!strcmp(attr_str, "u2"))
	{
		uv[0] = val;
		m_vertex->SetUV2(uv);
		m_mesh->SetMeshModified(true);
		return 0;
	}

	if (!strcmp(attr_str, "v2"))
	{
		uv[1] = val;
		m_vertex->SetUV2(uv);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}

	// col
	unsigned int icol = *((const unsigned int *)m_vertex->getRGBA());
	unsigned char *cp = (unsigned char*) &icol;
	val *= 255.0;
	if (!strcmp(attr_str, "r"))
	{
		cp[0] = (unsigned char) val;
		m_vertex->SetRGBA(icol);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	if (!strcmp(attr_str, "g"))
	{
		cp[1] = (unsigned char) val;
		m_vertex->SetRGBA(icol);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	if (!strcmp(attr_str, "b"))
	{
		cp[2] = (unsigned char) val;
		m_vertex->SetRGBA(icol);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	if (!strcmp(attr_str, "a"))
	{
		cp[3] = (unsigned char) val;
		m_vertex->SetRGBA(icol);
		m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
  }

  return CValue::py_setattro(attr, pyvalue);
}
#endif

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
double		KX_VertexProxy::GetNumber() { return -1;}
STR_String&	KX_VertexProxy::GetName() { return sVertexName;}
void		KX_VertexProxy::SetName(const char *) { };
CValue*		KX_VertexProxy::GetReplica() { return NULL;}

// stuff for python integration

PyObject* KX_VertexProxy::PyGetXYZ()
{
	return PyObjectFrom(MT_Point3(m_vertex->getXYZ()));
}

PyObject* KX_VertexProxy::PySetXYZ(PyObject* value)
{
	MT_Point3 vec;
	if (!PyVecTo(value, vec))
		return NULL;

	m_vertex->SetXYZ(vec);
	m_mesh->SetMeshModified(true);
	Py_RETURN_NONE;
}

PyObject* KX_VertexProxy::PyGetNormal()
{
	return PyObjectFrom(MT_Vector3(m_vertex->getNormal()));
}

PyObject* KX_VertexProxy::PySetNormal(PyObject* value)
{
	MT_Vector3 vec;
	if (!PyVecTo(value, vec))
		return NULL;

	m_vertex->SetNormal(vec);
	m_mesh->SetMeshModified(true);
	Py_RETURN_NONE;
}


PyObject* KX_VertexProxy::PyGetRGBA()
{
	int *rgba = (int *) m_vertex->getRGBA();
	return PyLong_FromSsize_t(*rgba);
}

PyObject* KX_VertexProxy::PySetRGBA(PyObject* value)
{
	if PyLong_Check(value) {
		int rgba = PyLong_AsSsize_t(value);
		m_vertex->SetRGBA(rgba);
		m_mesh->SetMeshModified(true);
		Py_RETURN_NONE;
	}
	else {
		MT_Vector4 vec;
		if (PyVecTo(value, vec))
		{
			m_vertex->SetRGBA(vec);
			m_mesh->SetMeshModified(true);
			Py_RETURN_NONE;
		}
	}

	PyErr_SetString(PyExc_TypeError, "vert.setRGBA(value): KX_VertexProxy, expected a 4D vector or an int");
	return NULL;
}


PyObject* KX_VertexProxy::PyGetUV()
{
	return PyObjectFrom(MT_Vector2(m_vertex->getUV1()));
}

PyObject* KX_VertexProxy::PySetUV(PyObject* value)
{
	MT_Point2 vec;
	if (!PyVecTo(value, vec))
		return NULL;

	m_vertex->SetUV(vec);
	m_mesh->SetMeshModified(true);
	Py_RETURN_NONE;
}

PyObject* KX_VertexProxy::PyGetUV2()
{
	return PyObjectFrom(MT_Vector2(m_vertex->getUV2()));
}

PyObject* KX_VertexProxy::PySetUV2(PyObject* args)
{
	MT_Point2 vec;
	unsigned int unit= RAS_TexVert::SECOND_UV;

	PyObject* list= NULL;
	if(!PyArg_ParseTuple(args, "O|i:setUV2", &list, &unit))
		return NULL;

	if (!PyVecTo(list, vec))
		return NULL;

	m_vertex->SetFlag((m_vertex->getFlag()|RAS_TexVert::SECOND_UV));
	m_vertex->SetUnit(unit);
	m_vertex->SetUV2(vec);
	m_mesh->SetMeshModified(true);
	Py_RETURN_NONE;
}

#endif // DISABLE_PYTHON
