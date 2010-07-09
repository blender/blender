/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#include "AUD_PyAPI.h"
#include "structmember.h"

#include "AUD_NULLDevice.h"
#include "AUD_SourceCaps.h"
#include "AUD_DelayFactory.h"
#include "AUD_DoubleFactory.h"
#include "AUD_FaderFactory.h"
#include "AUD_HighpassFactory.h"
#include "AUD_LimiterFactory.h"
#include "AUD_LoopFactory.h"
#include "AUD_LowpassFactory.h"
#include "AUD_PingPongFactory.h"
#include "AUD_PitchFactory.h"
#include "AUD_ReverseFactory.h"
#include "AUD_SinusFactory.h"
#include "AUD_FileFactory.h"
#include "AUD_SquareFactory.h"
#include "AUD_StreamBufferFactory.h"
#include "AUD_SuperposeFactory.h"
#include "AUD_VolumeFactory.h"

#ifdef WITH_SDL
#include "AUD_SDLDevice.h"
#endif

#ifdef WITH_OPENAL
#include "AUD_OpenALDevice.h"
#endif

#ifdef WITH_JACK
#include "AUD_JackDevice.h"
#endif

#include <cstdlib>
#include <unistd.h>

#define PY_MODULE_ADD_CONSTANT(module, name) PyModule_AddIntConstant(module, #name, name)

static PyObject* AUDError;

// ====================================================================

static void
Sound_dealloc(Sound* self)
{
	if(self->factory)
		delete self->factory;
	Py_XDECREF(self->child_list);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
Sound_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Sound *self;

	self = (Sound*)type->tp_alloc(type, 0);
	if(self != NULL)
	{
		static char *kwlist[] = {"filename", NULL};
		const char* filename = NULL;

		if(!PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &filename))
		{
			Py_DECREF(self);
			return NULL;
		}
		else if(filename == NULL)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Missing filename parameter!");
			return NULL;
		}

		try
		{
			self->factory = new AUD_FileFactory(filename);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Filefactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_sine(PyObject* nothing, PyObject* args);

static PyObject *
Sound_file(PyObject* nothing, PyObject* args);

static PyObject *
Sound_lowpass(PyObject* nothing, PyObject* args);

static PyObject *
Sound_delay(PyObject* nothing, PyObject* args);

static PyObject *
Sound_double(PyObject* nothing, PyObject* args);

static PyObject *
Sound_highpass(PyObject* nothing, PyObject* args);

static PyObject *
Sound_limiter(PyObject* nothing, PyObject* args);

static PyObject *
Sound_pitch(PyObject* nothing, PyObject* args);

static PyObject *
Sound_volume(PyObject* nothing, PyObject* args);

static PyObject *
Sound_fadein(PyObject* nothing, PyObject* args);

static PyObject *
Sound_fadeout(PyObject* nothing, PyObject* args);

static PyObject *
Sound_loop(PyObject* nothing, PyObject* args);

static PyObject *
Sound_superpose(PyObject* nothing, PyObject* args);

static PyObject *
Sound_pingpong(PyObject* nothing, PyObject* args);

static PyObject *
Sound_reverse(PyObject* nothing, PyObject* args);

static PyObject *
Sound_buffer(PyObject* nothing, PyObject* args);

static PyObject *
Sound_square(PyObject* nothing, PyObject* args);

static PyMethodDef Sound_methods[] = {
	{"sine", (PyCFunction)Sound_sine, METH_VARARGS | METH_STATIC,
	 "Creates a sine sound at a specific frequency."
	},
	{"file", (PyCFunction)Sound_file, METH_VARARGS | METH_STATIC,
	 "Creates a sound object of a sound file."
	},
	{"lowpass", (PyCFunction)Sound_lowpass, METH_VARARGS | METH_STATIC,
	 "Creates a lowpass filter with a specific cut off frequency."
	},
	{"delay", (PyCFunction)Sound_delay, METH_VARARGS | METH_STATIC,
	 "Delays a sound by a specific amount of seconds."
	},
	{"double", (PyCFunction)Sound_double, METH_VARARGS | METH_STATIC,
	 "Plays two sounds of the same specs in sequence."
	},
	{"highpass", (PyCFunction)Sound_highpass, METH_VARARGS | METH_STATIC,
	 "Creates a highpass filter with a specific cut off frequency."
	},
	{"limiter", (PyCFunction)Sound_limiter, METH_VARARGS | METH_STATIC,
	 "Limits a sound within a specific start and end time."
	},
	{"pitch", (PyCFunction)Sound_pitch, METH_VARARGS | METH_STATIC,
	 "Changes the pitch of a sound with a specific factor."
	},
	{"volume", (PyCFunction)Sound_volume, METH_VARARGS | METH_STATIC,
	 "Changes the volume of a sound with a specific factor."
	},
	{"fadein", (PyCFunction)Sound_fadein, METH_VARARGS | METH_STATIC,
	 "Fades a sound in from a specific start time and with a specific length."
	},
	{"fadeout", (PyCFunction)Sound_fadeout, METH_VARARGS | METH_STATIC,
	 "Fades a sound out from a specific start time and with a specific length."
	},
	{"loop", (PyCFunction)Sound_loop, METH_VARARGS | METH_STATIC,
	 "Loops a sound a specific amount of times, negative values mean endlessly."
	},
	{"superpose", (PyCFunction)Sound_superpose, METH_VARARGS | METH_STATIC,
	 "Mixes two sounds of the same specs."
	},
	{"pingpong", (PyCFunction)Sound_pingpong, METH_O | METH_STATIC,
	 "Plays a sound forward and then backward."
	},
	{"reverse", (PyCFunction)Sound_reverse, METH_O | METH_STATIC,
	 "Plays a sound reversed."
	},
	{"buffer", (PyCFunction)Sound_buffer, METH_O | METH_STATIC,
	 "Buffers a sound into RAM."
	},
	{"square", (PyCFunction)Sound_square, METH_VARARGS | METH_STATIC,
	 "Makes a square wave out of an audio wave depending on a threshold value."
	},
	{NULL}  /* Sentinel */
};

static PyTypeObject SoundType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"aud.Sound",               /* tp_name */
	sizeof(Sound),             /* tp_basicsize */
	0,                         /* tp_itemsize */
	(destructor)Sound_dealloc, /* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_reserved */
	0,                         /* tp_repr */
	0,                         /* tp_as_number */
	0,                         /* tp_as_sequence */
	0,                         /* tp_as_mapping */
	0,                         /* tp_hash  */
	0,                         /* tp_call */
	0,                         /* tp_str */
	0,                         /* tp_getattro */
	0,                         /* tp_setattro */
	0,                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,        /* tp_flags */
	"Sound object",            /* tp_doc */
	0,		                   /* tp_traverse */
	0,		                   /* tp_clear */
	0,		                   /* tp_richcompare */
	0,		                   /* tp_weaklistoffset */
	0,		                   /* tp_iter */
	0,		                   /* tp_iternext */
	Sound_methods,             /* tp_methods */
	0,                         /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,                         /* tp_init */
	0,                         /* tp_alloc */
	Sound_new,                 /* tp_new */
};

static PyObject *
Sound_sine(PyObject* nothing, PyObject* args)
{
	double frequency;

	if(!PyArg_ParseTuple(args, "d", &frequency))
		return NULL;

	Sound *self;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		try
		{
			self->factory = new AUD_SinusFactory(frequency, (AUD_SampleRate)44100);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Sinusfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_file(PyObject* nothing, PyObject* args)
{
	const char* filename = NULL;

	if(!PyArg_ParseTuple(args, "s", &filename))
		return NULL;

	Sound *self;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		try
		{
			self->factory = new AUD_FileFactory(filename);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Filefactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_lowpass(PyObject* nothing, PyObject* args)
{
	float frequency;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Of", &object, &frequency))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_LowpassFactory(child->factory, frequency, 0.9);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Lowpassfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_delay(PyObject* nothing, PyObject* args)
{
	float delay;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Of", &object, &delay))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_DelayFactory(child->factory, delay);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Delayfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_double(PyObject* nothing, PyObject* args)
{
	PyObject* object1;
	PyObject* object2;

	if(!PyArg_ParseTuple(args, "OO", &object1, &object2))
		return NULL;

	if(!PyObject_TypeCheck(object1, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "First object is not of type aud.Sound!");
		return NULL;
	}

	if(!PyObject_TypeCheck(object2, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Second object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child1 = (Sound*)object1;
	Sound *child2 = (Sound*)object2;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		self->child_list = Py_BuildValue("(OO)", object1, object2);

		try
		{
			self->factory = new AUD_DoubleFactory(child1->factory, child2->factory);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Doublefactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_superpose(PyObject* nothing, PyObject* args)
{
	PyObject* object1;
	PyObject* object2;

	if(!PyArg_ParseTuple(args, "OO", &object1, &object2))
		return NULL;

	if(!PyObject_TypeCheck(object1, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "First object is not of type aud.Sound!");
		return NULL;
	}

	if(!PyObject_TypeCheck(object2, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Second object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child1 = (Sound*)object1;
	Sound *child2 = (Sound*)object2;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		self->child_list = Py_BuildValue("(OO)", object1, object2);

		try
		{
			self->factory = new AUD_SuperposeFactory(child1->factory, child2->factory);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Superposefactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_highpass(PyObject* nothing, PyObject* args)
{
	float frequency;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Of", &object, &frequency))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_HighpassFactory(child->factory, frequency, 0.9);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Highpassfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_limiter(PyObject* nothing, PyObject* args)
{
	float start, end;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Off", &object, &start, &end))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_LimiterFactory(child->factory, start, end);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Limiterfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_pitch(PyObject* nothing, PyObject* args)
{
	float factor;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Of", &object, &factor))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_PitchFactory(child->factory, factor);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Pitchfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_volume(PyObject* nothing, PyObject* args)
{
	float volume;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Of", &object, &volume))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_VolumeFactory(child->factory, volume);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Volumefactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_square(PyObject* nothing, PyObject* args)
{
	float threshold;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Of", &object, &threshold))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_SquareFactory(child->factory, threshold);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Squarefactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_fadein(PyObject* nothing, PyObject* args)
{
	float start, length;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Off", &object, &start, &length))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_FaderFactory(child->factory, AUD_FADE_IN, start, length);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Faderfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_fadeout(PyObject* nothing, PyObject* args)
{
	float start, length;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Off", &object, &start, &length))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_FaderFactory(child->factory, AUD_FADE_OUT, start, length);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Faderfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_loop(PyObject* nothing, PyObject* args)
{
	int loop;
	PyObject* object;

	if(!PyArg_ParseTuple(args, "Oi", &object, &loop))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_LoopFactory(child->factory, loop);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Loopfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_pingpong(PyObject* nothing, PyObject* object)
{
	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_PingPongFactory(child->factory);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Pingpongfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_reverse(PyObject* nothing, PyObject* object)
{
	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		Py_INCREF(object);
		self->child_list = object;

		try
		{
			self->factory = new AUD_ReverseFactory(child->factory);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Reversefactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

static PyObject *
Sound_buffer(PyObject* nothing, PyObject* object)
{
	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	Sound *self;
	Sound *child = (Sound*)object;

	self = (Sound*)SoundType.tp_alloc(&SoundType, 0);
	if(self != NULL)
	{
		try
		{
			self->factory = new AUD_StreamBufferFactory(child->factory);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Bufferfactory couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

// ========== Handle ==================================================

static void
Handle_dealloc(Handle* self)
{
	Py_XDECREF(self->device);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
Handle_pause(Handle *self)
{
	return PyObject_CallMethod(self->device, "pause", "(O)", self);
}

static PyObject *
Handle_resume(Handle *self)
{
	return PyObject_CallMethod(self->device, "resume", "(O)", self);
}

static PyObject *
Handle_stop(Handle *self)
{
	return PyObject_CallMethod(self->device, "stop", "(O)", self);
}

static PyObject *
Handle_update(Handle *self, PyObject *data)
{
	return PyObject_CallMethod(self->device, "updateSource", "(OO)", self, data);
}

static PyMethodDef Handle_methods[] = {
	{"pause", (PyCFunction)Handle_pause, METH_NOARGS,
	 "Pauses the sound."
	},
	{"resume", (PyCFunction)Handle_resume, METH_NOARGS,
	 "Resumes the sound."
	},
	{"stop", (PyCFunction)Handle_stop, METH_NOARGS,
	 "Stops the sound."
	},
	{"update", (PyCFunction)Handle_update, METH_O,
	 "Updates the 3D information of the source. Awaits a 3D position and velocity vector and a 3x3 orientation matrix."
	},
	{NULL}  /* Sentinel */
};

static PyObject *
Handle_getPosition(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getPosition", "(O)", self);
}

static int
Handle_setPosition(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "seek", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static int
Handle_setKeep(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setKeep", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getStatus(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getStatus", "(O)", self);
}

static PyObject *
Handle_getVolume(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getVolume", "(O)", self);
}

static int
Handle_setVolume(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setVolume", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static int
Handle_setPitch(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setPitch", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static int
Handle_setLoopCount(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setLoopCount", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getRelative(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "isRelative", "(O)", self);
}

static int
Handle_setRelative(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setRelative", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getMinGain(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getMinGain", "(O)", self);
}

static int
Handle_setMinGain(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setMinGain", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getMaxGain(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getMaxGain", "(O)", self);
}

static int
Handle_setMaxGain(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setMaxGain", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getReferenceDistance(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getReferenceDistance", "(O)", self);
}

static int
Handle_setReferenceDistance(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setReferenceDistance", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getMaxDistance(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getMaxDistance", "(O)", self);
}

static int
Handle_setMaxDistance(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setMaxDistance", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getRolloffFactor(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getRolloffFactor", "(O)", self);
}

static int
Handle_setRolloffFactor(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setRolloffFactor", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getConeInnerAngle(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getConeInnerAngle", "(O)", self);
}

static int
Handle_setConeInnerAngle(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setConeInnerAngle", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getConeOuterAngle(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getConeOuterAngle", "(O)", self);
}

static int
Handle_setConeOuterAngle(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setConeOuterAngle", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyObject *
Handle_getConeOuterGain(Handle *self, void* nothing)
{
	return PyObject_CallMethod(self->device, "getConeOuterGain", "(O)", self);
}

static int
Handle_setConeOuterGain(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = PyObject_CallMethod(self->device, "setConeOuterGain", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyGetSetDef Handle_properties[] = {
	{"position", (getter)Handle_getPosition, (setter)Handle_setPosition,
	 "The playback position of the sound.", NULL },
	{"keep", NULL, (setter)Handle_setKeep,
	 "Whether the sound should be kept paused in the device when it's end is reached.", NULL },
	{"status", (getter)Handle_getStatus, NULL,
	 "Whether the sound is playing, paused or stopped.", NULL },
	{"volume", (getter)Handle_getVolume, (setter)Handle_setVolume,
	 "The volume of the sound.", NULL },
	{"pitch", NULL, (setter)Handle_setPitch,
	 "The pitch of the sound.", NULL },
	{"loopcount", NULL, (setter)Handle_setLoopCount,
	 "The loop count of the sound. A negative value indicates infinity.", NULL },
	{"relative", (getter)Handle_getRelative, (setter)Handle_setRelative,
	 "Whether the source's position is relative or absolute to the listener.", NULL },
	{"min_gain", (getter)Handle_getMinGain, (setter)Handle_setMinGain,
	 "The minimum gain of the source.", NULL },
	{"max_gain", (getter)Handle_getMaxGain, (setter)Handle_setMaxGain,
	 "The maximum gain of the source.", NULL },
	{"reference_distance", (getter)Handle_getReferenceDistance, (setter)Handle_setReferenceDistance,
	 "The reference distance of the source.", NULL },
	{"max_distance", (getter)Handle_getMaxDistance, (setter)Handle_setMaxDistance,
	 "The maximum distance of the source.", NULL },
	{"rolloff_factor", (getter)Handle_getRolloffFactor, (setter)Handle_setRolloffFactor,
	 "The rolloff factor of the source.", NULL },
	{"cone_inner_angle", (getter)Handle_getConeInnerAngle, (setter)Handle_setConeInnerAngle,
	 "The cone inner angle of the source.", NULL },
	{"cone_outer_angle", (getter)Handle_getConeOuterAngle, (setter)Handle_setConeOuterAngle,
	 "The cone outer angle of the source.", NULL },
	{"cone_outer_gain", (getter)Handle_getConeOuterGain, (setter)Handle_setConeOuterGain,
	 "The cone outer gain of the source.", NULL },
	{NULL}  /* Sentinel */
};

static PyTypeObject HandleType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"aud.Handle",              /* tp_name */
	sizeof(Handle),            /* tp_basicsize */
	0,                         /* tp_itemsize */
	(destructor)Handle_dealloc,/* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_reserved */
	0,                         /* tp_repr */
	0,                         /* tp_as_number */
	0,                         /* tp_as_sequence */
	0,                         /* tp_as_mapping */
	0,                         /* tp_hash  */
	0,                         /* tp_call */
	0,                         /* tp_str */
	0,                         /* tp_getattro */
	0,                         /* tp_setattro */
	0,                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,        /* tp_flags */
	"Handle object",           /* tp_doc */
	0,		                   /* tp_traverse */
	0,		                   /* tp_clear */
	0,		                   /* tp_richcompare */
	0,		                   /* tp_weaklistoffset */
	0,		                   /* tp_iter */
	0,		                   /* tp_iternext */
	Handle_methods,            /* tp_methods */
	0,                         /* tp_members */
	Handle_properties,         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,                         /* tp_init */
	0,                         /* tp_alloc */
	0,                         /* tp_new */
};

// ========== Device ==================================================

static void
Device_dealloc(Device* self)
{
	if(self->device)
		delete self->device;
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
Device_play(Device *self, PyObject *args, PyObject *kwds)
{
	PyObject* object;
	PyObject* keepo = NULL;

	bool keep = false;

	static char *kwlist[] = {"sound", "keep", NULL};

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|O", kwlist, &object, &keepo))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	if(keepo != NULL)
	{
		if(!PyBool_Check(keepo))
		{
			PyErr_SetString(PyExc_TypeError, "keep is not a boolean!");
			return NULL;
		}

		keep = keepo == Py_True;
	}

	Sound* sound = (Sound*)object;
	Handle *handle;

	handle = (Handle*)HandleType.tp_alloc(&HandleType, 0);
	if(handle != NULL)
	{
		handle->device = (PyObject*)self;
		Py_INCREF(self);

		try
		{
			handle->handle = self->device->play(sound->factory, keep);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(handle);
			PyErr_SetString(AUDError, "Couldn't play the sound!");
			return NULL;
		}
	}

	return (PyObject *)handle;
}

static PyObject *
Device_stop(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		if(self->device->stop(handle->handle))
		{
			Py_RETURN_TRUE;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't stop the sound!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_pause(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		if(self->device->pause(handle->handle))
		{
			Py_RETURN_TRUE;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't pause the sound!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_resume(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		if(self->device->resume(handle->handle))
		{
			Py_RETURN_TRUE;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't resume the sound!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_setKeep(Device *self, PyObject *args)
{
	PyObject* object;
	PyObject* keepo;

	if(!PyArg_ParseTuple(args, "OO", &object, &keepo))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	if(!PyBool_Check(keepo))
	{
		PyErr_SetString(PyExc_TypeError, "keep is not a boolean!");
		return NULL;
	}

	bool keep = keepo == Py_True;
	Handle* handle = (Handle*)object;

	try
	{
		if(self->device->setKeep(handle->handle, keep))
		{
			Py_RETURN_TRUE;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set keep of the sound!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_seek(Device *self, PyObject *args)
{
	PyObject* object;
	float position;

	if(!PyArg_ParseTuple(args, "Of", &object, &position))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		if(self->device->seek(handle->handle, position))
		{
			Py_RETURN_TRUE;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't seek the sound!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getPosition(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		return Py_BuildValue("f", self->device->getPosition(handle->handle));
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the position of the sound!");
		return NULL;
	}
}

static PyObject *
Device_getStatus(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		return Py_BuildValue("i", self->device->getStatus(handle->handle));
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the status of the sound!");
		return NULL;
	}
}

static PyObject *
Device_lock(Device *self)
{
	try
	{
		self->device->lock();
		Py_RETURN_NONE;
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't lock the device!");
		return NULL;
	}
}

static PyObject *
Device_unlock(Device *self)
{
	try
	{
		self->device->unlock();
		Py_RETURN_NONE;
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't unlock the device!");
		return NULL;
	}
}

static PyObject *
Device_setSourceVolume(Device *self, PyObject *args)
{
	PyObject* object;
	float volume;

	if(!PyArg_ParseTuple(args, "Of", &object, &volume))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_SourceCaps caps;
		caps.handle = handle->handle;
		caps.value = volume;
		if(self->device->setCapability(AUD_CAPS_SOURCE_VOLUME, &caps))
		{
			Py_RETURN_TRUE;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the sound volume!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getSourceVolume(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_SourceCaps caps;
		caps.handle = handle->handle;
		caps.value = 1.0f;
		if(self->device->getCapability(AUD_CAPS_SOURCE_VOLUME, &caps))
		{
			return Py_BuildValue("f", caps.value);
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't get the sound volume!");
		return NULL;
	}

	Py_RETURN_NAN;
}

static PyObject *
Device_setLoopCount(Device *self, PyObject *args)
{
	PyObject* object;
	int loops;

	if(!PyArg_ParseTuple(args, "Oi", &object, &loops))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_Message message;
		message.loopcount = loops;
		message.type = AUD_MSG_LOOP;
		if(self->device->sendMessage(handle->handle, message))
		{
			Py_RETURN_TRUE;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the loop count!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_setPitch(Device *self, PyObject *args)
{
	PyObject* object;
	float pitch;

	if(!PyArg_ParseTuple(args, "Of", &object, &pitch))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_SourceCaps caps;
		caps.handle = handle->handle;
		caps.value = pitch;
		if(self->device->setCapability(AUD_CAPS_SOURCE_PITCH, &caps))
		{
			Py_RETURN_TRUE;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the sound pitch!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_play3D(Device *self, PyObject *args, PyObject *kwds)
{
	PyObject* object;
	PyObject* keepo = NULL;

	bool keep = false;

	static char *kwlist[] = {"sound", "keep", NULL};

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|O", kwlist, &object, &keepo))
		return NULL;

	if(!PyObject_TypeCheck(object, &SoundType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Sound!");
		return NULL;
	}

	if(keepo != NULL)
	{
		if(!PyBool_Check(keepo))
		{
			PyErr_SetString(PyExc_TypeError, "keep is not a boolean!");
			return NULL;
		}

		keep = keepo == Py_True;
	}

	Sound* sound = (Sound*)object;
	Handle *handle;

	handle = (Handle*)HandleType.tp_alloc(&HandleType, 0);
	if(handle != NULL)
	{
		handle->device = (PyObject*)self;
		Py_INCREF(self);

		try
		{
			AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
			if(device)
			{
				handle->handle = device->play3D(sound->factory, keep);
			}
			else
			{
				Py_DECREF(handle);
				PyErr_SetString(AUDError, "Device is not a 3D device!");
				return NULL;
			}
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(handle);
			PyErr_SetString(AUDError, "Couldn't play the sound!");
			return NULL;
		}
	}

	return (PyObject *)handle;
}

static PyObject *
Device_updateListener(Device *self, PyObject *args)
{
	AUD_3DData data;

	if(!PyArg_ParseTuple(args, "(fff)(fff)((fff)(fff)(fff))",
						 &data.position[0], &data.position[1], &data.position[2],
						 &data.velocity[0], &data.velocity[1], &data.velocity[2],
						 &data.orientation[0], &data.orientation[1], &data.orientation[2],
						 &data.orientation[3], &data.orientation[4], &data.orientation[5],
						 &data.orientation[6], &data.orientation[7], &data.orientation[8]))
		return NULL;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->updateListener(data);
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't update the listener!");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *
Device_updateSource(Device *self, PyObject *args)
{
	PyObject* object;
	AUD_3DData data;

	if(!PyArg_ParseTuple(args, "O(fff)(fff)((fff)(fff)(fff))", &object,
						 &data.position[0], &data.position[1], &data.position[2],
						 &data.velocity[0], &data.velocity[1], &data.velocity[2],
						 &data.orientation[0], &data.orientation[1], &data.orientation[2],
						 &data.orientation[3], &data.orientation[4], &data.orientation[5],
						 &data.orientation[6], &data.orientation[7], &data.orientation[8]))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			if(device->updateSource(handle->handle, data))
				Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't update the source!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_isRelative(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			if(device->getSourceSetting(handle->handle, AUD_3DSS_IS_RELATIVE) > 0)
			{
				Py_RETURN_TRUE;
			}
			else
			{
				Py_RETURN_FALSE;
			}
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the status of the sound!");
		return NULL;
	}
}

static PyObject *
Device_setRelative(Device *self, PyObject *args)
{
	PyObject* object;
	PyObject* relativeo;

	if(!PyArg_ParseTuple(args, "OO", &object, &relativeo))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	if(!PyBool_Check(relativeo))
	{
		PyErr_SetString(PyExc_TypeError, "Value is not a boolean!");
		return NULL;
	}

	float relative = (relativeo == Py_True);
	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSourceSetting(handle->handle, AUD_3DSS_IS_RELATIVE, relative);
			Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the status!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getMinGain(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSourceSetting(handle->handle, AUD_3DSS_MIN_GAIN));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the minimum gain of the sound!");
		return NULL;
	}
}

static PyObject *
Device_setMinGain(Device *self, PyObject *args)
{
	PyObject* object;
	float gain;

	if(!PyArg_ParseTuple(args, "Of", &object, &gain))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSourceSetting(handle->handle, AUD_3DSS_MIN_GAIN, gain);
			Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the minimum source gain!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getMaxGain(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSourceSetting(handle->handle, AUD_3DSS_MAX_GAIN));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the maximum gain of the sound!");
		return NULL;
	}
}

static PyObject *
Device_setMaxGain(Device *self, PyObject *args)
{
	PyObject* object;
	float gain;

	if(!PyArg_ParseTuple(args, "Of", &object, &gain))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSourceSetting(handle->handle, AUD_3DSS_MAX_GAIN, gain);
			Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the maximum source gain!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getReferenceDistance(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSourceSetting(handle->handle, AUD_3DSS_REFERENCE_DISTANCE));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the reference distance of the sound!");
		return NULL;
	}
}

static PyObject *
Device_setReferenceDistance(Device *self, PyObject *args)
{
	PyObject* object;
	float distance;

	if(!PyArg_ParseTuple(args, "Of", &object, &distance))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSourceSetting(handle->handle, AUD_3DSS_REFERENCE_DISTANCE, distance);
			Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the reference distance!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getMaxDistance(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSourceSetting(handle->handle, AUD_3DSS_MAX_DISTANCE));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the maximum distance of the sound!");
		return NULL;
	}
}

static PyObject *
Device_setMaxDistance(Device *self, PyObject *args)
{
	PyObject* object;
	float distance;

	if(!PyArg_ParseTuple(args, "Of", &object, &distance))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSourceSetting(handle->handle, AUD_3DSS_MAX_DISTANCE, distance);
			Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the maximum distance!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getRolloffFactor(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSourceSetting(handle->handle, AUD_3DSS_ROLLOFF_FACTOR));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the rolloff factor of the sound!");
		return NULL;
	}
}

static PyObject *
Device_setRolloffFactor(Device *self, PyObject *args)
{
	PyObject* object;
	float factor;

	if(!PyArg_ParseTuple(args, "Of", &object, &factor))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSourceSetting(handle->handle, AUD_3DSS_ROLLOFF_FACTOR, factor);
			Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the rolloff factor!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getConeInnerAngle(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSourceSetting(handle->handle, AUD_3DSS_CONE_INNER_ANGLE));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the cone inner angle of the sound!");
		return NULL;
	}
}

static PyObject *
Device_setConeInnerAngle(Device *self, PyObject *args)
{
	PyObject* object;
	float angle;

	if(!PyArg_ParseTuple(args, "Of", &object, &angle))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSourceSetting(handle->handle, AUD_3DSS_CONE_INNER_ANGLE, angle);
			Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the cone inner angle!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getConeOuterAngle(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSourceSetting(handle->handle, AUD_3DSS_CONE_OUTER_ANGLE));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the cone outer angle of the sound!");
		return NULL;
	}
}

static PyObject *
Device_setConeOuterAngle(Device *self, PyObject *args)
{
	PyObject* object;
	float angle;

	if(!PyArg_ParseTuple(args, "Of", &object, &angle))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSourceSetting(handle->handle, AUD_3DSS_CONE_OUTER_ANGLE, angle);
			Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the cone outer angle!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_getConeOuterGain(Device *self, PyObject *object)
{
	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSourceSetting(handle->handle, AUD_3DSS_CONE_OUTER_GAIN));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve the cone outer gain of the sound!");
		return NULL;
	}
}

static PyObject *
Device_setConeOuterGain(Device *self, PyObject *args)
{
	PyObject* object;
	float gain;

	if(!PyArg_ParseTuple(args, "Of", &object, &gain))
		return NULL;

	if(!PyObject_TypeCheck(object, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type aud.Handle!");
		return NULL;
	}

	Handle* handle = (Handle*)object;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSourceSetting(handle->handle, AUD_3DSS_CONE_OUTER_GAIN, gain);
			Py_RETURN_TRUE;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set the cone outer gain!");
		return NULL;
	}

	Py_RETURN_FALSE;
}

static PyObject *
Device_OpenAL(PyTypeObject *type, PyObject *args, PyObject *kwds);

static PyObject *
Device_SDL(PyTypeObject *type, PyObject *args, PyObject *kwds);

static PyObject *
Device_Jack(PyTypeObject *type, PyObject *args, PyObject *kwds);

static PyObject *
Device_Null(PyTypeObject *type);

static PyMethodDef Device_methods[] = {
	{"play", (PyCFunction)Device_play, METH_VARARGS | METH_KEYWORDS,
	 "Plays a sound."
	},
	{"stop", (PyCFunction)Device_stop, METH_O,
	 "Stops a playing sound."
	},
	{"pause", (PyCFunction)Device_pause, METH_O,
	 "Pauses a playing sound."
	},
	{"resume", (PyCFunction)Device_resume, METH_O,
	 "Resumes a playing sound."
	},
	{"setKeep", (PyCFunction)Device_setKeep, METH_VARARGS,
	 "Sets whether a sound should be kept or not."
	},
	{"seek", (PyCFunction)Device_seek, METH_VARARGS,
	 "Seeks the sound to a specific position expressed in seconds."
	},
	{"getPosition", (PyCFunction)Device_getPosition, METH_O,
	 "Retrieves the playback position of a sound in seconds."
	},
	{"getStatus", (PyCFunction)Device_getStatus, METH_O,
	 "Retrieves the playback status of a sound."
	},
	{"lock", (PyCFunction)Device_lock, METH_NOARGS,
	 "Locks the sound device."
	},
	{"unlock", (PyCFunction)Device_unlock, METH_NOARGS,
	 "Unlocks the sound device."
	},
	{"setVolume", (PyCFunction)Device_setSourceVolume, METH_VARARGS,
	 "Sets the volume of a source."
	},
	{"getVolume", (PyCFunction)Device_getSourceVolume, METH_O,
	 "Gets the volume of a source."
	},
	{"setLoopCount", (PyCFunction)Device_setLoopCount, METH_VARARGS,
	 "Sets the loop count of a source."
	},
	{"setPitch", (PyCFunction)Device_setPitch, METH_VARARGS,
	 "Sets the pitch of a source."
	},
	{"play3D", (PyCFunction)Device_play3D, METH_VARARGS | METH_KEYWORDS,
	 "Plays a sound 3 dimensional if possible."
	},
	{"updateListener", (PyCFunction)Device_updateListener, METH_VARARGS,
	 "Updates the listener's position, velocity and orientation."
	},
	{"updateSource", (PyCFunction)Device_updateSource, METH_VARARGS,
	 "Updates the soucre's position, velocity and orientation."
	},
	{"isRelative", (PyCFunction)Device_isRelative, METH_O,
	 "Checks whether the source's position is relative or absolute to the listener."
	},
	{"setRelative", (PyCFunction)Device_setRelative, METH_VARARGS,
	 "Sets whether the source's position is relative or absolute to the listener."
	},
	{"getMinGain", (PyCFunction)Device_getMinGain, METH_O,
	 "Gets the minimum gain of a source."
	},
	{"setMinGain", (PyCFunction)Device_setMinGain, METH_VARARGS,
	 "Sets the minimum gain of a source."
	},
	{"getMaxGain", (PyCFunction)Device_getMaxGain, METH_O,
	 "Gets the maximum gain of a source."
	},
	{"setMaxGain", (PyCFunction)Device_setMaxGain, METH_VARARGS,
	 "Sets the maximum gain of a source."
	},
	{"getReferenceDistance", (PyCFunction)Device_getReferenceDistance, METH_O,
	 "Gets the reference distance of a source."
	},
	{"setReferenceDistance", (PyCFunction)Device_setReferenceDistance, METH_VARARGS,
	 "Sets the reference distance of a source."
	},
	{"getMaxDistance", (PyCFunction)Device_getMaxDistance, METH_O,
	 "Gets the maximum distance of a source."
	},
	{"setMaxDistance", (PyCFunction)Device_setMaxDistance, METH_VARARGS,
	 "Sets the maximum distance of a source."
	},
	{"getRolloffFactor", (PyCFunction)Device_getRolloffFactor, METH_O,
	 "Gets the rolloff factor of a source."
	},
	{"setRolloffFactor", (PyCFunction)Device_setRolloffFactor, METH_VARARGS,
	 "Sets the rolloff factor of a source."
	},
	{"getConeInnerAngle", (PyCFunction)Device_getConeInnerAngle, METH_O,
	 "Gets the cone inner angle of a source."
	},
	{"setConeInnerAngle", (PyCFunction)Device_setConeInnerAngle, METH_VARARGS,
	 "Sets the cone inner angle of a source."
	},
	{"getConeOuterAngle", (PyCFunction)Device_getConeOuterAngle, METH_O,
	 "Gets the cone outer angle of a source."
	},
	{"setConeOuterAngle", (PyCFunction)Device_setConeOuterAngle, METH_VARARGS,
	 "Sets the cone outer angle of a source."
	},
	{"getConeOuterGain", (PyCFunction)Device_getConeOuterGain, METH_O,
	 "Gets the cone outer gain of a source."
	},
	{"setConeOuterGain", (PyCFunction)Device_setConeOuterGain, METH_VARARGS,
	 "Sets the cone outer gain of a source."
	},
	{"OpenAL", (PyCFunction)Device_OpenAL, METH_VARARGS | METH_STATIC | METH_KEYWORDS,
	 "Creates an OpenAL device."
	},
	{"SDL", (PyCFunction)Device_SDL, METH_VARARGS | METH_STATIC | METH_KEYWORDS,
	 "Creates an SDL device."
	},
	{"Jack", (PyCFunction)Device_Jack, METH_VARARGS | METH_STATIC | METH_KEYWORDS,
	 "Creates an Jack device."
	},
	{"Null", (PyCFunction)Device_Null, METH_NOARGS | METH_STATIC,
	 "Creates an Null device."
	},
	{NULL}  /* Sentinel */
};

static PyObject *
Device_getRate(Device *self, void* nothing)
{
	try
	{
		AUD_DeviceSpecs specs = self->device->getSpecs();
		return Py_BuildValue("i", specs.rate);
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve device stats!");
		return NULL;
	}
}

static PyObject *
Device_getFormat(Device *self, void* nothing)
{
	try
	{
		AUD_DeviceSpecs specs = self->device->getSpecs();
		return Py_BuildValue("i", specs.format);
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve device stats!");
		return NULL;
	}
}

static PyObject *
Device_getChannels(Device *self, void* nothing)
{
	try
	{
		AUD_DeviceSpecs specs = self->device->getSpecs();
		return Py_BuildValue("i", specs.channels);
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve device stats!");
		return NULL;
	}
}

static PyObject *
Device_getVolume(Device *self, void* nothing)
{
	try
	{
		float volume = 0.0;
		if(self->device->getCapability(AUD_CAPS_VOLUME, &volume))
			return Py_BuildValue("f", volume);
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve device volume!");
		return NULL;
	}

	Py_RETURN_NAN;
}

static int
Device_setVolume(Device *self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f", &volume))
		return -1;

	try
	{
		if(self->device->setCapability(AUD_CAPS_VOLUME, &volume))
			return 0;
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set device volume!");
	}

	return -1;
}

static PyObject *
Device_getSpeedOfSound(Device *self, void* nothing)
{
	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSetting(AUD_3DS_SPEED_OF_SOUND));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve device speed of sound!");
		return NULL;
	}
}

static int
Device_setSpeedOfSound(Device *self, PyObject* args, void* nothing)
{
	float speed;

	if(!PyArg_Parse(args, "f", &speed))
		return -1;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSetting(AUD_3DS_SPEED_OF_SOUND, speed);
			return 0;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set device speed of sound!");
	}

	return -1;
}

static PyObject *
Device_getDopplerFactor(Device *self, void* nothing)
{
	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("f", device->getSetting(AUD_3DS_DOPPLER_FACTOR));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve device doppler factor!");
		return NULL;
	}
}

static int
Device_setDopplerFactor(Device *self, PyObject* args, void* nothing)
{
	float factor;

	if(!PyArg_Parse(args, "f", &factor))
		return -1;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSetting(AUD_3DS_DOPPLER_FACTOR, factor);
			return 0;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set device doppler factor!");
	}

	return -1;
}

static PyObject *
Device_getDistanceModel(Device *self, void* nothing)
{
	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			return Py_BuildValue("i", int(device->getSetting(AUD_3DS_DISTANCE_MODEL)));
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
			return NULL;
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't retrieve device distance model!");
		return NULL;
	}
}

static int
Device_setDistanceModel(Device *self, PyObject* args, void* nothing)
{
	int model;

	if(!PyArg_Parse(args, "i", &model))
		return -1;

	try
	{
		AUD_I3DDevice* device = dynamic_cast<AUD_I3DDevice*>(self->device);
		if(device)
		{
			device->setSetting(AUD_3DS_DISTANCE_MODEL, model);
			return 0;
		}
		else
		{
			PyErr_SetString(AUDError, "Device is not a 3D device!");
		}
	}
	catch(AUD_Exception&)
	{
		PyErr_SetString(AUDError, "Couldn't set device distance model!");
	}

	return -1;
}

static PyGetSetDef Device_properties[] = {
	{"rate", (getter)Device_getRate, NULL,
	 "The sampling rate of the device in Hz.", NULL },
	{"format", (getter)Device_getFormat, NULL,
	 "The native sample format of the device.", NULL },
	{"channels", (getter)Device_getChannels, NULL,
	 "The channel count of the device.", NULL },
	{"volume", (getter)Device_getVolume, (setter)Device_setVolume,
	 "The overall volume of the device.", NULL },
	{"speedofsound", (getter)Device_getSpeedOfSound, (setter)Device_setSpeedOfSound,
	 "The speed of sound of the device.", NULL },
	{"dopplerfactor", (getter)Device_getDopplerFactor, (setter)Device_setDopplerFactor,
	 "The doppler factor of the device.", NULL },
	{"distancemodel", (getter)Device_getDistanceModel, (setter)Device_setDistanceModel,
	 "The distance model of the device.", NULL },
	{NULL}  /* Sentinel */
};

static PyTypeObject DeviceType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"aud.Device",              /* tp_name */
	sizeof(Device),            /* tp_basicsize */
	0,                         /* tp_itemsize */
	(destructor)Device_dealloc,/* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_reserved */
	0,                         /* tp_repr */
	0,                         /* tp_as_number */
	0,                         /* tp_as_sequence */
	0,                         /* tp_as_mapping */
	0,                         /* tp_hash  */
	0,                         /* tp_call */
	0,                         /* tp_str */
	0,                         /* tp_getattro */
	0,                         /* tp_setattro */
	0,                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,        /* tp_flags */
	"Device object",           /* tp_doc */
	0,		                   /* tp_traverse */
	0,		                   /* tp_clear */
	0,		                   /* tp_richcompare */
	0,		                   /* tp_weaklistoffset */
	0,		                   /* tp_iter */
	0,		                   /* tp_iternext */
	Device_methods,            /* tp_methods */
	0,                         /* tp_members */
	Device_properties,         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,                         /* tp_init */
	0,                         /* tp_alloc */
	0,                         /* tp_new */
};

static PyObject *
Device_OpenAL(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
#ifdef WITH_OPENAL
	int buffersize = AUD_DEFAULT_BUFFER_SIZE;
	int frequency = AUD_RATE_44100;

	static char *kwlist[] = {"frequency", "buffersize", NULL};

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &frequency, &buffersize))
		return NULL;

	if(buffersize < 128)
	{
		PyErr_SetString(PyExc_ValueError, "buffersize must be greater than 127!");
		return NULL;
	}

	Device *self;

	self = (Device*)DeviceType.tp_alloc(&DeviceType, 0);
	if(self != NULL)
	{
		try
		{
			AUD_DeviceSpecs specs;
			specs.rate = static_cast<AUD_SampleRate>(frequency);
			specs.channels = AUD_CHANNELS_STEREO;
			specs.format = AUD_FORMAT_S16;
			self->device = new AUD_OpenALDevice(specs, buffersize);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "OpenAL device couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
#else
	PyErr_SetString(AUDError, "OpenAL device couldn't be created!");
	return NULL;
#endif
}

static PyObject *
Device_SDL(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
#ifdef WITH_SDL
	int buffersize = AUD_DEFAULT_BUFFER_SIZE;
	int frequency = AUD_RATE_44100;

	static char *kwlist[] = {"frequency", "buffersize", NULL};

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &frequency, &buffersize))
		return NULL;

	if(buffersize < 128)
	{
		PyErr_SetString(PyExc_ValueError, "buffersize must be greater than 127!");
		return NULL;
	}

	Device *self;

	self = (Device*)DeviceType.tp_alloc(&DeviceType, 0);
	if(self != NULL)
	{
		try
		{
			AUD_DeviceSpecs specs;
			specs.rate = static_cast<AUD_SampleRate>(frequency);
			specs.channels = AUD_CHANNELS_STEREO;
			specs.format = AUD_FORMAT_S16;
			self->device = new AUD_SDLDevice(specs, buffersize);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "SDL device couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
#else
	PyErr_SetString(AUDError, "SDL device couldn't be created!");
	return NULL;
#endif
}

static PyObject *
Device_Jack(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
#ifdef WITH_JACK
	int buffersize = AUD_DEFAULT_BUFFER_SIZE;
	int channels = AUD_CHANNELS_STEREO;

	static char *kwlist[] = {"channels", "buffersize", NULL};

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &channels, &buffersize))
		return NULL;

	if(buffersize < 128)
	{
		PyErr_SetString(PyExc_ValueError, "buffersize must be greater than 127!");
		return NULL;
	}

	Device *self;

	self = (Device*)DeviceType.tp_alloc(&DeviceType, 0);
	if(self != NULL)
	{
		try
		{
			AUD_DeviceSpecs specs;
			specs.rate = AUD_RATE_44100;
			specs.channels = static_cast<AUD_Channels>(channels);
			specs.format = AUD_FORMAT_FLOAT32;
			self->device = new AUD_JackDevice(specs, buffersize);
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Jack device couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
#else
	PyErr_SetString(AUDError, "Jack device couldn't be created!");
	return NULL;
#endif
}

static PyObject *
Device_Null(PyTypeObject *type)
{
	Device *self;

	self = (Device*)DeviceType.tp_alloc(&DeviceType, 0);
	if(self != NULL)
	{
		try
		{
			self->device = new AUD_NULLDevice();
		}
		catch(AUD_Exception&)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Null device couldn't be created!");
			return NULL;
		}
	}

	return (PyObject *)self;
}

PyObject *
Device_empty()
{
	return DeviceType.tp_alloc(&DeviceType, 0);
}

// ====================================================================

static struct PyModuleDef audmodule = {
	PyModuleDef_HEAD_INIT,
	"aud",   /* name of module */
	NULL,    /* module documentation, may be NULL */
	-1,      /* size of per-interpreter state of the module,
			   or -1 if the module keeps state in global variables. */
   NULL, NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_aud(void)
{
	PyObject* m;

	if(PyType_Ready(&SoundType) < 0)
		return NULL;

	if(PyType_Ready(&DeviceType) < 0)
		return NULL;

	if(PyType_Ready(&HandleType) < 0)
		return NULL;

	m = PyModule_Create(&audmodule);
	if(m == NULL)
		return NULL;

	Py_INCREF(&SoundType);
	PyModule_AddObject(m, "Sound", (PyObject*)&SoundType);

	Py_INCREF(&DeviceType);
	PyModule_AddObject(m, "Device", (PyObject*)&DeviceType);

	Py_INCREF(&HandleType);
	PyModule_AddObject(m, "Handle", (PyObject*)&HandleType);

	AUDError = PyErr_NewException("aud.error", NULL, NULL);
	Py_INCREF(AUDError);
	PyModule_AddObject(m, "error", AUDError);

	// format constants
	PY_MODULE_ADD_CONSTANT(m, AUD_FORMAT_FLOAT32);
	PY_MODULE_ADD_CONSTANT(m, AUD_FORMAT_FLOAT64);
	PY_MODULE_ADD_CONSTANT(m, AUD_FORMAT_INVALID);
	PY_MODULE_ADD_CONSTANT(m, AUD_FORMAT_S16);
	PY_MODULE_ADD_CONSTANT(m, AUD_FORMAT_S24);
	PY_MODULE_ADD_CONSTANT(m, AUD_FORMAT_S32);
	PY_MODULE_ADD_CONSTANT(m, AUD_FORMAT_U8);
	// status constants
	PY_MODULE_ADD_CONSTANT(m, AUD_STATUS_INVALID);
	PY_MODULE_ADD_CONSTANT(m, AUD_STATUS_PAUSED);
	PY_MODULE_ADD_CONSTANT(m, AUD_STATUS_PLAYING);
	// distance model constants
	PY_MODULE_ADD_CONSTANT(m, AUD_DISTANCE_MODEL_EXPONENT);
	PY_MODULE_ADD_CONSTANT(m, AUD_DISTANCE_MODEL_EXPONENT_CLAMPED);
	PY_MODULE_ADD_CONSTANT(m, AUD_DISTANCE_MODEL_INVERSE);
	PY_MODULE_ADD_CONSTANT(m, AUD_DISTANCE_MODEL_INVERSE_CLAMPED);
	PY_MODULE_ADD_CONSTANT(m, AUD_DISTANCE_MODEL_LINEAR);
	PY_MODULE_ADD_CONSTANT(m, AUD_DISTANCE_MODEL_LINEAR_CLAMPED);
	PY_MODULE_ADD_CONSTANT(m, AUD_DISTANCE_MODEL_NONE);

	return m;
}
