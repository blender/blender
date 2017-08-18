/*******************************************************************************
* Copyright 2009-2015 Juan Francisco Crespo Galán
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
******************************************************************************/

#include "PySource.h"

#include "Exception.h"
#include "fx/Source.h"

#include <memory>

extern PyObject* AUDError;

static PyObject *
Source_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	SourceP* self = (SourceP*)type->tp_alloc(type, 0);

	if(self != nullptr)
	{
		float azimuth, elevation, distance;
		if(!PyArg_ParseTuple(args, "fff:angles", &azimuth, &elevation, &distance))
			return nullptr;

		try
		{
			self->source = new std::shared_ptr<aud::Source>(new aud::Source(azimuth, elevation, distance));
		}
		catch(aud::Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

static void
Source_dealloc(SourceP* self)
{
	if(self->source)
		delete reinterpret_cast<std::shared_ptr<aud::Source>*>(self->source);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef Source_methods[] = {
	{ nullptr }  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Source_azimuth_doc,
	"The azimuth angle.");

static int
Source_set_azimuth(SourceP* self, PyObject* args, void* nothing)
{
	float azimuth;

	if(!PyArg_Parse(args, "f:azimuth", &azimuth))
		return -1;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::Source>*>(self->source))->setAzimuth(azimuth);
		return 0;
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

static PyObject *
Source_get_azimuth(SourceP* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<aud::Source>*>(self->source))->getAzimuth());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Source_elevation_doc,
	"The elevation angle.");

static int
Source_set_elevation(SourceP* self, PyObject* args, void* nothing)
{
	float elevation;

	if(!PyArg_Parse(args, "f:elevation", &elevation))
		return -1;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::Source>*>(self->source))->setElevation(elevation);
		return 0;
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

static PyObject *
Source_get_elevation(SourceP* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<aud::Source>*>(self->source))->getElevation());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Source_distance_doc,
	"The distance value. 0 is min, 1 is max.");

static int
Source_set_distance(SourceP* self, PyObject* args, void* nothing)
{
	float distance;

	if(!PyArg_Parse(args, "f:distance", &distance))
		return -1;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::Source>*>(self->source))->setDistance(distance);
		return 0;
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

static PyObject *
Source_get_distance(SourceP* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<aud::Source>*>(self->source))->getDistance());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static PyGetSetDef Source_properties[] = {
	{ (char*)"azimuth", (getter)Source_get_azimuth, (setter)Source_set_azimuth,
	M_aud_Source_azimuth_doc, nullptr },
	{ (char*)"elevation", (getter)Source_get_elevation, (setter)Source_set_elevation,
	M_aud_Source_elevation_doc, nullptr },
	{ (char*)"distance", (getter)Source_get_distance, (setter)Source_set_distance,
	M_aud_Source_distance_doc, nullptr },
	{ nullptr }  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Source_doc,
	"The source object represents the source position of a binaural sound.");

PyTypeObject SourceType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"aud.Source",							/* tp_name */
	sizeof(SourceP),						/* tp_basicsize */
	0,										/* tp_itemsize */
	(destructor)Source_dealloc,				/* tp_dealloc */
	0,										/* tp_print */
	0,										/* tp_getattr */
	0,										/* tp_setattr */
	0,										/* tp_reserved */
	0,										/* tp_repr */
	0,										/* tp_as_number */
	0,										/* tp_as_sequence */
	0,										/* tp_as_mapping */
	0,										/* tp_hash  */
	0,										/* tp_call */
	0,										/* tp_str */
	0,										/* tp_getattro */
	0,										/* tp_setattro */
	0,										/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,						/* tp_flags */
	M_aud_Source_doc,						/* tp_doc */
	0,										/* tp_traverse */
	0,										/* tp_clear */
	0,										/* tp_richcompare */
	0,										/* tp_weaklistoffset */
	0,										/* tp_iter */
	0,										/* tp_iternext */
	Source_methods,							/* tp_methods */
	0,										/* tp_members */
	Source_properties,						/* tp_getset */
	0,										/* tp_base */
	0,										/* tp_dict */
	0,										/* tp_descr_get */
	0,										/* tp_descr_set */
	0,										/* tp_dictoffset */
	0,										/* tp_init */
	0,										/* tp_alloc */
	Source_new,								/* tp_new */
};

AUD_API PyObject* Source_empty()
{
	return SourceType.tp_alloc(&SourceType, 0);
}


AUD_API SourceP* checkSource(PyObject* source)
{
	if(!PyObject_TypeCheck(source, &SourceType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type Source!");
		return nullptr;
	}

	return (SourceP*)source;
}


bool initializeSource()
{
	return PyType_Ready(&SourceType) >= 0;
}


void addSourceToModule(PyObject* module)
{
	Py_INCREF(&SourceType);
	PyModule_AddObject(module, "Source", (PyObject *)&SourceType);
}
