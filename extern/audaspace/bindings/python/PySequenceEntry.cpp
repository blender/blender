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

#include "PySequenceEntry.h"

#include "PySound.h"

#include "Exception.h"
#include "sequence/AnimateableProperty.h"
#include "sequence/SequenceEntry.h"

#include <structmember.h>
#include <vector>

using aud::Exception;
using aud::AnimateableProperty;
using aud::AnimateablePropertyType;
using aud::ISound;

extern PyObject* AUDError;

// ====================================================================

static void
SequenceEntry_dealloc(SequenceEntry* self)
{
	if(self->entry)
		delete reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(M_aud_SequenceEntry_move_doc,
			 ".. method:: move()\n\n"
			 "   Moves the entry.\n\n"
			 "   :arg begin: The new start time.\n"
			 "   :type begin: double\n"
			 "   :arg end: The new end time or a negative value if unknown.\n"
			 "   :type end: double\n"
			 "   :arg skip: How many seconds to skip at the beginning.\n"
			 "   :type skip: double\n");

static PyObject *
SequenceEntry_move(SequenceEntry* self, PyObject* args)
{
	double begin, end, skip;

	if(!PyArg_ParseTuple(args, "ddd:move", &begin, &end, &skip))
		return nullptr;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry))->move(begin, end, skip);
		Py_RETURN_NONE;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_SequenceEntry_setAnimationData_doc,
			 ".. method:: setAnimationData()\n\n"
			 "   Writes animation data to a sequenced entry.\n\n"
			 "   :arg type: The type of animation data.\n"
			 "   :type type: int\n"
			 "   :arg frame: The frame this data is for.\n"
			 "   :type frame: int\n"
			 "   :arg data: The data to write.\n"
			 "   :type data: sequence of float\n"
			 "   :arg animated: Whether the attribute is animated.\n"
			 "   :type animated: bool");

static PyObject *
SequenceEntry_setAnimationData(SequenceEntry* self, PyObject* args)
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
	data.reserve(py_data_len);

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
		AnimateableProperty* prop = (*reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry))->getAnimProperty(static_cast<AnimateablePropertyType>(type));

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

static PyMethodDef SequenceEntry_methods[] = {
	{"move", (PyCFunction)SequenceEntry_move, METH_VARARGS,
	 M_aud_SequenceEntry_move_doc
	},
	{"setAnimationData", (PyCFunction)SequenceEntry_setAnimationData, METH_VARARGS,
	 M_aud_SequenceEntry_setAnimationData_doc
	},
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_SequenceEntry_attenuation_doc,
			 "This factor is used for distance based attenuation of the "
			 "source.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
SequenceEntry_get_attenuation(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return Py_BuildValue("f", (*entry)->getAttenuation());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
SequenceEntry_set_attenuation(SequenceEntry* self, PyObject* args, void* nothing)
{
	float factor;

	if(!PyArg_Parse(args, "f:attenuation", &factor))
		return -1;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setAttenuation(factor);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_cone_angle_inner_doc,
			 "The opening angle of the inner cone of the source. If the cone "
			 "values of a source are set there are two (audible) cones with "
			 "the apex at the :attr:`location` of the source and with infinite "
			 "height, heading in the direction of the source's "
			 ":attr:`orientation`.\n"
			 "In the inner cone the volume is normal. Outside the outer cone "
			 "the volume will be :attr:`cone_volume_outer` and in the area "
			 "between the volume will be interpolated linearly.");

static PyObject *
SequenceEntry_get_cone_angle_inner(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return Py_BuildValue("f", (*entry)->getConeAngleInner());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
SequenceEntry_set_cone_angle_inner(SequenceEntry* self, PyObject* args, void* nothing)
{
	float angle;

	if(!PyArg_Parse(args, "f:cone_angle_inner", &angle))
		return -1;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setConeAngleInner(angle);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_cone_angle_outer_doc,
			 "The opening angle of the outer cone of the source.\n\n"
			 ".. seealso:: :attr:`cone_angle_inner`");

static PyObject *
SequenceEntry_get_cone_angle_outer(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return Py_BuildValue("f", (*entry)->getConeAngleOuter());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
SequenceEntry_set_cone_angle_outer(SequenceEntry* self, PyObject* args, void* nothing)
{
	float angle;

	if(!PyArg_Parse(args, "f:cone_angle_outer", &angle))
		return -1;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setConeAngleOuter(angle);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_cone_volume_outer_doc,
			 "The volume outside the outer cone of the source.\n\n"
			 ".. seealso:: :attr:`cone_angle_inner`");

static PyObject *
SequenceEntry_get_cone_volume_outer(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return Py_BuildValue("f", (*entry)->getConeVolumeOuter());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
SequenceEntry_set_cone_volume_outer(SequenceEntry* self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f:cone_volume_outer", &volume))
		return -1;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setConeVolumeOuter(volume);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_distance_maximum_doc,
			 "The maximum distance of the source.\n"
			 "If the listener is further away the source volume will be 0.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
SequenceEntry_get_distance_maximum(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return Py_BuildValue("f", (*entry)->getDistanceMaximum());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
SequenceEntry_set_distance_maximum(SequenceEntry* self, PyObject* args, void* nothing)
{
	float distance;

	if(!PyArg_Parse(args, "f:distance_maximum", &distance))
		return -1;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setDistanceMaximum(distance);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_distance_reference_doc,
			 "The reference distance of the source.\n"
			 "At this distance the volume will be exactly :attr:`volume`.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
SequenceEntry_get_distance_reference(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return Py_BuildValue("f", (*entry)->getDistanceReference());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
SequenceEntry_set_distance_reference(SequenceEntry* self, PyObject* args, void* nothing)
{
	float distance;

	if(!PyArg_Parse(args, "f:distance_reference", &distance))
		return -1;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setDistanceReference(distance);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_muted_doc,
			 "Whether the entry is muted.\n");

static PyObject *
SequenceEntry_get_muted(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return PyBool_FromLong((long)(*entry)->isMuted());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
SequenceEntry_set_muted(SequenceEntry* self, PyObject* args, void* nothing)
{
	if(!PyBool_Check(args))
	{
		PyErr_SetString(PyExc_TypeError, "muted is not a boolean!");
		return -1;
	}

	bool muted = args == Py_True;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->mute(muted);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_relative_doc,
			 "Whether the source's location, velocity and orientation is relative or absolute to the listener.");

static PyObject *
SequenceEntry_get_relative(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return PyBool_FromLong((long)(*entry)->isRelative());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return nullptr;
}

static int
SequenceEntry_set_relative(SequenceEntry* self, PyObject* args, void* nothing)
{
	if(!PyBool_Check(args))
	{
		PyErr_SetString(PyExc_TypeError, "Value is not a boolean!");
		return -1;
	}

	bool relative = (args == Py_True);

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setRelative(relative);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_sound_doc,
			 "The sound the entry is representing and will be played in the sequence.");

static PyObject *
SequenceEntry_get_sound(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		Sound* object = (Sound*) Sound_empty();
		if(object)
		{
			object->sound = new std::shared_ptr<ISound>((*entry)->getSound());
			return (PyObject *) object;
		}
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return nullptr;
}

static int
SequenceEntry_set_sound(SequenceEntry* self, PyObject* args, void* nothing)
{
	Sound* sound = checkSound(args);

	if(!sound)
		return -1;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setSound(*reinterpret_cast<std::shared_ptr<ISound>*>(sound->sound));
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_volume_maximum_doc,
			 "The maximum volume of the source.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
SequenceEntry_get_volume_maximum(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return Py_BuildValue("f", (*entry)->getVolumeMaximum());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
SequenceEntry_set_volume_maximum(SequenceEntry* self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f:volume_maximum", &volume))
		return -1;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setVolumeMaximum(volume);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_SequenceEntry_volume_minimum_doc,
			 "The minimum volume of the source.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
SequenceEntry_get_volume_minimum(SequenceEntry* self, void* nothing)
{
	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		return Py_BuildValue("f", (*entry)->getVolumeMinimum());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
SequenceEntry_set_volume_minimum(SequenceEntry* self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f:volume_minimum", &volume))
		return -1;

	try
	{
		std::shared_ptr<aud::SequenceEntry>* entry = reinterpret_cast<std::shared_ptr<aud::SequenceEntry>*>(self->entry);
		(*entry)->setVolumeMinimum(volume);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

static PyGetSetDef SequenceEntry_properties[] = {
	{(char*)"attenuation", (getter)SequenceEntry_get_attenuation, (setter)SequenceEntry_set_attenuation,
	 M_aud_SequenceEntry_attenuation_doc, nullptr },
	{(char*)"cone_angle_inner", (getter)SequenceEntry_get_cone_angle_inner, (setter)SequenceEntry_set_cone_angle_inner,
	 M_aud_SequenceEntry_cone_angle_inner_doc, nullptr },
	{(char*)"cone_angle_outer", (getter)SequenceEntry_get_cone_angle_outer, (setter)SequenceEntry_set_cone_angle_outer,
	 M_aud_SequenceEntry_cone_angle_outer_doc, nullptr },
	{(char*)"cone_volume_outer", (getter)SequenceEntry_get_cone_volume_outer, (setter)SequenceEntry_set_cone_volume_outer,
	 M_aud_SequenceEntry_cone_volume_outer_doc, nullptr },
	{(char*)"distance_maximum", (getter)SequenceEntry_get_distance_maximum, (setter)SequenceEntry_set_distance_maximum,
	 M_aud_SequenceEntry_distance_maximum_doc, nullptr },
	{(char*)"distance_reference", (getter)SequenceEntry_get_distance_reference, (setter)SequenceEntry_set_distance_reference,
	 M_aud_SequenceEntry_distance_reference_doc, nullptr },
	{(char*)"muted", (getter)SequenceEntry_get_muted, (setter)SequenceEntry_set_muted,
	 M_aud_SequenceEntry_muted_doc, nullptr },
	{(char*)"relative", (getter)SequenceEntry_get_relative, (setter)SequenceEntry_set_relative,
	 M_aud_SequenceEntry_relative_doc, nullptr },
	{(char*)"sound", (getter)SequenceEntry_get_sound, (setter)SequenceEntry_set_sound,
	 M_aud_SequenceEntry_sound_doc, nullptr },
	{(char*)"volume_maximum", (getter)SequenceEntry_get_volume_maximum, (setter)SequenceEntry_set_volume_maximum,
	 M_aud_SequenceEntry_volume_maximum_doc, nullptr },
	{(char*)"volume_minimum", (getter)SequenceEntry_get_volume_minimum, (setter)SequenceEntry_set_volume_minimum,
	 M_aud_SequenceEntry_volume_minimum_doc, nullptr },
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_SequenceEntry_doc,
			 "SequenceEntry objects represent an entry of a sequenced sound.");

static PyTypeObject SequenceEntryType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"aud.SequenceEntry",               /* tp_name */
	sizeof(SequenceEntry),             /* tp_basicsize */
	0,                                 /* tp_itemsize */
	(destructor)SequenceEntry_dealloc, /* tp_dealloc */
	0,                                 /* tp_print */
	0,                                 /* tp_getattr */
	0,                                 /* tp_setattr */
	0,                                 /* tp_reserved */
	0,                                 /* tp_repr */
	0,                                 /* tp_as_number */
	0,                                 /* tp_as_sequence */
	0,                                 /* tp_as_mapping */
	0,                                 /* tp_hash  */
	0,                                 /* tp_call */
	0,                                 /* tp_str */
	0,                                 /* tp_getattro */
	0,                                 /* tp_setattro */
	0,                                 /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,                /* tp_flags */
	M_aud_SequenceEntry_doc,           /* tp_doc */
	0,                                 /* tp_traverse */
	0,                                 /* tp_clear */
	0,                                 /* tp_richcompare */
	0,                                 /* tp_weaklistoffset */
	0,                                 /* tp_iter */
	0,                                 /* tp_iternext */
	SequenceEntry_methods,             /* tp_methods */
	0,                                 /* tp_members */
	SequenceEntry_properties,          /* tp_getset */
	0,                                 /* tp_base */
	0,                                 /* tp_dict */
	0,                                 /* tp_descr_get */
	0,                                 /* tp_descr_set */
	0,                                 /* tp_dictoffset */
	0,                                 /* tp_init */
	0,                                 /* tp_alloc */
	0,                                 /* tp_new */
};

AUD_API PyObject* SequenceEntry_empty()
{
	return SequenceEntryType.tp_alloc(&SequenceEntryType, 0);
}


AUD_API SequenceEntry* checkSequenceEntry(PyObject* entry)
{
	if(!PyObject_TypeCheck(entry, &SequenceEntryType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type SequenceEntry!");
		return nullptr;
	}

	return (SequenceEntry*)entry;
}


bool initializeSequenceEntry()
{
	return PyType_Ready(&SequenceEntryType) >= 0;
}


void addSequenceEntryToModule(PyObject* module)
{
	Py_INCREF(&SequenceEntryType);
	PyModule_AddObject(module, "SequenceEntry", (PyObject *)&SequenceEntryType);
}
