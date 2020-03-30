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

#include "PyHRTF.h"
#include "PySound.h"

#include "Exception.h"
#include "fx/HRTF.h"
#include "fx/HRTFLoader.h"

extern PyObject* AUDError;

static PyObject *
HRTF_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	HRTFP* self = (HRTFP*)type->tp_alloc(type, 0);

	if(self != nullptr)
	{
		try
		{
			self->hrtf = new std::shared_ptr<aud::HRTF>(new aud::HRTF());
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
HRTF_dealloc(HRTFP* self)
{
	if(self->hrtf)
		delete reinterpret_cast<std::shared_ptr<aud::HRTF>*>(self->hrtf);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(M_aud_HRTF_addImpulseResponse_doc,
	".. classmethod:: addImpulseResponseFromSound(sound, azimuth, elevation)\n\n"
	"   Adds a new hrtf to the HRTF object\n\n"
	"   :arg sound: The sound that contains the hrtf.\n"
	"   :type sound: :class:`Sound`\n"
	"   :arg azimuth: The azimuth angle of the hrtf.\n"
	"   :type azimuth: float\n"
	"   :arg elevation: The elevation angle of the hrtf.\n"
	"   :type elevation: float\n"
	"   :return: Whether the action succeeded.\n"
	"   :rtype: bool");

static PyObject *
HRTF_addImpulseResponseFromSound(HRTFP* self, PyObject* args)
{
	PyObject* object;
	float azimuth, elevation;

	if(!PyArg_ParseTuple(args, "Off:hrtf", &object, &azimuth, &elevation))
		return nullptr;

	Sound* ir = checkSound(object);
	if(!ir)
		return nullptr;

	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<aud::HRTF>*>(self->hrtf))->addImpulseResponse(std::make_shared<aud::StreamBuffer>(*reinterpret_cast<std::shared_ptr<aud::ISound>*>(ir->sound)), azimuth, elevation));
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_HRTF_loadLeftHrtfSet_doc,
	".. classmethod:: loadLeftHrtfSet(extension, directory)\n\n"
	"   Loads all HRTFs from a directory.\n\n"
	"   :arg extension: The file extension of the hrtfs.\n"
	"   :type extension: string\n"
	"   :arg directory: The path to where the HRTF files are located.\n"
	"   :type extension: string\n"
	"   :return: The loaded :class:`HRTF` object.\n"
	"   :rtype: :class:`HRTF`\n\n");

static PyObject *
HRTF_loadLeftHrtfSet(PyTypeObject* type, PyObject* args)
{
	const char* dir = nullptr;
	const char* ext = nullptr;

	if(!PyArg_ParseTuple(args, "ss:hrtf", &ext, &dir))
		return nullptr;

	HRTFP* self;
	self = (HRTFP*)type->tp_alloc(type, 0);

	try
	{
		self->hrtf = new std::shared_ptr<aud::HRTF>(aud::HRTFLoader::loadLeftHRTFs(ext, dir));
	}
	catch(aud::Exception& e)
	{
		Py_DECREF(self);
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_HRTF_loadRightHrtfSet_doc,
	".. classmethod:: loadLeftHrtfSet(extension, directory)\n\n"
	"   Loads all HRTFs from a directory.\n\n"
	"   :arg extension: The file extension of the hrtfs.\n"
	"   :type extension: string\n"
	"   :arg directory: The path to where the HRTF files are located.\n"
	"   :type extension: string\n"
	"   :return: The loaded :class:`HRTF` object.\n"
	"   :rtype: :class:`HRTF`\n\n");

static PyObject *
HRTF_loadRightHrtfSet(PyTypeObject* type, PyObject* args)
{
	const char* dir = nullptr;
	const char* ext = nullptr;

	if(!PyArg_ParseTuple(args, "ss:hrtf", &ext, &dir))
		return nullptr;

	HRTFP* self;
	self = (HRTFP*)type->tp_alloc(type, 0);

	try
	{
		self->hrtf = new std::shared_ptr<aud::HRTF>(aud::HRTFLoader::loadRightHRTFs(ext, dir));
	}
	catch(aud::Exception& e)
	{
		Py_DECREF(self);
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
	return (PyObject *)self;
}

static PyMethodDef HRTF_methods[] = {
	{ "addImpulseResponseFromSound", (PyCFunction)HRTF_addImpulseResponseFromSound, METH_VARARGS | METH_KEYWORDS,
	M_aud_HRTF_addImpulseResponse_doc
	},
	{ "loadLeftHrtfSet", (PyCFunction)HRTF_loadLeftHrtfSet, METH_VARARGS | METH_CLASS,
	M_aud_HRTF_loadLeftHrtfSet_doc
	},
	{ "loadRightHrtfSet", (PyCFunction)HRTF_loadRightHrtfSet, METH_VARARGS | METH_CLASS,
	M_aud_HRTF_loadRightHrtfSet_doc
	},
	{ nullptr }  /* Sentinel */
};

PyDoc_STRVAR(M_aud_HRTF_doc,
	"An HRTF object represents a set of head related transfer functions as impulse responses. It's used for binaural sound");

PyTypeObject HRTFType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"aud.HRTF",								/* tp_name */
	sizeof(HRTFP),							/* tp_basicsize */
	0,										/* tp_itemsize */
	(destructor)HRTF_dealloc,				/* tp_dealloc */
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
	M_aud_HRTF_doc,							/* tp_doc */
	0,										/* tp_traverse */
	0,										/* tp_clear */
	0,										/* tp_richcompare */
	0,										/* tp_weaklistoffset */
	0,										/* tp_iter */
	0,										/* tp_iternext */
	HRTF_methods,							/* tp_methods */
	0,										/* tp_members */
	0,										/* tp_getset */
	0,										/* tp_base */
	0,										/* tp_dict */
	0,										/* tp_descr_get */
	0,										/* tp_descr_set */
	0,										/* tp_dictoffset */
	0,										/* tp_init */
	0,										/* tp_alloc */
	HRTF_new,								/* tp_new */
};

AUD_API PyObject* HRTF_empty()
{
	return HRTFType.tp_alloc(&HRTFType, 0);
}


AUD_API HRTFP* checkHRTF(PyObject* hrtf)
{
	if(!PyObject_TypeCheck(hrtf, &HRTFType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type HRTF!");
		return nullptr;
	}

	return (HRTFP*)hrtf;
}


bool initializeHRTF()
{
	return PyType_Ready(&HRTFType) >= 0;
}


void addHRTFToModule(PyObject* module)
{
	Py_INCREF(&HRTFType);
	PyModule_AddObject(module, "HRTF", (PyObject *)&HRTFType);
}
