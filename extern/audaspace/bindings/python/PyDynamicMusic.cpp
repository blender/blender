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

#include "PyDynamicMusic.h"
#include "PySound.h"
#include "PyHandle.h"
#include "PyDevice.h"

#include "Exception.h"
#include "fx/DynamicMusic.h"

extern PyObject* AUDError;

static PyObject *
DynamicMusic_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	DynamicMusicP* self = (DynamicMusicP*)type->tp_alloc(type, 0);

	if(self != nullptr)
	{
		PyObject* object;
		if(!PyArg_ParseTuple(args, "O:device", &object))
			return nullptr;
		Device* device = checkDevice(object);

		try
		{
			self->dynamicMusic = new std::shared_ptr<aud::DynamicMusic>(new aud::DynamicMusic(*reinterpret_cast<std::shared_ptr<aud::IDevice>*>(device->device)));
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
DynamicMusic_dealloc(DynamicMusicP* self)
{
	if(self->dynamicMusic)
		delete reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(M_aud_DynamicMusic_addScene_doc,
	".. classmethod:: addScene(scene)\n\n"
	"   Adds a new scene.\n\n"
	"   :arg scene: The scene sound.\n"
	"   :type scene: :class:`Sound`\n"
	"   :return: The new scene id.\n"
	"   :rtype: int");

static PyObject *
DynamicMusic_addScene(DynamicMusicP* self, PyObject* args)
{
	PyObject* object;
	if(!PyArg_Parse(args, "O:sound", &object))
		return nullptr;

	Sound* sound = checkSound(object);
	if(!sound)
		return nullptr;

	try
	{
		return Py_BuildValue("i", (*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->addScene(*reinterpret_cast<std::shared_ptr<aud::ISound>*>(sound->sound)));
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_DynamicMusic_addTransition_doc,
	".. classmethod:: addTransition(ini, end, transition)\n\n"
	"   Adds a new scene.\n\n"
	"   :arg ini: the initial scene foor the transition.\n"
	"   :type ini: int\n"
	"   :arg end: The final scene for the transition.\n"
	"   :type end: int\n"
	"   :arg transition: The transition sound.\n"
	"   :type transition: :class:`Sound`\n"
	"   :return: false if the ini or end scenes don't exist, true othrwise.\n"
	"   :rtype: bool");

static PyObject *
DynamicMusic_addTransition(DynamicMusicP* self, PyObject* args)
{
	PyObject* object;
	int ini, end;
	if(!PyArg_ParseTuple(args, "iiO:sound", &ini, &end, &object))
		return nullptr;
	Sound* sound = checkSound(object);
	if(!sound)
		return nullptr;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->addTransition(ini, end, *reinterpret_cast<std::shared_ptr<aud::ISound>*>(sound->sound));
		Py_RETURN_NONE;
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_DynamicMusic_resume_doc,
	".. classmethod:: resume()\n\n"
	"   Resumes playback of the scene.\n\n"
	"   :return: Whether the action succeeded.\n"
	"   :rtype: bool");

static PyObject *
DynamicMusic_resume(DynamicMusicP* self)
{
	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->resume());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_DynamicMusic_pause_doc,
	".. classmethod:: pause()\n\n"
	"   Pauses playback of the scene.\n\n"
	"   :return: Whether the action succeeded.\n"
	"   :rtype: bool");

static PyObject *
DynamicMusic_pause(DynamicMusicP* self)
{
	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->pause());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_DynamicMusic_stop_doc,
	".. classmethod:: stop()\n\n"
	"   Stops playback of the scene.\n\n"
	"   :return: Whether the action succeeded.\n"
	"   :rtype: bool\n\n");

static PyObject *
DynamicMusic_stop(DynamicMusicP* self)
{
	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->stop());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static PyMethodDef DynamicMusic_methods[] = {
	{ "addScene", (PyCFunction)DynamicMusic_addScene, METH_O,
	M_aud_DynamicMusic_addScene_doc
	},
	{ "addTransition", (PyCFunction)DynamicMusic_addTransition, METH_VARARGS,
	M_aud_DynamicMusic_addTransition_doc
	},
	{ "resume", (PyCFunction)DynamicMusic_resume, METH_NOARGS,
	M_aud_DynamicMusic_resume_doc
	},
	{ "pause", (PyCFunction)DynamicMusic_pause, METH_NOARGS,
	M_aud_DynamicMusic_pause_doc
	},
	{ "stop", (PyCFunction)DynamicMusic_stop, METH_NOARGS,
	M_aud_DynamicMusic_stop_doc
	},
	{ nullptr }  /* Sentinel */
};

/////////////////////////////////////////////////////

PyDoc_STRVAR(M_aud_DynamicMusic_status_doc,
	"Whether the scene is playing, paused or stopped (=invalid).");

static PyObject *
DynamicMusic_get_status(DynamicMusicP* self, void* nothing)
{
	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->getStatus());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_DynamicMusic_position_doc,
	"The playback position of the scene in seconds.");

static int
DynamicMusic_set_position(DynamicMusicP* self, PyObject* args, void* nothing)
{
	double position;

	if(!PyArg_Parse(args, "d:position", &position))
		return -1;

	try
	{
		if((*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->seek(position))
			return 0;
		PyErr_SetString(AUDError, "Couldn't seek the sound!");
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

static PyObject *
DynamicMusic_get_position(DynamicMusicP* self, void* nothing)
{
	try
	{
		return Py_BuildValue("d", (*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->getPosition());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_DynamicMusic_fadeTime_doc,
	"The length in seconds of the crossfade transition");

static int
DynamicMusic_set_fadeTime(DynamicMusicP* self, PyObject* args, void* nothing)
{
	float fadeTime;

	if(!PyArg_Parse(args, "f:fadeTime", &fadeTime))
		return -1;

	try
	{
		(*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->setFadeTime(fadeTime);
		return 0;
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

static PyObject *
DynamicMusic_get_fadeTime(DynamicMusicP* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->getFadeTime());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_DynamicMusic_scene_doc,
	"The current scene");

static int
DynamicMusic_set_scene(DynamicMusicP* self, PyObject* args, void* nothing)
{
	int scene;

	if(!PyArg_Parse(args, "i:scene", &scene))
		return -1;

	try
	{
		if((*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->changeScene(scene))
			return 0;
		PyErr_SetString(AUDError, "Couldn't change the scene!");
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

static PyObject *
DynamicMusic_get_scene(DynamicMusicP* self, void* nothing)
{
	try
	{
		return Py_BuildValue("i", (*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->getScene());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_DynamicMusic_volume_doc,
	"The volume of the scene.");

static int
DynamicMusic_set_volume(DynamicMusicP* self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f:volume", &volume))
		return -1;

	try
	{
		if((*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->setVolume(volume))
			return 0;
		PyErr_SetString(AUDError, "Couldn't change the volume!");
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

static PyObject *
DynamicMusic_get_volume(DynamicMusicP* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<aud::DynamicMusic>*>(self->dynamicMusic))->getVolume());
	}
	catch(aud::Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static PyGetSetDef DynamicMusic_properties[] = {
	{ (char*)"status", (getter)DynamicMusic_get_status, nullptr,
	M_aud_DynamicMusic_status_doc, nullptr },
	{ (char*)"position", (getter)DynamicMusic_get_position, (setter)DynamicMusic_set_position,
	M_aud_DynamicMusic_position_doc, nullptr },
	{ (char*)"fadeTime", (getter)DynamicMusic_get_fadeTime, (setter)DynamicMusic_set_fadeTime,
	M_aud_DynamicMusic_fadeTime_doc, nullptr },
	{ (char*)"scene", (getter)DynamicMusic_get_scene, (setter)DynamicMusic_set_scene,
	M_aud_DynamicMusic_scene_doc, nullptr },
	{ (char*)"volume", (getter)DynamicMusic_get_volume, (setter)DynamicMusic_set_volume,
	M_aud_DynamicMusic_volume_doc, nullptr },
	{ nullptr }  /* Sentinel */
};

PyDoc_STRVAR(M_aud_DynamicMusic_doc,
	"The DynamicMusic object allows to play music depending on a current scene, scene changes are managed by the class, with the possibility of custom transitions.\n"
	"The default transition is a crossfade effect, and the default scene is silent and has id 0");

PyTypeObject DynamicMusicType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"aud.DynamicMusic",						/* tp_name */
	sizeof(DynamicMusicP),					/* tp_basicsize */
	0,										/* tp_itemsize */
	(destructor)DynamicMusic_dealloc,		/* tp_dealloc */
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
	M_aud_DynamicMusic_doc,					/* tp_doc */
	0,										/* tp_traverse */
	0,										/* tp_clear */
	0,										/* tp_richcompare */
	0,										/* tp_weaklistoffset */
	0,										/* tp_iter */
	0,										/* tp_iternext */
	DynamicMusic_methods,					/* tp_methods */
	0,										/* tp_members */
	DynamicMusic_properties,				/* tp_getset */
	0,										/* tp_base */
	0,										/* tp_dict */
	0,										/* tp_descr_get */
	0,										/* tp_descr_set */
	0,										/* tp_dictoffset */
	0,										/* tp_init */
	0,										/* tp_alloc */
	DynamicMusic_new,						/* tp_new */
};

AUD_API PyObject* DynamicMusic_empty()
{
	return DynamicMusicType.tp_alloc(&DynamicMusicType, 0);
}


AUD_API DynamicMusicP* checkDynamicMusic(PyObject* dynamicMusic)
{
	if(!PyObject_TypeCheck(dynamicMusic, &DynamicMusicType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type DynamicMusic!");
		return nullptr;
	}

	return (DynamicMusicP*)dynamicMusic;
}


bool initializeDynamicMusic()
{
	return PyType_Ready(&DynamicMusicType) >= 0;
}


void addDynamicMusicToModule(PyObject* module)
{
	Py_INCREF(&DynamicMusicType);
	PyModule_AddObject(module, "DynamicMusic", (PyObject *)&DynamicMusicType);
}
