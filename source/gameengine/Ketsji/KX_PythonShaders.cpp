/**
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
 * Init bge.shaders
 */

#ifndef DISABLE_PYTHON
#include "KX_PythonInit.h"
#include "PyObjectPlus.h"
#include "KX_PythonSeq.h"
#include "KX_PythonShaders.h"
#include "KX_BlenderMaterial.h"
#include "BL_BlenderShader.h"
#include "DNA_material_types.h"
#include "BLI_listbase.h"

/**
 * The Shader type
 */

KX_PythonShader::KX_PythonShader()
	: PyObjectPlus(),
	m_vert(""),
	m_geom(""),
	m_frag(""),
	m_mat(NULL)
{
}

KX_PythonShader::KX_PythonShader(KX_BlenderMaterial *material)
	: PyObjectPlus(),
	m_mat(material)
{
	char vert[64000]="", geom[64000]="", frag[64000]="";
	material->GetBlenderShader()->GetSources(vert, geom, frag);

	m_vert = vert;
	m_geom = geom;
	m_frag = frag;

	CustomUniform *cu = static_cast<CustomUniform*>(material->GetBlenderMaterial()->csi.uniforms.first);

	while (cu)
	{
		m_uniforms.push_back(new KX_PythonUniform(cu));
		cu = cu->next;
	}
}

KX_PythonShader::~KX_PythonShader()
{
	// XXX This probably needs more attention...
	m_uniforms.clear();
}

PyObject *KX_PythonShader::py_shader_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	KX_PythonShader* pyshader = new KX_PythonShader();
	return pyshader->GetProxy();
}

PyTypeObject KX_PythonShader::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Shader",
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
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_shader_new
};

PyMethodDef KX_PythonShader::Methods[] = {
	KX_PYMETHODTABLE_O(KX_PythonShader, addUniform),
	{NULL} //Sentinel
};

PyAttributeDef KX_PythonShader::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("vertex", KX_PythonShader, pyattr_get_source, pyattr_set_source),
	KX_PYATTRIBUTE_RW_FUNCTION("geometry", KX_PythonShader, pyattr_get_source, pyattr_set_source),
	KX_PYATTRIBUTE_RW_FUNCTION("fragment", KX_PythonShader, pyattr_get_source, pyattr_set_source),
	KX_PYATTRIBUTE_RO_FUNCTION("uniforms", KX_PythonShader, pyattr_get_uniforms),
	{NULL}	//Sentinel
};

PyObject* KX_PythonShader::pyattr_get_source(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PythonShader *self = static_cast<KX_PythonShader*>(self_v);

	if (!strcmp(attrdef->m_name, "vertex"))
		return PyUnicode_FromString(self->m_vert.Ptr());
	else if (!strcmp(attrdef->m_name, "geometry"))
		return PyUnicode_FromString(self->m_geom.Ptr());
	else if (!strcmp(attrdef->m_name, "fragment"))
		return PyUnicode_FromString(self->m_frag.Ptr());
	else
		/* Should never happen */
		PyErr_SetString(PyExc_SystemError, "invalid attribute, internal error");

	Py_RETURN_NONE;
}

int KX_PythonShader::pyattr_set_source(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_PythonShader *self = static_cast<KX_PythonShader*>(self_v);

	PyObject* bytes = PyUnicode_AsASCIIString(value);
	char* source = PyBytes_AsString(bytes);

	if (!strcmp(attrdef->m_name, "vertex"))
	{
		self->m_vert = source;

		if (self->m_mat)
			self->m_mat->GetBlenderShader()->SetSources(source, NULL, NULL);
	}
	else if (!strcmp(attrdef->m_name, "geometry"))
	{
		self->m_geom = source;

		if (self->m_mat)
			self->m_mat->GetBlenderShader()->SetSources(NULL, source, NULL);
	}
	else if (!strcmp(attrdef->m_name, "fragment"))
	{
		self->m_frag = source;

		if (self->m_mat)
			self->m_mat->GetBlenderShader()->SetSources(NULL, NULL, source);
	}
	else
	{
		/* Should never happen */
		PyErr_SetString(PyExc_SystemError, "invalid type, internal error");
		Py_DECREF(bytes);
		return PY_SET_ATTR_FAIL;
	}
	
	Py_DECREF(bytes);
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_PythonShader::pyattr_get_uniforms(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PythonShader* self = static_cast<KX_PythonShader*>(self_v);
	return KX_PythonSeq_CreatePyObject(self->m_proxy, KX_PYGENSEQ_SHADER_TYPE_UNIFORMS);

}

KX_PYMETHODDEF_DOC_O(KX_PythonShader, addUniform,
	"addUniform(uniform) -- Adds the uniform to the shader's uniform list")
{
	if (!PyType_IsSubtype(&KX_PythonUniform::Type, Py_TYPE(value)))
		return NULL;

	KX_PythonUniform *uniform = static_cast<KX_PythonUniform*>BGE_PROXY_REF(value);

	BLI_addhead(&m_mat->GetBlenderMaterial()->csi.uniforms, uniform->GetCustomUniform());
	m_uniforms.push_back(uniform);

	Py_RETURN_NONE;
}

/**
 * The Uniform type
 */

KX_PythonUniform::KX_PythonUniform(char* name, short type, int size)
	: PyObjectPlus(),
	m_name(name),
	m_type(type),
	m_size(size),
	m_cu(NULL),
	m_owns_cu(true)
{
	m_cu = (CustomUniform*)malloc(sizeof(CustomUniform));
	m_cu->next = m_cu->prev = NULL;

	strcpy(m_cu->name, name);
	m_cu->type = type;
	m_cu->size = size;
	
	m_cu->data = 0;
	
	switch (m_cu->type)
	{
	case MA_UNF_FLOAT:
		*(float*)&m_cu->data = 0.f;
		break;
	case MA_UNF_VEC2:
	case MA_UNF_VEC3:
	case MA_UNF_VEC4:
		m_cu->data = calloc(m_size, sizeof(float));
		break;
	case MA_UNF_INT:
		*(int*)&m_cu->data = 0;
		break;
	case MA_UNF_IVEC2:
	case MA_UNF_IVEC3:
	case MA_UNF_IVEC4:
		m_cu->data = calloc(m_size, sizeof(int));
	default:
		m_cu->data = NULL;
	}

	m_data = m_cu->data;

}

KX_PythonUniform::KX_PythonUniform(CustomUniform *cu)
	: PyObjectPlus(),
	m_cu(cu),
	m_owns_cu(false)
{
	m_name = cu->name;
	m_type = cu->type;
	m_size = cu->size;
	m_data = cu->data;
}


KX_PythonUniform::~KX_PythonUniform()
{
	if (m_owns_cu)
		free(m_cu);
}

PyObject *KX_PythonUniform::py_uniform_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	char *name;
	short type_const;
	int size;

	if (!PyArg_ParseTuple(args, "sh:Uniform", &name, &type_const)) {
		PyErr_SetString(PyExc_ValueError, "wrong number of arguments. Please use Uniform(name, type)");
		return NULL;
	}
	
	switch(type_const)
	{
		case MA_UNF_FLOAT:
		case MA_UNF_INT:
		case MA_UNF_SAMPLER2D:
			size = 1;
			break;
		case MA_UNF_VEC2:
		case MA_UNF_IVEC2:
			size = 2;
			break;
		case MA_UNF_VEC3:
		case MA_UNF_IVEC3:
			size = 3;
			break;
		case MA_UNF_VEC4:
		case MA_UNF_IVEC4:
			size = 4;
			break;
		default:
			PyErr_SetString(PyExc_ValueError, "the supplied type is unsupported");
			return NULL;
	}

	KX_PythonUniform* pyuniform = new KX_PythonUniform(name, type_const, size);
	return pyuniform->GetProxy();
}

PyTypeObject KX_PythonUniform::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Uniform",
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
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_uniform_new
};

PyMethodDef KX_PythonUniform::Methods[] = {
	{NULL} //Sentinel
};

PyAttributeDef KX_PythonUniform::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RO("name", KX_PythonUniform, m_name),
	KX_PYATTRIBUTE_SHORT_RO("type", KX_PythonUniform, m_type),
	KX_PYATTRIBUTE_RW_FUNCTION("value", KX_PythonUniform, pyattr_get_value, pyattr_set_value),
	{NULL}	//Sentinel
};

PyObject *KX_PythonUniform::pyattr_get_value(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PythonUniform *self = static_cast<KX_PythonUniform*>(self_v);

	if (!self->m_data)
		Py_RETURN_NONE;

	switch(self->m_type)
	{
		case MA_UNF_FLOAT:
		{
			float val = *(float*)(&self->m_data);
			return PyFloat_FromDouble(val);
		}
		case MA_UNF_VEC2:
		case MA_UNF_VEC3:
		case MA_UNF_VEC4:
		{
			float* val = static_cast<float*>(self->m_data);
			PyObject* ret = PyList_New(self->m_size);

			for (int i=0; i<self->m_size; ++i)
			{
				PyList_SetItem(ret, i, PyFloat_FromDouble(val[i]));
			}

			return ret;

		}
		case MA_UNF_INT:
		{
			int val = *(int*)&self->m_data;
			return PyLong_FromLong(val);
		}
		case MA_UNF_IVEC2:
		case MA_UNF_IVEC3:
		case MA_UNF_IVEC4:
		{
			int* val = static_cast<int*>(self->m_data);
			PyObject* ret = PyList_New(self->m_size);

			for (int i=0; i<self->m_size; ++i)
			{
				PyList_SetItem(ret, i, PyLong_FromLong(val[i]));
			}

			return ret;
		}
		case MA_UNF_SAMPLER2D:
			return PyLong_FromLong(((Tex*)self->m_data)->ima->bindcode);
		default:
			// Should never happen
			PyErr_SetString(PyExc_AttributeError, "invalid type for uniform, internal error");
			return NULL;
	}
}

int KX_PythonUniform::pyattr_set_value(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{	
	KX_PythonUniform *self = static_cast<KX_PythonUniform*>(self_v);

	switch(self->m_type)
	{
		case MA_UNF_FLOAT:
		{
			if (!PyFloat_Check(value))
			{
				PyErr_SetString(PyExc_ValueError, "float uniform type requires a float value");
				return PY_SET_ATTR_FAIL;
			}

			*(float*)(&self->m_data) = PyFloat_AsDouble(value);
			if (self->m_cu)
				self->m_cu->data = self->m_data;
			return PY_SET_ATTR_SUCCESS;
		}
		case MA_UNF_VEC2:
		case MA_UNF_VEC3:
		case MA_UNF_VEC4:
		{
			if (!PySequence_Check(value))
			{
				PyErr_SetString(PyExc_ValueError, "vector uniform types require a sequence of floats");
				return PY_SET_ATTR_FAIL;
			}

			if (PySequence_Size(value) != self->m_size)
			{
				PyErr_SetString(PyExc_ValueError, "not enough values in the sequence");
				return PY_SET_ATTR_FAIL;
			}

			for (int i=0; i<self->m_size; ++i)
			{
				PyObject* val = PySequence_Fast_GET_ITEM(value, i);
				if (!PyFloat_Check(val))
				{
					PyErr_SetString(PyExc_ValueError, "vector uniform types require a sequence of floats");
					return PY_SET_ATTR_FAIL;
				}
				((float*)self->m_data)[i] = PyFloat_AsDouble(val);
			}

			return PY_SET_ATTR_SUCCESS;
		}
		case MA_UNF_INT:
		{
			if (!PyLong_Check(value))
			{
				PyErr_SetString(PyExc_ValueError, "integer uniform type requires an integer value");
				return PY_SET_ATTR_FAIL;
			}

			*(int*)&self->m_data = PyLong_AsLong(value);
			if (self->m_cu)
				self->m_cu->data = self->m_data;
			return PY_SET_ATTR_SUCCESS;
		}
		case MA_UNF_IVEC2:
		case MA_UNF_IVEC3:
		case MA_UNF_IVEC4:
		{
			if (!PySequence_Check(value))
			{
				PyErr_SetString(PyExc_ValueError, "integer vector uniform types require a sequence of integers");
				return PY_SET_ATTR_FAIL;
			}

			if (PySequence_Size(value) != self->m_size)
			{
				PyErr_SetString(PyExc_ValueError, "not enough values in the sequence");
				return PY_SET_ATTR_FAIL;
			}

			for (int i=0; i<self->m_size; ++i)
			{
				PyObject* val = PySequence_Fast_GET_ITEM(value, i);
				if (!PyLong_Check(val))
				{
					PyErr_SetString(PyExc_ValueError, "integer vector uniform types require a sequence of integers");
					return PY_SET_ATTR_FAIL;
				}
				((int*)self->m_data)[i] = PyLong_AsLong(val);
			}

			return PY_SET_ATTR_SUCCESS;
		}
		case MA_UNF_SAMPLER2D:
			PyErr_SetString(PyExc_AttributeError, "Sampler2D value is read-only");
			return PY_SET_ATTR_FAIL;
		default:
			// Should never happen
			PyErr_SetString(PyExc_AttributeError, "invalid type for uniform, internal error");
			return PY_SET_ATTR_FAIL;
	}
}

#endif // ndef DISABLE_PYTHON