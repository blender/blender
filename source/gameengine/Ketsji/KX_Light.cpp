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

/** \file gameengine/Ketsji/KX_Light.cpp
 *  \ingroup ketsji
 */

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#include <stdio.h>

#include "KX_Light.h"
#include "KX_Camera.h"
#include "RAS_IRasterizer.h"
#include "RAS_ICanvas.h"
#include "RAS_ILightObject.h"

#include "KX_PyMath.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_lamp_types.h"

#include "BKE_scene.h"
#include "MEM_guardedalloc.h"

#include "BLI_math.h"

KX_LightObject::KX_LightObject(void* sgReplicationInfo,SG_Callbacks callbacks,
                               RAS_IRasterizer* rasterizer,
                               RAS_ILightObject* lightobj,
                               bool glsl)
	: KX_GameObject(sgReplicationInfo,callbacks),
	  m_rasterizer(rasterizer)
{
	m_lightobj = lightobj;
	m_lightobj->m_scene = sgReplicationInfo;
	m_lightobj->m_light = this;
	m_rasterizer->AddLight(m_lightobj);
	m_lightobj->m_glsl = glsl;
	m_blenderscene = ((KX_Scene*)sgReplicationInfo)->GetBlenderScene();
	m_base = NULL;
};


KX_LightObject::~KX_LightObject()
{
	if (m_lightobj) {
		m_rasterizer->RemoveLight(m_lightobj);
		delete(m_lightobj);
	}

	if (m_base) {
		BKE_scene_base_unlink(m_blenderscene, m_base);
		MEM_freeN(m_base);
	}
}


CValue*		KX_LightObject::GetReplica()
{

	KX_LightObject* replica = new KX_LightObject(*this);

	replica->ProcessReplica();
	
	replica->m_lightobj = m_lightobj->Clone();
	replica->m_lightobj->m_light = replica;
	m_rasterizer->AddLight(replica->m_lightobj);
	if (m_base)
		m_base = NULL;

	return replica;
}

void KX_LightObject::UpdateScene(KX_Scene *kxscene)
{
	m_lightobj->m_scene = (void*)kxscene;
	m_blenderscene = kxscene->GetBlenderScene();
	m_base = BKE_scene_base_add(m_blenderscene, GetBlenderObject());
}

void KX_LightObject::SetLayer(int layer)
{
	m_lightobj->m_layer = layer;
}

#ifdef WITH_PYTHON
/* ------------------------------------------------------------------------- */
/* Python Integration Hooks					                                 */
/* ------------------------------------------------------------------------- */

PyTypeObject KX_LightObject::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_LightObject",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	&KX_GameObject::Sequence,
	&KX_GameObject::Mapping,
	0,0,0,
	NULL,
	NULL,
	0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&KX_GameObject::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_LightObject::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_LightObject::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("layer", KX_LightObject, pyattr_get_layer, pyattr_set_layer),
	KX_PYATTRIBUTE_RW_FUNCTION("energy", KX_LightObject, pyattr_get_energy, pyattr_set_energy),
	KX_PYATTRIBUTE_RW_FUNCTION("distance", KX_LightObject, pyattr_get_distance, pyattr_set_distance),
	KX_PYATTRIBUTE_RW_FUNCTION("color", KX_LightObject, pyattr_get_color, pyattr_set_color),
	KX_PYATTRIBUTE_RW_FUNCTION("lin_attenuation", KX_LightObject, pyattr_get_lin_attenuation, pyattr_set_lin_attenuation),
	KX_PYATTRIBUTE_RW_FUNCTION("quad_attenuation", KX_LightObject, pyattr_get_quad_attenuation, pyattr_set_quad_attenuation),
	KX_PYATTRIBUTE_RW_FUNCTION("spotsize", KX_LightObject, pyattr_get_spotsize, pyattr_set_spotsize),
	KX_PYATTRIBUTE_RW_FUNCTION("spotblend", KX_LightObject, pyattr_get_spotblend, pyattr_set_spotblend),
	KX_PYATTRIBUTE_RO_FUNCTION("SPOT", KX_LightObject, pyattr_get_typeconst),
	KX_PYATTRIBUTE_RO_FUNCTION("SUN", KX_LightObject, pyattr_get_typeconst),
	KX_PYATTRIBUTE_RO_FUNCTION("NORMAL", KX_LightObject, pyattr_get_typeconst),
	KX_PYATTRIBUTE_RW_FUNCTION("type", KX_LightObject, pyattr_get_type, pyattr_set_type),
	{ NULL }	//Sentinel
};

PyObject *KX_LightObject::pyattr_get_layer(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	return PyLong_FromLong(self->m_lightobj->m_layer);
}

int KX_LightObject::pyattr_set_layer(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);

	if (PyLong_Check(value)) {
		int val = PyLong_AsLong(value);
		if (val < 1)
			val = 1;
		else if (val > 20)
			val = 20;

		self->m_lightobj->m_layer = val;
		return PY_SET_ATTR_SUCCESS;
	}

	PyErr_Format(PyExc_TypeError, "expected an integer for attribute \"%s\"", attrdef->m_name);
	return PY_SET_ATTR_FAIL;
}

PyObject *KX_LightObject::pyattr_get_energy(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	return PyFloat_FromDouble(self->m_lightobj->m_energy);
}

int KX_LightObject::pyattr_set_energy(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);

	if (PyFloat_Check(value)) {
		float val = PyFloat_AsDouble(value);
		if (val < 0)
			val = 0;
		else if (val > 10)
			val = 10;

		self->m_lightobj->m_energy = val;
		return PY_SET_ATTR_SUCCESS;
	}

	PyErr_Format(PyExc_TypeError, "expected float value for attribute \"%s\"", attrdef->m_name);
	return PY_SET_ATTR_FAIL;
}

PyObject *KX_LightObject::pyattr_get_distance(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	return PyFloat_FromDouble(self->m_lightobj->m_distance);
}

int KX_LightObject::pyattr_set_distance(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);

	if (PyFloat_Check(value)) {
		float val = PyFloat_AsDouble(value);
		if (val < 0.01)
			val = 0.01;
		else if (val > 5000.f)
			val = 5000.f;

		self->m_lightobj->m_distance = val;
		return PY_SET_ATTR_SUCCESS;
	}

	PyErr_Format(PyExc_TypeError, "expected float value for attribute \"%s\"", attrdef->m_name);
	return PY_SET_ATTR_FAIL;
}

PyObject *KX_LightObject::pyattr_get_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	return Py_BuildValue("[fff]", self->m_lightobj->m_color[0], self->m_lightobj->m_color[1], self->m_lightobj->m_color[1]);
}

int KX_LightObject::pyattr_set_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);

	MT_Vector3 color;
	if (PyVecTo(value, color))
	{
		self->m_lightobj->m_color[0] = color[0];
		self->m_lightobj->m_color[1] = color[1];
		self->m_lightobj->m_color[2] = color[2];
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

PyObject *KX_LightObject::pyattr_get_lin_attenuation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	return PyFloat_FromDouble(self->m_lightobj->m_att1);
}

int KX_LightObject::pyattr_set_lin_attenuation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);

	if (PyFloat_Check(value)) {
		float val = PyFloat_AsDouble(value);
		if (val < 0.f)
			val = 0.f;
		else if (val > 1.f)
			val = 1.f;

		self->m_lightobj->m_att1 = val;
		return PY_SET_ATTR_SUCCESS;
	}

	PyErr_Format(PyExc_TypeError, "expected float value for attribute \"%s\"", attrdef->m_name);
	return PY_SET_ATTR_FAIL;
}

PyObject *KX_LightObject::pyattr_get_quad_attenuation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	return PyFloat_FromDouble(self->m_lightobj->m_att2);
}

int KX_LightObject::pyattr_set_quad_attenuation(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);

	if (PyFloat_Check(value)) {
		float val = PyFloat_AsDouble(value);
		if (val < 0.f)
			val = 0.f;
		else if (val > 1.f)
			val = 1.f;

		self->m_lightobj->m_att2 = val;
		return PY_SET_ATTR_SUCCESS;
	}

	PyErr_Format(PyExc_TypeError, "expected float value for attribute \"%s\"", attrdef->m_name);
	return PY_SET_ATTR_FAIL;
}

PyObject *KX_LightObject::pyattr_get_spotsize(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	return PyFloat_FromDouble(RAD2DEG(self->m_lightobj->m_spotsize));
}

int KX_LightObject::pyattr_set_spotsize(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);

	if (PyFloat_Check(value)) {
		float val = PyFloat_AsDouble(value);
		if (val < 0.f)
			val = 0.f;
		else if (val > 180.f)
			val = 180.f;

		self->m_lightobj->m_spotsize = DEG2RAD(val);
		return PY_SET_ATTR_SUCCESS;
	}

	PyErr_Format(PyExc_TypeError, "expected float value for attribute \"%s\"", attrdef->m_name);
	return PY_SET_ATTR_FAIL;
}
PyObject *KX_LightObject::pyattr_get_spotblend(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	return PyFloat_FromDouble(self->m_lightobj->m_spotblend);
}

int KX_LightObject::pyattr_set_spotblend(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);

	if (PyFloat_Check(value)) {
		float val = PyFloat_AsDouble(value);
		if (val < 0.f)
			val = 0.f;
		else if (val > 1.f)
			val = 1.f;

		self->m_lightobj->m_spotblend = val;
		return PY_SET_ATTR_SUCCESS;
	}

	PyErr_Format(PyExc_TypeError, "expected float value for attribute \"%s\"", attrdef->m_name);
	return PY_SET_ATTR_FAIL;
}

PyObject *KX_LightObject::pyattr_get_typeconst(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	PyObject *retvalue;

	const char* type = attrdef->m_name;

	if (!strcmp(type, "SPOT")) {
		retvalue = PyLong_FromLong(RAS_ILightObject::LIGHT_SPOT);
	} else if (!strcmp(type, "SUN")) {
		retvalue = PyLong_FromLong(RAS_ILightObject::LIGHT_SUN);
	} else if (!strcmp(type, "NORMAL")) {
		retvalue = PyLong_FromLong(RAS_ILightObject::LIGHT_NORMAL);
	}
	else {
		/* should never happen */
		PyErr_SetString(PyExc_TypeError, "light.type: internal error, invalid light type");
		retvalue = NULL;
	}

	return retvalue;
}

PyObject *KX_LightObject::pyattr_get_type(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	return PyLong_FromLong(self->m_lightobj->m_type);
}

int KX_LightObject::pyattr_set_type(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_LightObject* self = static_cast<KX_LightObject*>(self_v);
	const int val = PyLong_AsLong(value);
	if ((val==-1 && PyErr_Occurred()) || val<0 || val>2) {
		PyErr_SetString(PyExc_ValueError, "light.type= val: KX_LightObject, expected an int between 0 and 2");
		return PY_SET_ATTR_FAIL;
	}
	
	switch (val) {
		case 0:
			self->m_lightobj->m_type = self->m_lightobj->LIGHT_SPOT;
			break;
		case 1:
			self->m_lightobj->m_type = self->m_lightobj->LIGHT_SUN;
			break;
		case 2:
			self->m_lightobj->m_type = self->m_lightobj->LIGHT_NORMAL;
			break;
	}

	return PY_SET_ATTR_SUCCESS;
}
#endif // WITH_PYTHON
