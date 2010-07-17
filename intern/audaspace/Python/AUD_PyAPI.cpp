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

// ====================================================================

#define PY_MODULE_ADD_CONSTANT(module, name) PyModule_AddIntConstant(module, #name, name)

static inline int APyArg_ParseTupleAndKeywords(PyObject * o1, PyObject * o2, const char * f, const char ** k, ...)
{
	va_list ap;
	va_start(ap, k);
	int result = PyArg_ParseTupleAndKeywords(o1, o2, f, const_cast<char **>(k), ap);
	va_end(ap);
	return result;
}

static inline PyObject* APyObject_CallMethod(PyObject *o, const char *m, const char *f, ...)
{
	va_list ap;
	va_start(ap, f);
	PyObject* result = PyObject_CallMethod(o, const_cast<char *>(m), const_cast<char *>(f), ap);
	va_end(ap);
	return result;
}

// ====================================================================

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
		static const char *kwlist[] = {"filename", NULL};
		const char* filename = NULL;

		if(!APyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &filename))
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

PyDoc_STRVAR(M_aud_Sound_sine_doc,
			 "Creates a sine sound at a specific frequency.");

static PyObject *
Sound_sine(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_file_doc,
			 "Creates a sound object of a sound file.");

static PyObject *
Sound_file(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_lowpass_doc,
			 "Creates a lowpass filter with a specific cut off frequency.");

static PyObject *
Sound_lowpass(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_delay_doc,
			 "Delays a sound by a specific amount of seconds.");

static PyObject *
Sound_delay(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_double_doc,
			 "Plays two sounds of the same specs in sequence.");

static PyObject *
Sound_double(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_highpass_doc,
			 "Creates a highpass filter with a specific cut off frequency.");

static PyObject *
Sound_highpass(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_limiter_doc,
			 "Limits a sound within a specific start and end time.");

static PyObject *
Sound_limiter(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_pitch_doc,
			 "Changes the pitch of a sound with a specific factor.");

static PyObject *
Sound_pitch(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_volume_doc,
			 "Changes the volume of a sound with a specific factor.");

static PyObject *
Sound_volume(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_fadein_doc,
			 "Fades a sound in from a specific start time and with a specific length.");

static PyObject *
Sound_fadein(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_fadeout_doc,
			 "Fades a sound out from a specific start time and with a specific length.");

static PyObject *
Sound_fadeout(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_loop_doc,
			 "Loops a sound a specific amount of times, negative values mean endlessly.");

static PyObject *
Sound_loop(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_superpose_doc,
			 "Mixes two sounds of the same specs.");

static PyObject *
Sound_superpose(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_pingpong_doc,
			 "Plays a sound forward and then backward.");

static PyObject *
Sound_pingpong(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_reverse_doc,
			 "Plays a sound reversed.");

static PyObject *
Sound_reverse(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_buffer_doc,
			 "Buffers a sound into RAM.");

static PyObject *
Sound_buffer(PyObject* nothing, PyObject* args);

PyDoc_STRVAR(M_aud_Sound_square_doc,
			 "Makes a square wave out of an audio wave depending on a threshold value.");

static PyObject *
Sound_square(PyObject* nothing, PyObject* args);

static PyMethodDef Sound_methods[] = {
	{"sine", (PyCFunction)Sound_sine, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_sine_doc
	},
	{"file", (PyCFunction)Sound_file, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_file_doc
	},
	{"lowpass", (PyCFunction)Sound_lowpass, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_lowpass_doc
	},
	{"delay", (PyCFunction)Sound_delay, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_delay_doc
	},
	{"double", (PyCFunction)Sound_double, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_double_doc
	},
	{"highpass", (PyCFunction)Sound_highpass, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_highpass_doc
	},
	{"limiter", (PyCFunction)Sound_limiter, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_limiter_doc
	},
	{"pitch", (PyCFunction)Sound_pitch, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_pitch_doc
	},
	{"volume", (PyCFunction)Sound_volume, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_volume_doc
	},
	{"fadein", (PyCFunction)Sound_fadein, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_fadein_doc
	},
	{"fadeout", (PyCFunction)Sound_fadeout, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_fadeout_doc
	},
	{"loop", (PyCFunction)Sound_loop, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_loop_doc
	},
	{"superpose", (PyCFunction)Sound_superpose, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_superpose_doc
	},
	{"pingpong", (PyCFunction)Sound_pingpong, METH_O | METH_STATIC,
	 M_aud_Sound_pingpong_doc
	},
	{"reverse", (PyCFunction)Sound_reverse, METH_O | METH_STATIC,
	 M_aud_Sound_reverse_doc
	},
	{"buffer", (PyCFunction)Sound_buffer, METH_O | METH_STATIC,
	 M_aud_Sound_buffer_doc
	},
	{"square", (PyCFunction)Sound_square, METH_VARARGS | METH_STATIC,
	 M_aud_Sound_square_doc
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

// ========== Handle ==================================================

static void
Handle_dealloc(Handle* self)
{
	Py_XDECREF(self->device);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

PyDoc_STRVAR(M_aud_Handle_pause_doc,
			 "Pauses the sound.");

static PyObject *
Handle_pause(Handle *self)
{
	return APyObject_CallMethod(self->device, "_pause", "(O)", self);
}

PyDoc_STRVAR(M_aud_Handle_resume_doc,
			 "Resumes the sound.");

static PyObject *
Handle_resume(Handle *self)
{
	return APyObject_CallMethod(self->device, "_resume", "(O)", self);
}

PyDoc_STRVAR(M_aud_Handle_stop_doc,
			 "Stops the sound.");

static PyObject *
Handle_stop(Handle *self)
{
	return APyObject_CallMethod(self->device, "_stop", "(O)", self);
}

PyDoc_STRVAR(M_aud_Handle_update_doc,
			 "Updates the 3D information of the source. Awaits a 3D position and velocity vector and a 3x3 orientation matrix.");

static PyObject *
Handle_update(Handle *self, PyObject *data)
{
	return APyObject_CallMethod(self->device, "_update_source", "(OO)", self, data);
}

static PyMethodDef Handle_methods[] = {
	{"pause", (PyCFunction)Handle_pause, METH_NOARGS,
	 M_aud_Handle_pause_doc
	},
	{"resume", (PyCFunction)Handle_resume, METH_NOARGS,
	 M_aud_Handle_resume_doc
	},
	{"stop", (PyCFunction)Handle_stop, METH_NOARGS,
	 M_aud_Handle_stop_doc
	},
	{"update", (PyCFunction)Handle_update, METH_O,
	 M_aud_Handle_update_doc
	},
	{NULL}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Handle_position_doc,
			 "The playback position of the sound.");

static PyObject *
Handle_get_position(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_position", "(O)", self);
}

static int
Handle_set_position(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_seek", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_keep_doc,
			 "Whether the sound should be kept paused in the device when it's end is reached.");

static int
Handle_set_keep(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_keep", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_status_doc,
			 "Whether the sound is playing, paused or stopped.");

static PyObject *
Handle_get_status(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_status", "(O)", self);
}

PyDoc_STRVAR(M_aud_Handle_volume_doc,
			 "The volume of the sound.");

static PyObject *
Handle_get_volume(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_volume", "(O)", self);
}

static int
Handle_set_volume(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_volume", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_pitch_doc,
			 "The pitch of the sound.");

static int
Handle_set_pitch(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_pitch", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_loop_count_doc,
			 "The loop count of the sound. A negative value indicates infinity.");

static int
Handle_set_loop_count(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_loop_count", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_relative_doc,
			 "Whether the source's position is relative or absolute to the listener.");

static PyObject *
Handle_get_relative(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_is_relative", "(O)", self);
}

static int
Handle_set_relative(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_relative", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_min_gain_doc,
			 "The minimum gain of the source.");

static PyObject *
Handle_get_min_gain(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_min_gain", "(O)", self);
}

static int
Handle_set_min_gain(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_min_gain", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_max_gain_doc,
			 "The maximum gain of the source.");

static PyObject *
Handle_get_max_gain(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_max_gain", "(O)", self);
}

static int
Handle_set_max_gain(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_max_gain", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_reference_distance_doc,
			 "The reference distance of the source.");

static PyObject *
Handle_get_reference_distance(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_reference_distance", "(O)", self);
}

static int
Handle_set_reference_distance(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_reference_distance", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_max_distance_doc,
			 "The maximum distance of the source.");

static PyObject *
Handle_get_max_distance(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_max_distance", "(O)", self);
}

static int
Handle_set_max_distance(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_max_distance", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_rolloff_factor_doc,
			 "The rolloff factor of the source.");

static PyObject *
Handle_get_rolloff_factor(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_rolloff_factor", "(O)", self);
}

static int
Handle_set_rolloff_factor(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_rolloff_factor", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_cone_inner_angle_doc,
			 "The cone inner angle of the source.");

static PyObject *
Handle_get_cone_inner_angle(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_cone_inner_angle", "(O)", self);
}

static int
Handle_set_cone_inner_angle(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_cone_inner_angle", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_cone_outer_angle_doc,
			 "The cone outer angle of the source.");

static PyObject *
Handle_get_cone_outer_angle(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_cone_outer_angle", "(O)", self);
}

static int
Handle_set_cone_outer_angle(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_cone_outer_angle", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

PyDoc_STRVAR(M_aud_Handle_cone_outer_gain_doc,
			 "The cone outer gain of the source.");

static PyObject *
Handle_get_cone_outer_gain(Handle *self, void* nothing)
{
	return APyObject_CallMethod(self->device, "_get_cone_outer_gain", "(O)", self);
}

static int
Handle_set_cone_outer_gain(Handle *self, PyObject* args, void* nothing)
{
	PyObject* result = APyObject_CallMethod(self->device, "_set_cone_outer_gain", "(OO)", self, args);
	if(result)
	{
		Py_DECREF(result);
		return 0;
	}
	return -1;
}

static PyGetSetDef Handle_properties[] = {
	{(char*)"position", (getter)Handle_get_position, (setter)Handle_set_position,
	 M_aud_Handle_position_doc, NULL },
	{(char*)"keep", NULL, (setter)Handle_set_keep,
	 M_aud_Handle_keep_doc, NULL },
	{(char*)"status", (getter)Handle_get_status, NULL,
	 M_aud_Handle_status_doc, NULL },
	{(char*)"volume", (getter)Handle_get_volume, (setter)Handle_set_volume,
	 M_aud_Handle_volume_doc, NULL },
	{(char*)"pitch", NULL, (setter)Handle_set_pitch,
	 M_aud_Handle_pitch_doc, NULL },
	{(char*)"loop_count", NULL, (setter)Handle_set_loop_count,
	 M_aud_Handle_loop_count_doc, NULL },
	{(char*)"relative", (getter)Handle_get_relative, (setter)Handle_set_relative,
	 M_aud_Handle_relative_doc, NULL },
	{(char*)"min_gain", (getter)Handle_get_min_gain, (setter)Handle_set_min_gain,
	 M_aud_Handle_min_gain_doc, NULL },
	{(char*)"max_gain", (getter)Handle_get_max_gain, (setter)Handle_set_max_gain,
	 M_aud_Handle_max_gain_doc, NULL },
	{(char*)"reference_distance", (getter)Handle_get_reference_distance, (setter)Handle_set_reference_distance,
	 M_aud_Handle_reference_distance_doc, NULL },
	{(char*)"max_distance", (getter)Handle_get_max_distance, (setter)Handle_set_max_distance,
	 M_aud_Handle_max_distance_doc, NULL },
	{(char*)"rolloff_factor", (getter)Handle_get_rolloff_factor, (setter)Handle_set_rolloff_factor,
	 M_aud_Handle_rolloff_factor_doc, NULL },
	{(char*)"cone_inner_angle", (getter)Handle_get_cone_inner_angle, (setter)Handle_set_cone_inner_angle,
	 M_aud_Handle_cone_inner_angle_doc, NULL },
	{(char*)"cone_outer_angle", (getter)Handle_get_cone_outer_angle, (setter)Handle_set_cone_outer_angle,
	 M_aud_Handle_cone_outer_angle_doc, NULL },
	{(char*)"cone_outer_gain", (getter)Handle_get_cone_outer_gain, (setter)Handle_set_cone_outer_gain,
	 M_aud_Handle_cone_outer_gain_doc, NULL },
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

PyDoc_STRVAR(M_aud_Device_play_doc,
			 "Plays a sound.");

static PyObject *
Device_play(Device *self, PyObject *args, PyObject *kwds)
{
	PyObject* object;
	PyObject* keepo = NULL;

	bool keep = false;

	static const char *kwlist[] = {"sound", "keep", NULL};

	if(!APyArg_ParseTupleAndKeywords(args, kwds, "O|O", kwlist, &object, &keepo))
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
Device_set_keep(Device *self, PyObject *args)
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
Device_get_position(Device *self, PyObject *object)
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
Device_get_status(Device *self, PyObject *object)
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

PyDoc_STRVAR(M_aud_Device_lock_doc,
			 "Locks the sound device.");

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

PyDoc_STRVAR(M_aud_Device_unlock_doc,
			 "Unlocks the sound device.");

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
Device_set_source_volume(Device *self, PyObject *args)
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
Device_get_source_volume(Device *self, PyObject *object)
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
Device_set_loop_count(Device *self, PyObject *args)
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
Device_set_pitch(Device *self, PyObject *args)
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

PyDoc_STRVAR(M_aud_Device_play3D_doc,
			 "Plays a sound 3 dimensional if possible.");

static PyObject *
Device_play3D(Device *self, PyObject *args, PyObject *kwds)
{
	PyObject* object;
	PyObject* keepo = NULL;

	bool keep = false;

	static const char *kwlist[] = {"sound", "keep", NULL};

	if(!APyArg_ParseTupleAndKeywords(args, kwds, "O|O", kwlist, &object, &keepo))
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

PyDoc_STRVAR(M_aud_Device_update_listener_doc,
			 "Updates the listener's position, velocity and orientation.");

static PyObject *
Device_update_listener(Device *self, PyObject *args)
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
Device_update_source(Device *self, PyObject *args)
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
Device_is_relative(Device *self, PyObject *object)
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
Device_set_relative(Device *self, PyObject *args)
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
Device_get_min_gain(Device *self, PyObject *object)
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
Device_set_min_gain(Device *self, PyObject *args)
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
Device_get_max_gain(Device *self, PyObject *object)
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
Device_set_max_gain(Device *self, PyObject *args)
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
Device_get_reference_distance(Device *self, PyObject *object)
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
Device_set_reference_distance(Device *self, PyObject *args)
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
Device_get_max_distance(Device *self, PyObject *object)
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
Device_set_max_distance(Device *self, PyObject *args)
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
Device_get_rolloff_factor(Device *self, PyObject *object)
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
Device_set_rolloff_factor(Device *self, PyObject *args)
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
Device_get_cone_inner_angle(Device *self, PyObject *object)
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
Device_set_cone_inner_angle(Device *self, PyObject *args)
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
Device_get_cone_outer_angle(Device *self, PyObject *object)
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
Device_set_cone_outer_angle(Device *self, PyObject *args)
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
Device_get_cone_outer_gain(Device *self, PyObject *object)
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
Device_set_cone_outer_gain(Device *self, PyObject *args)
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

PyDoc_STRVAR(M_aud_Device_OpenAL_doc,
			 "Creates an OpenAL device.");

static PyObject *
Device_OpenAL(PyTypeObject *type, PyObject *args, PyObject *kwds);

PyDoc_STRVAR(M_aud_Device_SDL_doc,
			 "Creates an SDL device.");

static PyObject *
Device_SDL(PyTypeObject *type, PyObject *args, PyObject *kwds);

PyDoc_STRVAR(M_aud_Device_Jack_doc,
			 "Creates an Jack device.");

static PyObject *
Device_Jack(PyTypeObject *type, PyObject *args, PyObject *kwds);

PyDoc_STRVAR(M_aud_Device_Null_doc,
			 "Creates an Null device.");

static PyObject *
Device_Null(PyTypeObject *type);

static PyMethodDef Device_methods[] = {
	{"play", (PyCFunction)Device_play, METH_VARARGS | METH_KEYWORDS,
	 M_aud_Device_play_doc
	},
	{"_stop", (PyCFunction)Device_stop, METH_O,
	 ""
	},
	{"_pause", (PyCFunction)Device_pause, METH_O,
	 ""
	},
	{"_resume", (PyCFunction)Device_resume, METH_O,
	 ""
	},
	{"_set_keep", (PyCFunction)Device_set_keep, METH_VARARGS,
	 ""
	},
	{"_seek", (PyCFunction)Device_seek, METH_VARARGS,
	 ""
	},
	{"_get_position", (PyCFunction)Device_get_position, METH_O,
	 ""
	},
	{"_get_status", (PyCFunction)Device_get_status, METH_O,
	 ""
	},
	{"lock", (PyCFunction)Device_lock, METH_NOARGS,
	 M_aud_Device_lock_doc
	},
	{"unlock", (PyCFunction)Device_unlock, METH_NOARGS,
	 M_aud_Device_unlock_doc
	},
	{"_set_volume", (PyCFunction)Device_set_source_volume, METH_VARARGS,
	 ""
	},
	{"_get_volume", (PyCFunction)Device_get_source_volume, METH_O,
	 ""
	},
	{"_set_loop_count", (PyCFunction)Device_set_loop_count, METH_VARARGS,
	 ""
	},
	{"_set_pitch", (PyCFunction)Device_set_pitch, METH_VARARGS,
	 ""
	},
	{"play3D", (PyCFunction)Device_play3D, METH_VARARGS | METH_KEYWORDS,
	 M_aud_Device_play3D_doc
	},
	{"update_listener", (PyCFunction)Device_update_listener, METH_VARARGS,
	 M_aud_Device_update_listener_doc
	},
	{"_update_source", (PyCFunction)Device_update_source, METH_VARARGS,
	 ""
	},
	{"_is_relative", (PyCFunction)Device_is_relative, METH_O,
	 ""
	},
	{"_set_relative", (PyCFunction)Device_set_relative, METH_VARARGS,
	 ""
	},
	{"_get_min_gain", (PyCFunction)Device_get_min_gain, METH_O,
	 ""
	},
	{"_set_min_gain", (PyCFunction)Device_set_min_gain, METH_VARARGS,
	 ""
	},
	{"_get_max_gain", (PyCFunction)Device_get_max_gain, METH_O,
	 ""
	},
	{"_set_max_gain", (PyCFunction)Device_set_max_gain, METH_VARARGS,
	 ""
	},
	{"_get_reference_distance", (PyCFunction)Device_get_reference_distance, METH_O,
	 ""
	},
	{"_set_reference_distance", (PyCFunction)Device_set_reference_distance, METH_VARARGS,
	 ""
	},
	{"_get_max_distance", (PyCFunction)Device_get_max_distance, METH_O,
	 ""
	},
	{"_set_max_distance", (PyCFunction)Device_set_max_distance, METH_VARARGS,
	 ""
	},
	{"_get_rolloff_factor", (PyCFunction)Device_get_rolloff_factor, METH_O,
	 ""
	},
	{"_set_rolloff_factor", (PyCFunction)Device_set_rolloff_factor, METH_VARARGS,
	 ""
	},
	{"_get_cone_inner_angle", (PyCFunction)Device_get_cone_inner_angle, METH_O,
	 ""
	},
	{"_set_cone_inner_angle", (PyCFunction)Device_set_cone_inner_angle, METH_VARARGS,
	 ""
	},
	{"_get_cone_outer_angle", (PyCFunction)Device_get_cone_outer_angle, METH_O,
	 ""
	},
	{"_set_cone_outer_angle", (PyCFunction)Device_set_cone_outer_angle, METH_VARARGS,
	 ""
	},
	{"_get_cone_outer_gain", (PyCFunction)Device_get_cone_outer_gain, METH_O,
	 ""
	},
	{"_set_cone_outer_gain", (PyCFunction)Device_set_cone_outer_gain, METH_VARARGS,
	 ""
	},
	{"OpenAL", (PyCFunction)Device_OpenAL, METH_VARARGS | METH_STATIC | METH_KEYWORDS,
	 M_aud_Device_OpenAL_doc
	},
	{"SDL", (PyCFunction)Device_SDL, METH_VARARGS | METH_STATIC | METH_KEYWORDS,
	 M_aud_Device_SDL_doc
	},
	{"Jack", (PyCFunction)Device_Jack, METH_VARARGS | METH_STATIC | METH_KEYWORDS,
	 M_aud_Device_Jack_doc
	},
	{"Null", (PyCFunction)Device_Null, METH_NOARGS | METH_STATIC,
	 M_aud_Device_Null_doc
	},
	{NULL}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Device_rate_doc,
			 "The sampling rate of the device in Hz.");

static PyObject *
Device_get_rate(Device *self, void* nothing)
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

PyDoc_STRVAR(M_aud_Device_format_doc,
			 "The native sample format of the device.");

static PyObject *
Device_get_format(Device *self, void* nothing)
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

PyDoc_STRVAR(M_aud_Device_channels_doc,
			 "The channel count of the device.");

static PyObject *
Device_get_channels(Device *self, void* nothing)
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

PyDoc_STRVAR(M_aud_Device_volume_doc,
			 "The overall volume of the device.");

static PyObject *
Device_get_volume(Device *self, void* nothing)
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
Device_set_volume(Device *self, PyObject* args, void* nothing)
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

PyDoc_STRVAR(M_aud_Device_speed_of_sound_doc,
			 "The speed of sound of the device.");

static PyObject *
Device_get_speed_of_sound(Device *self, void* nothing)
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
Device_set_speed_of_sound(Device *self, PyObject* args, void* nothing)
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

PyDoc_STRVAR(M_aud_Device_doppler_factor_doc,
			 "The doppler factor of the device.");

static PyObject *
Device_get_doppler_factor(Device *self, void* nothing)
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
Device_set_doppler_factor(Device *self, PyObject* args, void* nothing)
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

PyDoc_STRVAR(M_aud_Device_distance_model_doc,
			 "The distance model of the device.");

static PyObject *
Device_get_distance_model(Device *self, void* nothing)
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
Device_set_distance_model(Device *self, PyObject* args, void* nothing)
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
	{(char*)"rate", (getter)Device_get_rate, NULL,
	 M_aud_Device_rate_doc, NULL },
	{(char*)"format", (getter)Device_get_format, NULL,
	 M_aud_Device_format_doc, NULL },
	{(char*)"channels", (getter)Device_get_channels, NULL,
	 M_aud_Device_channels_doc, NULL },
	{(char*)"volume", (getter)Device_get_volume, (setter)Device_set_volume,
	 M_aud_Device_volume_doc, NULL },
	{(char*)"speed_of_sound", (getter)Device_get_speed_of_sound, (setter)Device_set_speed_of_sound,
	 M_aud_Device_speed_of_sound_doc, NULL },
	{(char*)"doppler_factor", (getter)Device_get_doppler_factor, (setter)Device_set_doppler_factor,
	 M_aud_Device_doppler_factor_doc, NULL },
	{(char*)"distance_model", (getter)Device_get_distance_model, (setter)Device_set_distance_model,
	 M_aud_Device_distance_model_doc, NULL },
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

	static const char *kwlist[] = {"frequency", "buffersize", NULL};

	if(!APyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &frequency, &buffersize))
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

	static const char *kwlist[] = {"frequency", "buffersize", NULL};

	if(!APyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &frequency, &buffersize))
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

	static const char *kwlist[] = {"channels", "buffersize", NULL};

	if(!APyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &channels, &buffersize))
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
