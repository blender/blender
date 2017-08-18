/*******************************************************************************
* Copyright 2015-2016 Juan Francisco Crespo Galán
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

#include "PyPlaybackManager.h"
#include "PySound.h"
#include "PyHandle.h"
#include "PyDevice.h"

#include "Exception.h"
#include "fx/PlaybackManager.h"

extern PyObject* AUDError;

static PyObject *
PlaybackManager_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	PlaybackManagerP* self = (PlaybackManagerP*)type->tp_alloc(type, 0);

	if(self != nullptr)
	{
		PyObject* object;
		if(!PyArg_ParseTuple(args, "O:catKey", &object))
			return nullptr;
		Device* device = checkDevice(object);

		try
		{
			self->playbackManager = new std::shared_ptr<aud::PlaybackManager>(new aud::PlaybackManager(*reinterpret_cast<std::shared_ptr<aud::IDevice>*>(device->device)));
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
PlaybackManager_dealloc(PlaybackManagerP* self)
{
	if(self->playbackManager)
		delete reinterpret_cast<std::shared_ptr<aud::PlaybackManager>*>(self->playbackManager);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(M_aud_PlaybackManager_play_doc,
	"setVolume(sound, catKey)\n\n"
	"Plays a sound through the playback manager and assigns it to a category.\n\n"
	":arg sound: The sound to play.\n"
	":type sound: :class:`Sound`\n"
	":arg catKey: the key of the category in which the sound will be added, if it doesn't exist, a new one will be created.\n"
	":type catKey: int\n"
	":return: The playback handle with which playback can be controlled with.\n"
	":rtype: :class:`Handle`");

static PyObject *
PlaybackManager_play(PlaybackManagerP* self, PyObject* args)
{
	PyObject* object;
	unsigned int cat;

	if(!PyArg_ParseTuple(args, "OI:catKey", &object, &cat))
		return nullptr;

	Sound* sound = checkSound(object);
	if(!sound)
		return nullptr;

	Handle* handle;

	handle = (Handle*)Handle_empty();
	if(handle != nullptr)
	{
		try
		{
			handle->handle = new std::shared_ptr<aud::IHandle>((*reinterpret_cast<std::shared_ptr<aud::PlaybackManager>*>(self->playbackManager))->play(*reinterpret_cast<std::shared_ptr<aud::ISound>*>(sound->sound), cat));
		}
		catch(aud::Exception& e)
		{
			Py_DECREF(handle);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)handle;
}

PyDoc_STRVAR(M_aud_PlaybackManager_resume_doc,
	"resume(catKey)\n\n"
	"Resumes playback of the catgory.\n\n"
	":arg catKey: the key of the category.\n"
	":type catKey: int\n"
	":return: Whether the action succeeded.\n"
	":rtype: bool");

static PyObject *
PlaybackManager_resume(PlaybackManagerP* self, PyObject* args)
{
	unsigned int cat;

	if(!PyArg_ParseTuple(args, "I:catKey", &cat))
		return nullptr;

	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<aud::PlaybackManager>*>(self->playbackManager))->resume(cat));
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_PlaybackManager_pause_doc,
	"pause(catKey)\n\n"
	"Pauses playback of the category.\n\n"
	":arg catKey: the key of the category.\n"
	":type catKey: int\n"
	":return: Whether the action succeeded.\n"
	":rtype: bool");

static PyObject *
PlaybackManager_pause(PlaybackManagerP* self, PyObject* args)
{
	unsigned int cat;

	if(!PyArg_ParseTuple(args, "I:catKey", &cat))
		return nullptr;

	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<aud::PlaybackManager>*>(self->playbackManager))->pause(cat));
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_PlaybackManager_add_category_doc,
	"addCategory(volume)\n\n"
	"Adds a category with a custom volume.\n\n"
	":arg volume: The volume for ther new category.\n"
	":type volume: float\n"
	":return: The key of the new category.\n"
	":rtype: int\n\n");

static PyObject *
PlaybackManager_add_category(PlaybackManagerP* self, PyObject* args)
{
	float vol;

	if(!PyArg_ParseTuple(args, "f:volume", &vol))
		return nullptr;

	try
	{
		return Py_BuildValue("I", (*reinterpret_cast<std::shared_ptr<aud::PlaybackManager>*>(self->playbackManager))->addCategory(vol));
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_PlaybackManager_get_volume_doc,
	"getVolume(catKey)\n\n"
	"Retrieves the volume of a category.\n\n"
	":arg catKey: the key of the category.\n"
	":type catKey: int\n"
	":return: The volume of the cateogry.\n"
	":rtype: float\n\n");

static PyObject *
PlaybackManager_get_volume(PlaybackManagerP* self, PyObject* args)
{
	unsigned int cat;

	if(!PyArg_ParseTuple(args, "I:catKey", &cat))
		return nullptr;

	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<aud::PlaybackManager>*>(self->playbackManager))->getVolume(cat));
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_PlaybackManager_set_volume_doc,
	"setVolume(volume, catKey)\n\n"
	"Changes the volume of a category.\n\n"
	":arg volume: the new volume value.\n"
	":type volume: float\n"
	":arg catKey: the key of the category.\n"
	":type catKey: int\n"
	":return: Whether the action succeeded.\n"
	":rtype: int\n\n");

static PyObject *
PlaybackManager_set_volume(PlaybackManagerP* self, PyObject* args)
{
	float volume;
	unsigned int cat;

	if(!PyArg_ParseTuple(args, "fI:volume", &volume, &cat))
		return nullptr;

	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<aud::PlaybackManager>*>(self->playbackManager))->setVolume(volume, cat));
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_PlaybackManager_stop_doc,
	"stop(catKey)\n\n"
	"Stops playback of the category.\n\n"
	":arg catKey: the key of the category.\n"
	":type catKey: int\n"
	":return: Whether the action succeeded.\n"
	":rtype: bool\n\n");

static PyObject *
PlaybackManager_stop(PlaybackManagerP* self, PyObject* args)
{
	unsigned int cat;

	if(!PyArg_ParseTuple(args, "I:catKey", &cat))
		return nullptr;

	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<aud::PlaybackManager>*>(self->playbackManager))->stop(cat));
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_PlaybackManager_clean_doc,
	"clean()\n\n"
	"Cleans all the invalid and finished sound from the playback manager.\n\n");

static PyObject *
PlaybackManager_clean(PlaybackManagerP* self)
{
	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::PlaybackManager>*>(self->playbackManager))->clean();
		Py_RETURN_NONE;
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static PyMethodDef PlaybackManager_methods[] = {
	{ "play", (PyCFunction)PlaybackManager_play, METH_VARARGS | METH_KEYWORDS,
	M_aud_PlaybackManager_play_doc
	},
	{ "resume", (PyCFunction)PlaybackManager_resume, METH_VARARGS,
	M_aud_PlaybackManager_resume_doc
	},
	{ "pause", (PyCFunction)PlaybackManager_pause, METH_VARARGS,
	M_aud_PlaybackManager_pause_doc
	},
	{ "stop", (PyCFunction)PlaybackManager_stop, METH_VARARGS,
	M_aud_PlaybackManager_stop_doc
	},
	{ "addCategory", (PyCFunction)PlaybackManager_add_category, METH_VARARGS,
	M_aud_PlaybackManager_add_category_doc
	},
	{ "getVolume", (PyCFunction)PlaybackManager_get_volume, METH_VARARGS,
	M_aud_PlaybackManager_get_volume_doc
	},
	{ "setVolume", (PyCFunction)PlaybackManager_set_volume, METH_VARARGS,
	M_aud_PlaybackManager_set_volume_doc
	},
	{ "clean", (PyCFunction)PlaybackManager_clean, METH_NOARGS,
	M_aud_PlaybackManager_clean_doc
	},
	{ nullptr }  /* Sentinel */
};

PyDoc_STRVAR(M_aud_PlaybackManager_doc,
	"A PlabackManager object allows to easily control groups os sounds organized in categories.");

PyTypeObject PlaybackManagerType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"aud.PlaybackManager",					/* tp_name */
	sizeof(PlaybackManagerP),				/* tp_basicsize */
	0,										/* tp_itemsize */
	(destructor)PlaybackManager_dealloc,	/* tp_dealloc */
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
	M_aud_PlaybackManager_doc,				/* tp_doc */
	0,										/* tp_traverse */
	0,										/* tp_clear */
	0,										/* tp_richcompare */
	0,										/* tp_weaklistoffset */
	0,										/* tp_iter */
	0,										/* tp_iternext */
	PlaybackManager_methods,				/* tp_methods */
	0,										/* tp_members */
	0,										/* tp_getset */
	0,										/* tp_base */
	0,										/* tp_dict */
	0,										/* tp_descr_get */
	0,										/* tp_descr_set */
	0,										/* tp_dictoffset */
	0,										/* tp_init */
	0,										/* tp_alloc */
	PlaybackManager_new,					/* tp_new */
};

AUD_API PyObject* PlaybackManager_empty()
{
	return PlaybackManagerType.tp_alloc(&PlaybackManagerType, 0);
}


AUD_API PlaybackManagerP* checkPlaybackManager(PyObject* playbackManager)
{
	if(!PyObject_TypeCheck(playbackManager, &PlaybackManagerType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type PlaybackManager!");
		return nullptr;
	}

	return (PlaybackManagerP*)playbackManager;
}


bool initializePlaybackManager()
{
	return PyType_Ready(&PlaybackManagerType) >= 0;
}


void addPlaybackManagerToModule(PyObject* module)
{
	Py_INCREF(&PlaybackManagerType);
	PyModule_AddObject(module, "PlaybackManager", (PyObject *)&PlaybackManagerType);
}
