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

#include "PyThreadPool.h"

#include "Exception.h"
#include "util/ThreadPool.h"

extern PyObject* AUDError;

static PyObject *
ThreadPool_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	ThreadPoolP* self = (ThreadPoolP*)type->tp_alloc(type, 0);

	if(self != nullptr)
	{
		unsigned int nThreads;
		if(!PyArg_ParseTuple(args, "I:nThreads", &nThreads))
			return nullptr;

		try
		{
			self->threadPool = new std::shared_ptr<aud::ThreadPool>(new aud::ThreadPool(nThreads));
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
ThreadPool_dealloc(ThreadPoolP* self)
{
	if(self->threadPool)
		delete reinterpret_cast<std::shared_ptr<aud::ThreadPool>*>(self->threadPool);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef ThreadPool_methods[] = {
	{ nullptr }  /* Sentinel */
};

PyDoc_STRVAR(M_aud_ThreadPool_doc,
	"A ThreadPool is used to parallelize convolution efficiently.");

PyTypeObject ThreadPoolType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"aud.ThreadPool",						/* tp_name */
	sizeof(ThreadPoolP),					/* tp_basicsize */
	0,										/* tp_itemsize */
	(destructor)ThreadPool_dealloc,			/* tp_dealloc */
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
	M_aud_ThreadPool_doc,					/* tp_doc */
	0,										/* tp_traverse */
	0,										/* tp_clear */
	0,										/* tp_richcompare */
	0,										/* tp_weaklistoffset */
	0,										/* tp_iter */
	0,										/* tp_iternext */
	ThreadPool_methods,						/* tp_methods */
	0,										/* tp_members */
	0,										/* tp_getset */
	0,										/* tp_base */
	0,										/* tp_dict */
	0,										/* tp_descr_get */
	0,										/* tp_descr_set */
	0,										/* tp_dictoffset */
	0,										/* tp_init */
	0,										/* tp_alloc */
	ThreadPool_new,							/* tp_new */
};

AUD_API PyObject* ThreadPool_empty()
{
	return ThreadPoolType.tp_alloc(&ThreadPoolType, 0);
}


AUD_API ThreadPoolP* checkThreadPool(PyObject* threadPool)
{
	if(!PyObject_TypeCheck(threadPool, &ThreadPoolType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type ThreadPool!");
		return nullptr;
	}

	return (ThreadPoolP*)threadPool;
}


bool initializeThreadPool()
{
	return PyType_Ready(&ThreadPoolType) >= 0;
}


void addThreadPoolToModule(PyObject* module)
{
	Py_INCREF(&ThreadPoolType);
	PyModule_AddObject(module, "ThreadPool", (PyObject *)&ThreadPoolType);
}
