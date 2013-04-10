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

/** \file gameengine/Ketsji/KX_VertexProxy.cpp
 *  \ingroup ketsji
 */


#ifdef WITH_PYTHON

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
	{"getUV", (PyCFunction)KX_VertexProxy::sPyGetUV1, METH_NOARGS},
	{"setUV", (PyCFunction)KX_VertexProxy::sPySetUV1, METH_O},

	{"getUV2", (PyCFunction)KX_VertexProxy::sPyGetUV2,METH_NOARGS},
	{"setUV2", (PyCFunction)KX_VertexProxy::sPySetUV2,METH_VARARGS},

	{"getRGBA", (PyCFunction)KX_VertexProxy::sPyGetRGBA,METH_NOARGS},
	{"setRGBA", (PyCFunction)KX_VertexProxy::sPySetRGBA,METH_O},
	{"getNormal", (PyCFunction)KX_VertexProxy::sPyGetNormal,METH_NOARGS},
	{"setNormal", (PyCFunction)KX_VertexProxy::sPySetNormal,METH_O},
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_VertexProxy::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("x", KX_VertexProxy, pyattr_get_x, pyattr_set_x),
	KX_PYATTRIBUTE_RW_FUNCTION("y", KX_VertexProxy, pyattr_get_y, pyattr_set_y),
	KX_PYATTRIBUTE_RW_FUNCTION("z", KX_VertexProxy, pyattr_get_z, pyattr_set_z),

	KX_PYATTRIBUTE_RW_FUNCTION("r", KX_VertexProxy, pyattr_get_r, pyattr_set_r),
	KX_PYATTRIBUTE_RW_FUNCTION("g", KX_VertexProxy, pyattr_get_g, pyattr_set_g),
	KX_PYATTRIBUTE_RW_FUNCTION("b", KX_VertexProxy, pyattr_get_b, pyattr_set_b),
	KX_PYATTRIBUTE_RW_FUNCTION("a", KX_VertexProxy, pyattr_get_a, pyattr_set_a),

	KX_PYATTRIBUTE_RW_FUNCTION("u", KX_VertexProxy, pyattr_get_u, pyattr_set_u),
	KX_PYATTRIBUTE_RW_FUNCTION("v", KX_VertexProxy, pyattr_get_v, pyattr_set_v),

	KX_PYATTRIBUTE_RW_FUNCTION("u2", KX_VertexProxy, pyattr_get_u2, pyattr_set_u2),
	KX_PYATTRIBUTE_RW_FUNCTION("v2", KX_VertexProxy, pyattr_get_v2, pyattr_set_v2),

	KX_PYATTRIBUTE_RW_FUNCTION("XYZ", KX_VertexProxy, pyattr_get_XYZ, pyattr_set_XYZ),
	KX_PYATTRIBUTE_RW_FUNCTION("UV", KX_VertexProxy, pyattr_get_UV, pyattr_set_UV),
	KX_PYATTRIBUTE_RW_FUNCTION("uvs", KX_VertexProxy, pyattr_get_uvs, pyattr_set_uvs),

	KX_PYATTRIBUTE_RW_FUNCTION("color", KX_VertexProxy, pyattr_get_color, pyattr_set_color),
	KX_PYATTRIBUTE_RW_FUNCTION("normal", KX_VertexProxy, pyattr_get_normal, pyattr_set_normal),

	{ NULL }	//Sentinel
};

PyObject *KX_VertexProxy::pyattr_get_x(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getXYZ()[0]);
}

PyObject *KX_VertexProxy::pyattr_get_y(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getXYZ()[1]);
}

PyObject *KX_VertexProxy::pyattr_get_z(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getXYZ()[2]);
}

PyObject *KX_VertexProxy::pyattr_get_r(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getRGBA()[0]/255.0);
}

PyObject *KX_VertexProxy::pyattr_get_g(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getRGBA()[1]/255.0);
}

PyObject *KX_VertexProxy::pyattr_get_b(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getRGBA()[2]/255.0);
}

PyObject *KX_VertexProxy::pyattr_get_a(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getRGBA()[3]/255.0);
}

PyObject *KX_VertexProxy::pyattr_get_u(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getUV(0)[0]);
}

PyObject *KX_VertexProxy::pyattr_get_v(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getUV(0)[1]);
}

PyObject *KX_VertexProxy::pyattr_get_u2(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getUV(1)[0]);
}

PyObject *KX_VertexProxy::pyattr_get_v2(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyFloat_FromDouble(self->m_vertex->getUV(1)[1]);
}

PyObject *KX_VertexProxy::pyattr_get_XYZ(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyObjectFrom(MT_Vector3(self->m_vertex->getXYZ()));
}

PyObject *KX_VertexProxy::pyattr_get_UV(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyObjectFrom(MT_Point2(self->m_vertex->getUV(0)));
}

PyObject *KX_VertexProxy::pyattr_get_uvs(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self= static_cast<KX_VertexProxy*>(self_v);
	
	PyObject* uvlist = PyList_New(RAS_TexVert::MAX_UNIT);
	for (int i=0; i<RAS_TexVert::MAX_UNIT; ++i)
	{
		PyList_SET_ITEM(uvlist, i, PyObjectFrom(MT_Point2(self->m_vertex->getUV(i))));
	}

	return uvlist;
}

PyObject *KX_VertexProxy::pyattr_get_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	const unsigned char *colp = self->m_vertex->getRGBA();
	MT_Vector4 color(colp[0], colp[1], colp[2], colp[3]);
	color /= 255.0;
	return PyObjectFrom(color);
}

PyObject *KX_VertexProxy::pyattr_get_normal(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	return PyObjectFrom(MT_Vector3(self->m_vertex->getNormal()));
}

int KX_VertexProxy::pyattr_set_x(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		MT_Point3 pos(self->m_vertex->getXYZ());
		pos.x() = val;
		self->m_vertex->SetXYZ(pos);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_y(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		MT_Point3 pos(self->m_vertex->getXYZ());
		pos.y() = val;
		self->m_vertex->SetXYZ(pos);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_z(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		MT_Point3 pos(self->m_vertex->getXYZ());
		pos.z() = val;
		self->m_vertex->SetXYZ(pos);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_u(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		MT_Point2 uv = self->m_vertex->getUV(0);
		uv[0] = val;
		self->m_vertex->SetUV(0, uv);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_v(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		MT_Point2 uv = self->m_vertex->getUV(0);
		uv[1] = val;
		self->m_vertex->SetUV(0, uv);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_u2(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		MT_Point2 uv = self->m_vertex->getUV(1);
		uv[0] = val;
		self->m_vertex->SetUV(1, uv);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_v2(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		MT_Point2 uv = self->m_vertex->getUV(1);
		uv[1] = val;
		self->m_vertex->SetUV(1, uv);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_r(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		unsigned int icol = *((const unsigned int *)self->m_vertex->getRGBA());
		unsigned char *cp = (unsigned char*) &icol;
		val *= 255.0;
		cp[0] = (unsigned char) val;
		self->m_vertex->SetRGBA(icol);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_g(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		unsigned int icol = *((const unsigned int *)self->m_vertex->getRGBA());
		unsigned char *cp = (unsigned char*) &icol;
		val *= 255.0;
		cp[1] = (unsigned char) val;
		self->m_vertex->SetRGBA(icol);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_b(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		unsigned int icol = *((const unsigned int *)self->m_vertex->getRGBA());
		unsigned char *cp = (unsigned char*) &icol;
		val *= 255.0;
		cp[2] = (unsigned char) val;
		self->m_vertex->SetRGBA(icol);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_a(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PyFloat_Check(value))
	{
		float val = PyFloat_AsDouble(value);
		unsigned int icol = *((const unsigned int *)self->m_vertex->getRGBA());
		unsigned char *cp = (unsigned char*) &icol;
		val *= 255.0;
		cp[3] = (unsigned char) val;
		self->m_vertex->SetRGBA(icol);
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_XYZ(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PySequence_Check(value))
	{
		MT_Point3 vec;
		if (PyVecTo(value, vec))
		{
			self->m_vertex->SetXYZ(vec);
			self->m_mesh->SetMeshModified(true);
			return PY_SET_ATTR_SUCCESS;
		}
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_UV(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PySequence_Check(value))
	{
		MT_Point2 vec;
		if (PyVecTo(value, vec)) {
			self->m_vertex->SetUV(0, vec);
			self->m_mesh->SetMeshModified(true);
			return PY_SET_ATTR_SUCCESS;
		}
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_uvs(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self= static_cast<KX_VertexProxy*>(self_v);
	if (PySequence_Check(value))
	{
		MT_Point2 vec;
		for (int i=0; i<PySequence_Size(value) && i<RAS_TexVert::MAX_UNIT; ++i)
		{
			if (PyVecTo(PySequence_GetItem(value, i), vec))
			{
				self->m_vertex->SetUV(i, vec);
				self->m_mesh->SetMeshModified(true);
			}
			else
			{
				PyErr_SetString(PyExc_AttributeError, STR_String().Format("list[%d] was not a vector", i).ReadPtr());
				return PY_SET_ATTR_FAIL;
			}
		}
		
		self->m_mesh->SetMeshModified(true);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_color(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PySequence_Check(value))
	{
		MT_Vector4 vec;
		if (PyVecTo(value, vec))
		{
			self->m_vertex->SetRGBA(vec);
			self->m_mesh->SetMeshModified(true);
			return PY_SET_ATTR_SUCCESS;
		}
	}
	return PY_SET_ATTR_FAIL;
}

int KX_VertexProxy::pyattr_set_normal(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_VertexProxy* self = static_cast<KX_VertexProxy*>(self_v);
	if (PySequence_Check(value))
	{
		MT_Vector3 vec;
		if (PyVecTo(value, vec))
		{
			self->m_vertex->SetNormal(vec);
			self->m_mesh->SetMeshModified(true);
			return PY_SET_ATTR_SUCCESS;
		}
	}
	return PY_SET_ATTR_FAIL;
}

KX_VertexProxy::KX_VertexProxy(KX_MeshProxy*mesh, RAS_TexVert* vertex)
:	m_vertex(vertex),
	m_mesh(mesh)
{
	/* see bug [#27071] */
	Py_INCREF(m_mesh->GetProxy());
}

KX_VertexProxy::~KX_VertexProxy()
{
	/* see bug [#27071] */
	Py_DECREF(m_mesh->GetProxy());
}



// stuff for cvalue related things
CValue*		KX_VertexProxy::Calc(VALUE_OPERATOR, CValue *) { return NULL;}
CValue*		KX_VertexProxy::CalcFinal(VALUE_DATA_TYPE, VALUE_OPERATOR, CValue *) { return NULL;}
static STR_String sVertexName = "vertex";
const STR_String &	KX_VertexProxy::GetText() {return sVertexName;};
double		KX_VertexProxy::GetNumber() { return -1;}
STR_String&	KX_VertexProxy::GetName() { return sVertexName;}
void		KX_VertexProxy::SetName(const char *) { };
CValue*		KX_VertexProxy::GetReplica() { return NULL;}

// stuff for python integration

PyObject *KX_VertexProxy::PyGetXYZ()
{
	return PyObjectFrom(MT_Point3(m_vertex->getXYZ()));
}

PyObject *KX_VertexProxy::PySetXYZ(PyObject *value)
{
	MT_Point3 vec;
	if (!PyVecTo(value, vec))
		return NULL;

	m_vertex->SetXYZ(vec);
	m_mesh->SetMeshModified(true);
	Py_RETURN_NONE;
}

PyObject *KX_VertexProxy::PyGetNormal()
{
	return PyObjectFrom(MT_Vector3(m_vertex->getNormal()));
}

PyObject *KX_VertexProxy::PySetNormal(PyObject *value)
{
	MT_Vector3 vec;
	if (!PyVecTo(value, vec))
		return NULL;

	m_vertex->SetNormal(vec);
	m_mesh->SetMeshModified(true);
	Py_RETURN_NONE;
}


PyObject *KX_VertexProxy::PyGetRGBA()
{
	int *rgba = (int *) m_vertex->getRGBA();
	return PyLong_FromLong(*rgba);
}

PyObject *KX_VertexProxy::PySetRGBA(PyObject *value)
{
	if (PyLong_Check(value)) {
		int rgba = PyLong_AsLong(value);
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


PyObject *KX_VertexProxy::PyGetUV1()
{
	return PyObjectFrom(MT_Vector2(m_vertex->getUV(0)));
}

PyObject *KX_VertexProxy::PySetUV1(PyObject *value)
{
	MT_Point2 vec;
	if (!PyVecTo(value, vec))
		return NULL;

	m_vertex->SetUV(0, vec);
	m_mesh->SetMeshModified(true);
	Py_RETURN_NONE;
}

PyObject *KX_VertexProxy::PyGetUV2()
{
	return PyObjectFrom(MT_Vector2(m_vertex->getUV(1)));
}

PyObject *KX_VertexProxy::PySetUV2(PyObject *args)
{
	MT_Point2 vec;
	if (!PyVecTo(args, vec))
		return NULL;

	m_vertex->SetUV(1, vec);
	m_mesh->SetMeshModified(true);
	Py_RETURN_NONE;
}

#endif // WITH_PYTHON
