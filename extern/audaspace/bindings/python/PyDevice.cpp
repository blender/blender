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

#include "PyDevice.h"

#include "PySound.h"
#include "PyHandle.h"

#include "Exception.h"
#include "devices/IDevice.h"
#include "devices/I3DDevice.h"
#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"

#include <structmember.h>

using namespace aud;

extern PyObject* AUDError;
static const char* device_not_3d_error = "Device is not a 3D device!";

// ====================================================================

static void
Device_dealloc(Device* self)
{
	if(self->device)
		delete reinterpret_cast<std::shared_ptr<IDevice>*>(self->device);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Device_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	Device* self;

	static const char* kwlist[] = {"type", "rate", "channels", "format", "buffer_size", "name", nullptr};
	const char* device = nullptr;
	double rate = RATE_48000;
	int channels = CHANNELS_STEREO;
	int format = FORMAT_FLOAT32;
	int buffersize = AUD_DEFAULT_BUFFER_SIZE;
	const char* name = "";

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|sdiiis:Device", const_cast<char**>(kwlist),
									&device, &rate, &channels, &format, &buffersize, &name))
		return nullptr;

	if(buffersize < 128)
	{
		PyErr_SetString(PyExc_ValueError, "buffer_size must be at least 128!");
		return nullptr;
	}

	self = (Device*)type->tp_alloc(type, 0);

	if(self != nullptr)
	{
		DeviceSpecs specs;
		specs.channels = (Channels)channels;
		specs.format = (SampleFormat)format;
		specs.rate = (SampleRate)rate;

		self->device = nullptr;

		try
		{
			if(!device)
			{
				auto dev = DeviceManager::getDevice();
				if(!dev)
				{
					DeviceManager::openDefaultDevice();
					dev = DeviceManager::getDevice();
				}
				self->device = new std::shared_ptr<IDevice>(dev);
			}
			else
			{
				std::shared_ptr<IDeviceFactory> factory;
				if(!*device)
					factory = DeviceManager::getDefaultDeviceFactory();
				else
					factory = DeviceManager::getDeviceFactory(device);

				if(factory)
				{
					factory->setName(name);
					factory->setSpecs(specs);
					factory->setBufferSize(buffersize);
					self->device = new std::shared_ptr<IDevice>(factory->openDevice());
				}
			}
		}
		catch(Exception& e)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}

		if(!self->device)
		{
			Py_DECREF(self);
			PyErr_SetString(AUDError, "Unsupported device type!");
			return nullptr;
		}
	}

	return (PyObject *)self;
}

PyDoc_STRVAR(M_aud_Device_lock_doc,
			 ".. classmethod:: lock()\n\n"
			 "   Locks the device so that it's guaranteed, that no samples are\n"
			 "   read from the streams until :meth:`unlock` is called.\n"
			 "   This is useful if you want to do start/stop/pause/resume some\n"
			 "   sounds at the same time.\n\n"
			 "   .. note::\n\n"
			 "      The device has to be unlocked as often as locked to be\n"
			 "      able to continue playback.\n\n"
			 "   .. warning::\n\n"
			 "      Make sure the time between locking and unlocking is\n"
			 "      as short as possible to avoid clicks.");

static PyObject *
Device_lock(Device* self)
{
	try
	{
		(*reinterpret_cast<std::shared_ptr<IDevice>*>(self->device))->lock();
		Py_RETURN_NONE;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Device_play_doc,
			 ".. classmethod:: play(sound, keep=False)\n\n"
			 "   Plays a sound.\n\n"
			 "   :arg sound: The sound to play.\n"
			 "   :type sound: :class:`Sound`\n"
			 "   :arg keep: See :attr:`Handle.keep`.\n"
			 "   :type keep: bool\n"
			 "   :return: The playback handle with which playback can be\n"
			 "      controlled with.\n"
			 "   :rtype: :class:`Handle`");

static PyObject *
Device_play(Device* self, PyObject* args, PyObject* kwds)
{
	PyObject* object;
	PyObject* keepo = nullptr;

	bool keep = false;

	static const char* kwlist[] = {"sound", "keep", nullptr};

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:play", const_cast<char**>(kwlist), &object, &keepo))
		return nullptr;

	Sound* sound = checkSound(object);

	if(!sound)
		return nullptr;

	if(keepo != nullptr)
	{
		if(!PyBool_Check(keepo))
		{
			PyErr_SetString(PyExc_TypeError, "keep is not a boolean!");
			return nullptr;
		}

		keep = keepo == Py_True;
	}

	Handle* handle;

	handle = (Handle*)Handle_empty();
	if(handle != nullptr)
	{
		try
		{
			handle->handle = new std::shared_ptr<IHandle>((*reinterpret_cast<std::shared_ptr<IDevice>*>(self->device))->play(*reinterpret_cast<std::shared_ptr<ISound>*>(sound->sound), keep));
		}
		catch(Exception& e)
		{
			Py_DECREF(handle);
			PyErr_SetString(AUDError, e.what());
			return nullptr;
		}
	}

	return (PyObject *)handle;
}

PyDoc_STRVAR(M_aud_Device_stopAll_doc,
			 ".. classmethod:: stopAll()\n\n"
			 "   Stops all playing and paused sounds.");

static PyObject *
Device_stopAll(Device* self)
{
	try
	{
		(*reinterpret_cast<std::shared_ptr<IDevice>*>(self->device))->stopAll();
		Py_RETURN_NONE;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Device_unlock_doc,
			 ".. classmethod:: unlock()\n\n"
			 "   Unlocks the device after a lock call, see :meth:`lock` for\n"
			 "   details.");

static PyObject *
Device_unlock(Device* self)
{
	try
	{
		(*reinterpret_cast<std::shared_ptr<IDevice>*>(self->device))->unlock();
		Py_RETURN_NONE;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static PyMethodDef Device_methods[] = {
	{"lock", (PyCFunction)Device_lock, METH_NOARGS,
	 M_aud_Device_lock_doc
	},
	{"play", (PyCFunction)Device_play, METH_VARARGS | METH_KEYWORDS,
	 M_aud_Device_play_doc
	},
	{"stopAll", (PyCFunction)Device_stopAll, METH_NOARGS,
	 M_aud_Device_stopAll_doc
	},
	{"unlock", (PyCFunction)Device_unlock, METH_NOARGS,
	 M_aud_Device_unlock_doc
	},
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Device_channels_doc,
			 "The channel count of the device.");

static PyObject *
Device_get_channels(Device* self, void* nothing)
{
	try
	{
		DeviceSpecs specs = (*reinterpret_cast<std::shared_ptr<IDevice>*>(self->device))->getSpecs();
		return Py_BuildValue("i", specs.channels);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Device_distance_model_doc,
			 "The distance model of the device.\n\n"
			 ".. seealso:: `OpenAL Documentation <https://www.openal.org/documentation/>`__");

static PyObject *
Device_get_distance_model(Device* self, void* nothing)
{
	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			return Py_BuildValue("i", int(device->getDistanceModel()));
		}
		else
		{
			PyErr_SetString(AUDError, device_not_3d_error);
			return nullptr;
		}
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Device_set_distance_model(Device* self, PyObject* args, void* nothing)
{
	int model;

	if(!PyArg_Parse(args, "i:distance_model", &model))
		return -1;

	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			device->setDistanceModel(DistanceModel(model));
			return 0;
		}
		else
			PyErr_SetString(AUDError, device_not_3d_error);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Device_doppler_factor_doc,
			 "The doppler factor of the device.\n"
			 "This factor is a scaling factor for the velocity vectors in "
			 "doppler calculation. So a value bigger than 1 will exaggerate "
			 "the effect as it raises the velocity.");

static PyObject *
Device_get_doppler_factor(Device* self, void* nothing)
{
	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			return Py_BuildValue("f", device->getDopplerFactor());
		}
		else
		{
			PyErr_SetString(AUDError, device_not_3d_error);
			return nullptr;
		}
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Device_set_doppler_factor(Device* self, PyObject* args, void* nothing)
{
	float factor;

	if(!PyArg_Parse(args, "f:doppler_factor", &factor))
		return -1;

	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			device->setDopplerFactor(factor);
			return 0;
		}
		else
			PyErr_SetString(AUDError, device_not_3d_error);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Device_format_doc,
			 "The native sample format of the device.");

static PyObject *
Device_get_format(Device* self, void* nothing)
{
	try
	{
		DeviceSpecs specs = (*reinterpret_cast<std::shared_ptr<IDevice>*>(self->device))->getSpecs();
		return Py_BuildValue("i", specs.format);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Device_listener_location_doc,
			 "The listeners's location in 3D space, a 3D tuple of floats.");

static PyObject *
Device_get_listener_location(Device* self, void* nothing)
{
	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			Vector3 v = device->getListenerLocation();
			return Py_BuildValue("(fff)", v.x(), v.y(), v.z());
		}
		else
		{
			PyErr_SetString(AUDError, device_not_3d_error);
		}
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return nullptr;
}

static int
Device_set_listener_location(Device* self, PyObject* args, void* nothing)
{
	float x, y, z;

	if(!PyArg_Parse(args, "(fff):listener_location", &x, &y, &z))
		return -1;

	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			Vector3 location(x, y, z);
			device->setListenerLocation(location);
			return 0;
		}
		else
			PyErr_SetString(AUDError, device_not_3d_error);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Device_listener_orientation_doc,
			 "The listener's orientation in 3D space as quaternion, a 4 float tuple.");

static PyObject *
Device_get_listener_orientation(Device* self, void* nothing)
{
	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			Quaternion o = device->getListenerOrientation();
			return Py_BuildValue("(ffff)", o.w(), o.x(), o.y(), o.z());
		}
		else
		{
			PyErr_SetString(AUDError, device_not_3d_error);
		}
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return nullptr;
}

static int
Device_set_listener_orientation(Device* self, PyObject* args, void* nothing)
{
	float w, x, y, z;

	if(!PyArg_Parse(args, "(ffff):listener_orientation", &w, &x, &y, &z))
		return -1;

	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			Quaternion orientation(w, x, y, z);
			device->setListenerOrientation(orientation);
			return 0;
		}
		else
			PyErr_SetString(AUDError, device_not_3d_error);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Device_listener_velocity_doc,
			 "The listener's velocity in 3D space, a 3D tuple of floats.");

static PyObject *
Device_get_listener_velocity(Device* self, void* nothing)
{
	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			Vector3 v = device->getListenerVelocity();
			return Py_BuildValue("(fff)", v.x(), v.y(), v.z());
		}
		else
		{
			PyErr_SetString(AUDError, device_not_3d_error);
		}
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return nullptr;
}

static int
Device_set_listener_velocity(Device* self, PyObject* args, void* nothing)
{
	float x, y, z;

	if(!PyArg_Parse(args, "(fff):listener_velocity", &x, &y, &z))
		return -1;

	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			Vector3 velocity(x, y, z);
			device->setListenerVelocity(velocity);
			return 0;
		}
		else
			PyErr_SetString(AUDError, device_not_3d_error);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Device_rate_doc,
			 "The sampling rate of the device in Hz.");

static PyObject *
Device_get_rate(Device* self, void* nothing)
{
	try
	{
		DeviceSpecs specs = (*reinterpret_cast<std::shared_ptr<IDevice>*>(self->device))->getSpecs();
		return Py_BuildValue("d", specs.rate);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Device_speed_of_sound_doc,
			 "The speed of sound of the device.\n"
			 "The speed of sound in air is typically 343.3 m/s.");

static PyObject *
Device_get_speed_of_sound(Device* self, void* nothing)
{
	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			return Py_BuildValue("f", device->getSpeedOfSound());
		}
		else
		{
			PyErr_SetString(AUDError, device_not_3d_error);
			return nullptr;
		}
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Device_set_speed_of_sound(Device* self, PyObject* args, void* nothing)
{
	float speed;

	if(!PyArg_Parse(args, "f:speed_of_sound", &speed))
		return -1;

	try
	{
		I3DDevice* device = dynamic_cast<I3DDevice*>(reinterpret_cast<std::shared_ptr<IDevice>*>(self->device)->get());
		if(device)
		{
			device->setSpeedOfSound(speed);
			return 0;
		}
		else
			PyErr_SetString(AUDError, device_not_3d_error);
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Device_volume_doc,
			 "The overall volume of the device.");

static PyObject *
Device_get_volume(Device* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<IDevice>*>(self->device))->getVolume());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Device_set_volume(Device* self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f:volume", &volume))
		return -1;

	try
	{
		(*reinterpret_cast<std::shared_ptr<IDevice>*>(self->device))->setVolume(volume);
		return 0;
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return -1;
	}
}

static PyGetSetDef Device_properties[] = {
	{(char*)"channels", (getter)Device_get_channels, nullptr,
	 M_aud_Device_channels_doc, nullptr },
	{(char*)"distance_model", (getter)Device_get_distance_model, (setter)Device_set_distance_model,
	 M_aud_Device_distance_model_doc, nullptr },
	{(char*)"doppler_factor", (getter)Device_get_doppler_factor, (setter)Device_set_doppler_factor,
	 M_aud_Device_doppler_factor_doc, nullptr },
	{(char*)"format", (getter)Device_get_format, nullptr,
	 M_aud_Device_format_doc, nullptr },
	{(char*)"listener_location", (getter)Device_get_listener_location, (setter)Device_set_listener_location,
	 M_aud_Device_listener_location_doc, nullptr },
	{(char*)"listener_orientation", (getter)Device_get_listener_orientation, (setter)Device_set_listener_orientation,
	 M_aud_Device_listener_orientation_doc, nullptr },
	{(char*)"listener_velocity", (getter)Device_get_listener_velocity, (setter)Device_set_listener_velocity,
	 M_aud_Device_listener_velocity_doc, nullptr },
	{(char*)"rate", (getter)Device_get_rate, nullptr,
	 M_aud_Device_rate_doc, nullptr },
	{(char*)"speed_of_sound", (getter)Device_get_speed_of_sound, (setter)Device_set_speed_of_sound,
	 M_aud_Device_speed_of_sound_doc, nullptr },
	{(char*)"volume", (getter)Device_get_volume, (setter)Device_set_volume,
	 M_aud_Device_volume_doc, nullptr },
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Device_doc,
			 "Device objects represent an audio output backend like OpenAL or "
			 "SDL, but might also represent a file output or RAM buffer "
			 "output.");

static PyTypeObject DeviceType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
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
	M_aud_Device_doc,          /* tp_doc */
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
	Device_new,                /* tp_new */
};

AUD_API PyObject* Device_empty()
{
	return DeviceType.tp_alloc(&DeviceType, 0);
}


AUD_API Device* checkDevice(PyObject* device)
{
	if(!PyObject_TypeCheck(device, &DeviceType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type Device!");
		return nullptr;
	}

	return (Device*)device;
}


bool initializeDevice()
{
	return PyType_Ready(&DeviceType) >= 0;
}


void addDeviceToModule(PyObject* module)
{
	Py_INCREF(&DeviceType);
	PyModule_AddObject(module, "Device", (PyObject *)&DeviceType);
}
