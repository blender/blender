/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
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

#include "PySequence.h"

#include "PySound.h"
#include "PySequenceEntry.h"

#include "sequence/AnimateableProperty.h"
#include "sequence/Sequence.h"
#include "Exception.h"

#include <vector>
#include <structmember.h>

using aud::Channels;
using aud::DistanceModel;
using aud::Exception;
using aud::ISound;
using aud::AnimateableProperty;
using aud::AnimateablePropertyType;
using aud::Specs;

extern PyObject* AUDError;

// ====================================================================

static void
Sequence_dealloc(Sequence* self)
{
	if(self->sequence)
		delete reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Sequence_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	Sequence* self;

	int channels = aud::CHANNELS_STEREO;
	double rate = aud::RATE_48000;
	float fps = 30.0f;
	bool muted = false;
	PyObject* mutedo = nullptr;

	self = (Sequence*)type->tp_alloc(type, 0);
	if(self != nullptr)
	{
		static const char* kwlist[] = {"channels", "rate", "fps", "muted", nullptr};

		if(!PyArg_ParseTupleAndKeywords(args, kwds, "|idfO:Sequence", const_cast<char**>(kwlist), &channels, &rate, &fps, &mutedo))
		{
			Py_DECREF(self);
			return nullptr;
		}

		if(mutedo)
		{
			if(!PyBool_Check(mutedo))
			{
				PyErr_SetString(PyExc_TypeError, "muted is not a boolean!");
				return nullptr;
			}

			muted = mutedo == Py_True;
		}

		aud::Specs specs;
		specs.channels = static_cast<aud::Channels>(channels);
		specs.rate = rate;

		try
		{
			self->sequence = new std::shared_ptr<aud::Sequence>(new aud::Sequence(specs, fps, muted));
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Sequence_add_doc,
			 "add()\n\n"
			 "Adds a new entry to the sequence.\n\n"
			 ":arg sound: The sound this entry should play.\n"
			 ":type sound: :class:`Sound`\n"
			 ":arg begin: The start time.\n"
			 ":type begin: float\n"
			 ":arg end: The end time or a negative value if determined by the sound.\n"
			 ":type end: float\n"
			 ":arg skip: How much seconds should be skipped at the beginning.\n"
			 ":type skip: float\n"
			 ":return: The entry added.\n"
			 ":rtype: :class:`SequenceEntry`");

static PyObject *
Sequence_add(Sequence* self, PyObject* args, PyObject* kwds)
{
	PyObject* object;
	float begin;
	float end = -1.0f;
	float skip = 0.0f;

	static const char* kwlist[] = {"sound", "begin", "end", "skip", nullptr};

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "Of|ff:add", const_cast<char**>(kwlist), &object, &begin, &end, &skip))
		return nullptr;

	Sound* sound = checkSound(object);

	if(!sound)
		return nullptr;

	SequenceEntry* entry;

	entry = (SequenceEntry*)SequenceEntry_empty();
	if(entry != nullptr)
	{
		try
		{
			entry->entry = new std::shared_ptr<aud::SequenceEntry>((*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->add(*reinterpret_cast<std::shared_ptr<ISound>*>(sound->sound), begin, end, skip));
		}
		catch(Exception& e)
		{
			Py_DECREF(entry);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)entry;
}

PyDoc_STRVAR(M_aud_Sequence_remove_doc,
			 "remove()\n\n"
			 "Removes an entry from the sequence.\n\n"
			 ":arg entry: The entry to remove.\n"
			 ":type entry: :class:`SequenceEntry`\n");

static PyObject *
Sequence_remove(Sequence* self, PyObject* args)
{
	PyObject* object;

	if(!PyArg_ParseTuple(args, "O:remove", &object))
		return nullptr;

	SequenceEntry* entry = checkSequenceEntry(object);

	if(!entry)
		return nullptr;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->remove(*reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(entry->entry));
		Py_RETURN_NONE;
	}
	catch(Exception& e)
	{
		Py_DECREF(entry);
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Sequence_setAnimationData_doc,
			 "setAnimationData()\n\n"
			 "Writes animation data to a sequence.\n\n"
			 ":arg type: The type of animation data.\n"
			 ":type type: int\n"
			 ":arg frame: The frame this data is for.\n"
			 ":type frame: int\n"
			 ":arg data: The data to write.\n"
			 ":type data: sequence of float\n"
			 ":arg animated: Whether the attribute is animated.\n"
			 ":type animated: bool");

static PyObject *
Sequence_setAnimationData(Sequence* self, PyObject* args)
{
	int type, frame;
	PyObject* py_data;
	Py_ssize_t py_data_len;
	PyObject* animatedo;
	bool animated;

	if(!PyArg_ParseTuple(args, "iiOO:setAnimationData", &type, &frame, &py_data, &animatedo))
		return nullptr;

	if(!PySequence_Check(py_data))
	{
		PyErr_SetString(PyExc_TypeError, "Parameter is not a sequence!");
		return nullptr;
	}

	py_data_len= PySequence_Size(py_data);

	std::vector<float> data;
	data.resize(py_data_len);

	PyObject* py_value;
	float value;

	for(Py_ssize_t i = 0; i < py_data_len; i++)
	{
		py_value = PySequence_GetItem(py_data, i);
		value= (float)PyFloat_AsDouble(py_value);
		Py_DECREF(py_value);

		if(value == -1.0f && PyErr_Occurred()) {
			return nullptr;
		}

		data.push_back(value);
	}

	if(!PyBool_Check(animatedo))
	{
		PyErr_SetString(PyExc_TypeError, "animated is not a boolean!");
		return nullptr;
	}

	animated = animatedo == Py_True;

	try
	{
		AnimateableProperty* prop = (*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->getAnimProperty(static_cast<AnimateablePropertyType>(type));

		if(prop->getCount() != py_data_len)
		{
			PyErr_SetString(PyExc_ValueError, "the amount of floats doesn't fit the animated property");
			return nullptr;
		}

		if(animated)
		{
			if(frame >= 0)
				prop->write(&data[0], frame, 1);
		}
		else
		{
			prop->write(&data[0]);
		}
		Py_RETURN_NONE;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static PyMethodDef Sequence_methods[] = {
	{"add", (PyCFunction)Sequence_add, METH_VARARGS | METH_KEYWORDS,
	 M_aud_Sequence_add_doc
	},
	{"remove", (PyCFunction)Sequence_remove, METH_VARARGS,
	 M_aud_Sequence_remove_doc
	},
	{"setAnimationData", (PyCFunction)Sequence_setAnimationData, METH_VARARGS,
	 M_aud_Sequence_setAnimationData_doc
	},
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Sequence_channels_doc,
			 "The channel count of the sequence.");

static PyObject *
Sequence_get_channels(Sequence* self, void* nothing)
{
	try
	{
		Specs specs = (*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->getSpecs();
		return Py_BuildValue("i", specs.channels);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Sequence_set_channels(Sequence* self, PyObject* args, void* nothing)
{
	int channels;

	if(!PyArg_Parse(args, "i:channels", &channels))
		return -1;

	try
	{
		std::shared_ptr<aud::Sequence> sequence = *reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence);
		Specs specs = sequence->getSpecs();
		specs.channels = static_cast<Channels>(channels);
		sequence->setSpecs(specs);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return -1;
	}
}

PyDoc_STRVAR(M_aud_Sequence_distance_model_doc,
			 "The distance model of the sequence.\n\n"
			 ".. seealso:: http://connect.creativelabs.com/openal/Documentation/OpenAL%201.1%20Specification.htm#_Toc199835864");

static PyObject *
Sequence_get_distance_model(Sequence* self, void* nothing)
{
	try
	{
		return Py_BuildValue("i", (*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->getDistanceModel());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Sequence_set_distance_model(Sequence* self, PyObject* args, void* nothing)
{
	int distance_model;

	if(!PyArg_Parse(args, "i:distance_model", &distance_model))
		return -1;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->setDistanceModel(static_cast<DistanceModel>(distance_model));
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return -1;
	}
}

PyDoc_STRVAR(M_aud_Sequence_doppler_factor_doc,
			 "The doppler factor of the sequence.\n"
			 "This factor is a scaling factor for the velocity vectors in "
			 "doppler calculation. So a value bigger than 1 will exaggerate "
			 "the effect as it raises the velocity.");

static PyObject *
Sequence_get_doppler_factor(Sequence* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->getDopplerFactor());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Sequence_set_doppler_factor(Sequence* self, PyObject* args, void* nothing)
{
	float factor;

	if(!PyArg_Parse(args, "f:doppler_factor", &factor))
		return -1;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->setDopplerFactor(factor);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return -1;
	}
}

PyDoc_STRVAR(M_aud_Sequence_fps_doc,
			 "The listeners's location in 3D space, a 3D tuple of floats.");

static PyObject *
Sequence_get_fps(Sequence* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->getFPS());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Sequence_set_fps(Sequence* self, PyObject* args, void* nothing)
{
	float fps;

	if(!PyArg_Parse(args, "f:fps", &fps))
		return -1;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->setFPS(fps);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return -1;
	}
}

PyDoc_STRVAR(M_aud_Sequence_muted_doc,
			 "Whether the whole sequence is muted.\n");

static PyObject *
Sequence_get_muted(Sequence* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::Sequence>* sequence = reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence);
		return PyBool_FromLong((long)(*sequence)->isMuted());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Sequence_set_muted(Sequence* self, PyObject* args, void* nothing)
{
	if(!PyBool_Check(args))
	{
		PyErr_SetString(PyExc_TypeError, "muted is not a boolean!");
		return -1;
	}

	bool muted = args == Py_True;

	try
	{
		std::shared_ptr<aud::Sequence>* sequence = reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence);
		(*sequence)->mute(muted);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Sequence_rate_doc,
			 "The sampling rate of the sequence in Hz.");

static PyObject *
Sequence_get_rate(Sequence* self, void* nothing)
{
	try
	{
		Specs specs = (*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->getSpecs();
		return Py_BuildValue("d", specs.rate);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Sequence_set_rate(Sequence* self, PyObject* args, void* nothing)
{
	double rate;

	if(!PyArg_Parse(args, "d:rate", &rate))
		return -1;

	try
	{
		std::shared_ptr<aud::Sequence> sequence = *reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence);
		Specs specs = sequence->getSpecs();
		specs.rate = rate;
		sequence->setSpecs(specs);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return -1;
	}
}

PyDoc_STRVAR(M_aud_Sequence_speed_of_sound_doc,
			 "The speed of sound of the sequence.\n"
			 "The speed of sound in air is typically 343.3 m/s.");

static PyObject *
Sequence_get_speed_of_sound(Sequence* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->getSpeedOfSound());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Sequence_set_speed_of_sound(Sequence* self, PyObject* args, void* nothing)
{
	float speed;

	if(!PyArg_Parse(args, "f:speed_of_sound", &speed))
		return -1;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::Sequence>*>(self->sequence))->setSpeedOfSound(speed);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return -1;
	}
}

static PyGetSetDef Sequence_properties[] = {
	{(char*)"channels", (getter)Sequence_get_channels, (setter)Sequence_set_channels,
	 M_aud_Sequence_channels_doc, nullptr },
	{(char*)"distance_model", (getter)Sequence_get_distance_model, (setter)Sequence_set_distance_model,
	 M_aud_Sequence_distance_model_doc, nullptr },
	{(char*)"doppler_factor", (getter)Sequence_get_doppler_factor, (setter)Sequence_set_doppler_factor,
	 M_aud_Sequence_doppler_factor_doc, nullptr },
	{(char*)"fps", (getter)Sequence_get_fps, (setter)Sequence_set_fps,
	 M_aud_Sequence_fps_doc, nullptr },
	{(char*)"muted", (getter)Sequence_get_muted, (setter)Sequence_set_muted,
	 M_aud_Sequence_muted_doc, nullptr },
	{(char*)"rate", (getter)Sequence_get_rate, (setter)Sequence_set_rate,
	 M_aud_Sequence_rate_doc, nullptr },
	{(char*)"speed_of_sound", (getter)Sequence_get_speed_of_sound, (setter)Sequence_set_speed_of_sound,
	 M_aud_Sequence_speed_of_sound_doc, nullptr },
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Sequence_doc,
			 "This sound represents sequenced entries to play a sound sequence.");

extern PyTypeObject SoundType;

static PyTypeObject SequenceType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"aud.Sequence",              /* tp_name */
	sizeof(Sequence),            /* tp_basicsize */
	0,                           /* tp_itemsize */
	(destructor)Sequence_dealloc,/* tp_dealloc */
	0,                           /* tp_print */
	0,                           /* tp_getattr */
	0,                           /* tp_setattr */
	0,                           /* tp_reserved */
	0,                           /* tp_repr */
	0,                           /* tp_as_number */
	0,                           /* tp_as_sequence */
	0,                           /* tp_as_mapping */
	0,                           /* tp_hash  */
	0,                           /* tp_call */
	0,                           /* tp_str */
	0,                           /* tp_getattro */
	0,                           /* tp_setattro */
	0,                           /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,          /* tp_flags */
	M_aud_Sequence_doc,          /* tp_doc */
	0,		                     /* tp_traverse */
	0,		                     /* tp_clear */
	0,		                     /* tp_richcompare */
	0,		                     /* tp_weaklistoffset */
	0,		                     /* tp_iter */
	0,		                     /* tp_iternext */
	Sequence_methods,            /* tp_methods */
	0,                           /* tp_members */
	Sequence_properties,         /* tp_getset */
	&SoundType,                  /* tp_base */
	0,                           /* tp_dict */
	0,                           /* tp_descr_get */
	0,                           /* tp_descr_set */
	0,                           /* tp_dictoffset */
	0,                           /* tp_init */
	0,                           /* tp_alloc */
	Sequence_new,                /* tp_new */
};

AUD_API PyObject* Sequence_empty()
{
	return SequenceType.tp_alloc(&SequenceType, 0);
}


AUD_API Sequence* checkSequence(PyObject* sequence)
{
	if(!PyObject_TypeCheck(sequence, &SequenceType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type Sequence!");
		return nullptr;
	}

	return (Sequence*)sequence;
}


bool initializeSequence()
{
	return PyType_Ready(&SequenceType) >= 0;
}


void addSequenceToModule(PyObject* module)
{
	Py_INCREF(&SequenceType);
	PyModule_AddObject(module, "Sequence", (PyObject *)&SequenceType);
}
