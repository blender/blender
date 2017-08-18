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

#include "PyImpulseResponse.h"
#include "PySound.h"

#include "Exception.h"
#include "fx/ImpulseResponse.h"
#include "util/StreamBuffer.h"

extern PyObject* AUDError;

static PyObject *
ImpulseResponse_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	ImpulseResponseP* self = (ImpulseResponseP*)type->tp_alloc(type, 0);

	if(self != nullptr)
	{
		PyObject* object;
		if(!PyArg_ParseTuple(args, "O:sound", &object))
			return nullptr;
		Sound* sound = checkSound(object);

		try
		{
			self->impulseResponse = new std::shared_ptr<aud::ImpulseResponse>(new aud::ImpulseResponse(std::make_shared<aud::StreamBuffer>(*reinterpret_cast<std::shared_ptr<aud::ISound>*>(sound->sound))));
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
ImpulseResponse_dealloc(ImpulseResponseP* self)
{
	if(self->impulseResponse)
		delete reinterpret_cast<std::shared_ptr<aud::ImpulseResponse>*>(self->impulseResponse);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef ImpulseResponse_methods[] = {
	{ nullptr }  /* Sentinel */
};

PyDoc_STRVAR(M_aud_ImpulseResponse_doc,
	"An ImpulseResponse object represents a filter with which to convolve a sound.");

PyTypeObject ImpulseResponseType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"aud.ImpulseResponse",					/* tp_name */
	sizeof(ImpulseResponseP),				/* tp_basicsize */
	0,										/* tp_itemsize */
	(destructor)ImpulseResponse_dealloc,			/* tp_dealloc */
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
	M_aud_ImpulseResponse_doc,				/* tp_doc */
	0,										/* tp_traverse */
	0,										/* tp_clear */
	0,										/* tp_richcompare */
	0,										/* tp_weaklistoffset */
	0,										/* tp_iter */
	0,										/* tp_iternext */
	ImpulseResponse_methods,				/* tp_methods */
	0,										/* tp_members */
	0,										/* tp_getset */
	0,										/* tp_base */
	0,										/* tp_dict */
	0,										/* tp_descr_get */
	0,										/* tp_descr_set */
	0,										/* tp_dictoffset */
	0,										/* tp_init */
	0,										/* tp_alloc */
	ImpulseResponse_new,					/* tp_new */
};

AUD_API PyObject* ImpulseResponse_empty()
{
	return ImpulseResponseType.tp_alloc(&ImpulseResponseType, 0);
}


AUD_API ImpulseResponseP* checkImpulseResponse(PyObject* impulseResponse)
{
	if(!PyObject_TypeCheck(impulseResponse, &ImpulseResponseType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type ImpulseResponse!");
		return nullptr;
	}

	return (ImpulseResponseP*)impulseResponse;
}


bool initializeImpulseResponse()
{
	return PyType_Ready(&ImpulseResponseType) >= 0;
}


void addImpulseResponseToModule(PyObject* module)
{
	Py_INCREF(&ImpulseResponseType);
	PyModule_AddObject(module, "ImpulseResponse", (PyObject *)&ImpulseResponseType);
}
