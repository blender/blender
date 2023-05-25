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

#include "PyHandle.h"

#include "devices/IHandle.h"
#include "devices/I3DHandle.h"
#include "Exception.h"

#include <memory>

#include <structmember.h>

using namespace aud;

extern PyObject* AUDError;
static const char* device_not_3d_error = "Device is not a 3D device!";

static void
Handle_dealloc(Handle* self)
{
	if(self->handle)
		delete reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(M_aud_Handle_pause_doc,
			 ".. method:: pause()\n\n"
			 "   Pauses playback.\n\n"
			 "   :return: Whether the action succeeded.\n"
			 "   :rtype: bool");

static PyObject *
Handle_pause(Handle* self)
{
	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->pause());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Handle_resume_doc,
			 ".. method:: resume()\n\n"
			 "   Resumes playback.\n\n"
			 "   :return: Whether the action succeeded.\n"
			 "   :rtype: bool");

static PyObject *
Handle_resume(Handle* self)
{
	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->resume());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Handle_stop_doc,
			 ".. method:: stop()\n\n"
			 "   Stops playback.\n\n"
			 "   :return: Whether the action succeeded.\n"
			 "   :rtype: bool\n\n"
			 "   .. note:: This makes the handle invalid.");

static PyObject *
Handle_stop(Handle* self)
{
	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->stop());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
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
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Handle_attenuation_doc,
			 "This factor is used for distance based attenuation of the "
			 "source.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
Handle_get_attenuation(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			return Py_BuildValue("f", handle->getAttenuation());
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
Handle_set_attenuation(Handle* self, PyObject* args, void* nothing)
{
	float factor;

	if(!PyArg_Parse(args, "f:attenuation", &factor))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			if(handle->setAttenuation(factor))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the attenuation!");
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

PyDoc_STRVAR(M_aud_Handle_cone_angle_inner_doc,
			 "The opening angle of the inner cone of the source. If the cone "
			 "values of a source are set there are two (audible) cones with "
			 "the apex at the :attr:`location` of the source and with infinite "
			 "height, heading in the direction of the source's "
			 ":attr:`orientation`.\n"
			 "In the inner cone the volume is normal. Outside the outer cone "
			 "the volume will be :attr:`cone_volume_outer` and in the area "
			 "between the volume will be interpolated linearly.");

static PyObject *
Handle_get_cone_angle_inner(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			return Py_BuildValue("f", handle->getConeAngleInner());
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
Handle_set_cone_angle_inner(Handle* self, PyObject* args, void* nothing)
{
	float angle;

	if(!PyArg_Parse(args, "f:cone_angle_inner", &angle))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			if(handle->setConeAngleInner(angle))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the cone inner angle!");
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

PyDoc_STRVAR(M_aud_Handle_cone_angle_outer_doc,
			 "The opening angle of the outer cone of the source.\n\n"
			 ".. seealso:: :attr:`cone_angle_inner`");

static PyObject *
Handle_get_cone_angle_outer(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			return Py_BuildValue("f", handle->getConeAngleOuter());
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
Handle_set_cone_angle_outer(Handle* self, PyObject* args, void* nothing)
{
	float angle;

	if(!PyArg_Parse(args, "f:cone_angle_outer", &angle))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			if(handle->setConeAngleOuter(angle))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the cone outer angle!");
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

PyDoc_STRVAR(M_aud_Handle_cone_volume_outer_doc,
			 "The volume outside the outer cone of the source.\n\n"
			 ".. seealso:: :attr:`cone_angle_inner`");

static PyObject *
Handle_get_cone_volume_outer(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			return Py_BuildValue("f", handle->getConeVolumeOuter());
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
Handle_set_cone_volume_outer(Handle* self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f:cone_volume_outer", &volume))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			if(handle->setConeVolumeOuter(volume))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the cone outer volume!");
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

PyDoc_STRVAR(M_aud_Handle_distance_maximum_doc,
			 "The maximum distance of the source.\n"
			 "If the listener is further away the source volume will be 0.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
Handle_get_distance_maximum(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			return Py_BuildValue("f", handle->getDistanceMaximum());
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
Handle_set_distance_maximum(Handle* self, PyObject* args, void* nothing)
{
	float distance;

	if(!PyArg_Parse(args, "f:distance_maximum", &distance))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			if(handle->setDistanceMaximum(distance))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the maximum distance!");
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

PyDoc_STRVAR(M_aud_Handle_distance_reference_doc,
			 "The reference distance of the source.\n"
			 "At this distance the volume will be exactly :attr:`volume`.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
Handle_get_distance_reference(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			return Py_BuildValue("f", handle->getDistanceReference());
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
Handle_set_distance_reference(Handle* self, PyObject* args, void* nothing)
{
	float distance;

	if(!PyArg_Parse(args, "f:distance_reference", &distance))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			if(handle->setDistanceReference(distance))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the reference distance!");
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

PyDoc_STRVAR(M_aud_Handle_keep_doc,
			 "Whether the sound should be kept paused in the device when its "
			 "end is reached.\n"
			 "This can be used to seek the sound to some position and start "
			 "playback again.\n\n"
			 ".. warning:: If this is set to true and you forget stopping this "
			 "equals a memory leak as the handle exists until the device is "
			 "destroyed.");

static PyObject *
Handle_get_keep(Handle* self, void* nothing)
{
	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->getKeep());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Handle_set_keep(Handle* self, PyObject* args, void* nothing)
{
	if(!PyBool_Check(args))
	{
		PyErr_SetString(PyExc_TypeError, "keep is not a boolean!");
		return -1;
	}

	bool keep = args == Py_True;

	try
	{
		if((*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->setKeep(keep))
			return 0;
		PyErr_SetString(AUDError, "Couldn't set keep of the sound!");
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Handle_location_doc,
			 "The source's location in 3D space, a 3D tuple of floats.");

static PyObject *
Handle_get_location(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			Vector3 v = handle->getLocation();
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
Handle_set_location(Handle* self, PyObject* args, void* nothing)
{
	float x, y, z;

	if(!PyArg_Parse(args, "(fff):location", &x, &y, &z))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			Vector3 location(x, y, z);
			if(handle->setLocation(location))
				return 0;
			PyErr_SetString(AUDError, "Location couldn't be set!");
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

PyDoc_STRVAR(M_aud_Handle_loop_count_doc,
			 "The (remaining) loop count of the sound. A negative value indicates infinity.");

static PyObject *
Handle_get_loop_count(Handle* self, void* nothing)
{
	try
	{
		return Py_BuildValue("i", (*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->getLoopCount());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Handle_set_loop_count(Handle* self, PyObject* args, void* nothing)
{
	int loops;

	if(!PyArg_Parse(args, "i:loop_count", &loops))
		return -1;

	try
	{
		if((*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->setLoopCount(loops))
			return 0;
		PyErr_SetString(AUDError, "Couldn't set the loop count!");
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Handle_orientation_doc,
			 "The source's orientation in 3D space as quaternion, a 4 float tuple.");

static PyObject *
Handle_get_orientation(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			Quaternion o = handle->getOrientation();
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
Handle_set_orientation(Handle* self, PyObject* args, void* nothing)
{
	float w, x, y, z;

	if(!PyArg_Parse(args, "(ffff):orientation", &w, &x, &y, &z))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			Quaternion orientation(w, x, y, z);
			if(handle->setOrientation(orientation))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the orientation!");
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

PyDoc_STRVAR(M_aud_Handle_pitch_doc,
			 "The pitch of the sound.");

static PyObject *
Handle_get_pitch(Handle* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->getPitch());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Handle_set_pitch(Handle* self, PyObject* args, void* nothing)
{
	float pitch;

	if(!PyArg_Parse(args, "f:pitch", &pitch))
		return -1;

	try
	{
		if((*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->setPitch(pitch))
			return 0;
		PyErr_SetString(AUDError, "Couldn't set the sound pitch!");
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Handle_position_doc,
			 "The playback position of the sound in seconds.");

static PyObject *
Handle_get_position(Handle* self, void* nothing)
{
	try
	{
		return Py_BuildValue("d", (*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->getPosition());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Handle_set_position(Handle* self, PyObject* args, void* nothing)
{
	double position;

	if(!PyArg_Parse(args, "d:position", &position))
		return -1;

	try
	{
		if((*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->seek(position))
			return 0;
		PyErr_SetString(AUDError, "Couldn't seek the sound!");
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Handle_relative_doc,
			 "Whether the source's location, velocity and orientation is relative or absolute to the listener.");

static PyObject *
Handle_get_relative(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			return PyBool_FromLong((long)handle->isRelative());
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
Handle_set_relative(Handle* self, PyObject* args, void* nothing)
{
	if(!PyBool_Check(args))
	{
		PyErr_SetString(PyExc_TypeError, "Value is not a boolean!");
		return -1;
	}

	bool relative = (args == Py_True);

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			if(handle->setRelative(relative))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the relativeness!");
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

PyDoc_STRVAR(M_aud_Handle_status_doc,
			 "Whether the sound is playing, paused or stopped (=invalid).");

static PyObject *
Handle_get_status(Handle* self, void* nothing)
{
	try
	{
		return PyBool_FromLong((long)(*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->getStatus());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

PyDoc_STRVAR(M_aud_Handle_velocity_doc,
			 "The source's velocity in 3D space, a 3D tuple of floats.");

static PyObject *
Handle_get_velocity(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			Vector3 v = handle->getVelocity();
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
Handle_set_velocity(Handle* self, PyObject* args, void* nothing)
{
	float x, y, z;

	if(!PyArg_Parse(args, "(fff):velocity", &x, &y, &z))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			Vector3 velocity(x, y, z);
			if(handle->setVelocity(velocity))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the velocity!");
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

PyDoc_STRVAR(M_aud_Handle_volume_doc,
			 "The volume of the sound.");

static PyObject *
Handle_get_volume(Handle* self, void* nothing)
{
	try
	{
		return Py_BuildValue("f", (*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->getVolume());
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
		return nullptr;
	}
}

static int
Handle_set_volume(Handle* self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f:volume", &volume))
		return -1;

	try
	{
		if((*reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle))->setVolume(volume))
			return 0;
		PyErr_SetString(AUDError, "Couldn't set the sound volume!");
	}
	catch(Exception& e)
	{
		PyErr_SetString(AUDError, e.what());
	}

	return -1;
}

PyDoc_STRVAR(M_aud_Handle_volume_maximum_doc,
			 "The maximum volume of the source.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
Handle_get_volume_maximum(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			return Py_BuildValue("f", handle->getVolumeMaximum());
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
Handle_set_volume_maximum(Handle* self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f:volume_maximum", &volume))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			if(handle->setVolumeMaximum(volume))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the maximum volume!");
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

PyDoc_STRVAR(M_aud_Handle_volume_minimum_doc,
			 "The minimum volume of the source.\n\n"
			 ".. seealso:: :attr:`Device.distance_model`");

static PyObject *
Handle_get_volume_minimum(Handle* self, void* nothing)
{
	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			return Py_BuildValue("f", handle->getVolumeMinimum());
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
Handle_set_volume_minimum(Handle* self, PyObject* args, void* nothing)
{
	float volume;

	if(!PyArg_Parse(args, "f:volume_minimum", &volume))
		return -1;

	try
	{
		I3DHandle* handle = dynamic_cast<I3DHandle*>(reinterpret_cast<std::shared_ptr<IHandle>*>(self->handle)->get());
		if(handle)
		{
			if(handle->setVolumeMinimum(volume))
				return 0;
			PyErr_SetString(AUDError, "Couldn't set the minimum volume!");
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

static PyGetSetDef Handle_properties[] = {
	{(char*)"attenuation", (getter)Handle_get_attenuation, (setter)Handle_set_attenuation,
	 M_aud_Handle_attenuation_doc, nullptr },
	{(char*)"cone_angle_inner", (getter)Handle_get_cone_angle_inner, (setter)Handle_set_cone_angle_inner,
	 M_aud_Handle_cone_angle_inner_doc, nullptr },
	{(char*)"cone_angle_outer", (getter)Handle_get_cone_angle_outer, (setter)Handle_set_cone_angle_outer,
	 M_aud_Handle_cone_angle_outer_doc, nullptr },
	{(char*)"cone_volume_outer", (getter)Handle_get_cone_volume_outer, (setter)Handle_set_cone_volume_outer,
	 M_aud_Handle_cone_volume_outer_doc, nullptr },
	{(char*)"distance_maximum", (getter)Handle_get_distance_maximum, (setter)Handle_set_distance_maximum,
	 M_aud_Handle_distance_maximum_doc, nullptr },
	{(char*)"distance_reference", (getter)Handle_get_distance_reference, (setter)Handle_set_distance_reference,
	 M_aud_Handle_distance_reference_doc, nullptr },
	{(char*)"keep", (getter)Handle_get_keep, (setter)Handle_set_keep,
	 M_aud_Handle_keep_doc, nullptr },
	{(char*)"location", (getter)Handle_get_location, (setter)Handle_set_location,
	 M_aud_Handle_location_doc, nullptr },
	{(char*)"loop_count", (getter)Handle_get_loop_count, (setter)Handle_set_loop_count,
	 M_aud_Handle_loop_count_doc, nullptr },
	{(char*)"orientation", (getter)Handle_get_orientation, (setter)Handle_set_orientation,
	 M_aud_Handle_orientation_doc, nullptr },
	{(char*)"pitch", (getter)Handle_get_pitch, (setter)Handle_set_pitch,
	 M_aud_Handle_pitch_doc, nullptr },
	{(char*)"position", (getter)Handle_get_position, (setter)Handle_set_position,
	 M_aud_Handle_position_doc, nullptr },
	{(char*)"relative", (getter)Handle_get_relative, (setter)Handle_set_relative,
	 M_aud_Handle_relative_doc, nullptr },
	{(char*)"status", (getter)Handle_get_status, nullptr,
	 M_aud_Handle_status_doc, nullptr },
	{(char*)"velocity", (getter)Handle_get_velocity, (setter)Handle_set_velocity,
	 M_aud_Handle_velocity_doc, nullptr },
	{(char*)"volume", (getter)Handle_get_volume, (setter)Handle_set_volume,
	 M_aud_Handle_volume_doc, nullptr },
	{(char*)"volume_maximum", (getter)Handle_get_volume_maximum, (setter)Handle_set_volume_maximum,
	 M_aud_Handle_volume_maximum_doc, nullptr },
	{(char*)"volume_minimum", (getter)Handle_get_volume_minimum, (setter)Handle_set_volume_minimum,
	 M_aud_Handle_volume_minimum_doc, nullptr },
	{nullptr}  /* Sentinel */
};

PyDoc_STRVAR(M_aud_Handle_doc,
			 "Handle objects are playback handles that can be used to control "
			 "playback of a sound. If a sound is played back multiple times "
			 "then there are as many handles.");

static PyTypeObject HandleType = {
	PyVarObject_HEAD_INIT(nullptr, 0)
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
	M_aud_Handle_doc,          /* tp_doc */
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


AUD_API PyObject* Handle_empty()
{
	return HandleType.tp_alloc(&HandleType, 0);
}


AUD_API Handle*checkHandle(PyObject* handle)
{
	if(!PyObject_TypeCheck(handle, &HandleType))
	{
		PyErr_SetString(PyExc_TypeError, "Object is not of type Handle!");
		return nullptr;
	}

	return (Handle*)handle;
}


bool initializeHandle()
{
	return PyType_Ready(&HandleType) >= 0;
}


void addHandleToModule(PyObject* module)
{
	Py_INCREF(&HandleType);
	PyModule_AddObject(module, "Handle", (PyObject *)&HandleType);
}
