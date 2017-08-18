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

#include "devices/I3DHandle.h"
#include "Exception.h"

#include <cassert>

using namespace aud;

#define AUD_CAPI_IMPLEMENTATION
#include "AUD_Handle.h"

AUD_API int AUD_Handle_pause(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->pause();
}

AUD_API int AUD_Handle_resume(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->resume();
}

AUD_API int AUD_Handle_stop(AUD_Handle* handle)
{
	assert(handle);
	int result = (*handle)->stop();
	delete handle;
	return result;
}

AUD_API float AUD_Handle_getAttenuation(AUD_Handle* handle)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->getAttenuation();
	return 0.0f;
}

AUD_API int AUD_Handle_setAttenuation(AUD_Handle* handle, float value)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->setAttenuation(value);
	return false;
}

AUD_API float AUD_Handle_getConeAngleInner(AUD_Handle* handle)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->getConeAngleInner();
	return 0.0f;
}

AUD_API int AUD_Handle_setConeAngleInner(AUD_Handle* handle, float value)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->setConeAngleInner(value);
	return false;
}

AUD_API float AUD_Handle_getConeAngleOuter(AUD_Handle* handle)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->getConeAngleOuter();
	return 0.0f;
}

AUD_API int AUD_Handle_setConeAngleOuter(AUD_Handle* handle, float value)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->setConeAngleOuter(value);
	return false;
}

AUD_API float AUD_Handle_getConeVolumeOuter(AUD_Handle* handle)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->getConeVolumeOuter();
	return 0.0f;
}

AUD_API int AUD_Handle_setConeVolumeOuter(AUD_Handle* handle, float value)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->setConeVolumeOuter(value);
	return false;
}

AUD_API float AUD_Handle_getDistanceMaximum(AUD_Handle* handle)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->getDistanceMaximum();
	return 0.0f;
}

AUD_API int AUD_Handle_setDistanceMaximum(AUD_Handle* handle, float value)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->setDistanceMaximum(value);
	return false;
}

AUD_API float AUD_Handle_getDistanceReference(AUD_Handle* handle)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->getDistanceReference();
	return 0.0f;
}

AUD_API int AUD_Handle_setDistanceReference(AUD_Handle* handle, float value)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->setDistanceReference(value);
	return false;
}

AUD_API int AUD_Handle_doesKeep(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->getKeep();
}

AUD_API int AUD_Handle_setKeep(AUD_Handle* handle, int value)
{
	assert(handle);
	return (*handle)->setKeep(value);
}

AUD_API int AUD_Handle_getLocation(AUD_Handle* handle, float value[3])
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
	{
		Vector3 v = h->getLocation();
		value[0] = v.x();
		value[1] = v.y();
		value[2] = v.z();
		return true;
	}
	return false;
}

AUD_API int AUD_Handle_setLocation(AUD_Handle* handle, const float value[3])
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
	{
		Vector3 v = Vector3(value[0], value[1], value[2]);
		return h->setLocation(v);
	}
	return false;
}

AUD_API int AUD_Handle_getLoopCount(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->getLoopCount();
}

AUD_API int AUD_Handle_setLoopCount(AUD_Handle* handle, int value)
{
	assert(handle);
	return (*handle)->setLoopCount(value);
}

AUD_API int AUD_Handle_getOrientation(AUD_Handle* handle, float value[4])
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
	{
		Quaternion v = h->getOrientation();
		value[0] = v.x();
		value[1] = v.y();
		value[2] = v.z();
		value[3] = v.w();
		return true;
	}
	return false;
}

AUD_API int AUD_Handle_setOrientation(AUD_Handle* handle, const float value[4])
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
	{
		Quaternion v(value[3], value[0], value[1], value[2]);
		return h->setOrientation(v);
	}
	return false;
}

AUD_API float AUD_Handle_getPitch(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->getPitch();
}

AUD_API int AUD_Handle_setPitch(AUD_Handle* handle, float value)
{
	assert(handle);
	return (*handle)->setPitch(value);
}

AUD_API float AUD_Handle_getPosition(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->getPosition();
}

AUD_API int AUD_Handle_setPosition(AUD_Handle* handle, float value)
{
	assert(handle);
	return (*handle)->seek(value);
}

AUD_API int AUD_Handle_isRelative(AUD_Handle* handle)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->isRelative();
	return true;
}

AUD_API int AUD_Handle_setRelative(AUD_Handle* handle, int value)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->setRelative(value);
	return false;
}

AUD_API AUD_Status AUD_Handle_getStatus(AUD_Handle* handle)
{
	assert(handle);
	return static_cast<AUD_Status>((*handle)->getStatus());
}

AUD_API int AUD_Handle_getVelocity(AUD_Handle* handle, float value[3])
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
	{
		Vector3 v = h->getVelocity();
		value[0] = v.x();
		value[1] = v.y();
		value[2] = v.z();
		return true;
	}
	return false;
}

AUD_API int AUD_Handle_setVelocity(AUD_Handle* handle, const float value[3])
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
	{
		Vector3 v = Vector3(value[0], value[1], value[2]);
		return h->setVelocity(v);
	}
	return false;
}

AUD_API float AUD_Handle_getVolume(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->getVolume();
}

AUD_API int AUD_Handle_setVolume(AUD_Handle* handle, float value)
{
	assert(handle);
	return (*handle)->setVolume(value);
}

AUD_API float AUD_Handle_getVolumeMaximum(AUD_Handle* handle)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->getVolumeMaximum();
	return 0.0f;
}

AUD_API int AUD_Handle_setVolumeMaximum(AUD_Handle* handle, float value)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->setVolumeMaximum(value);
	return false;
}

AUD_API float AUD_Handle_getVolumeMinimum(AUD_Handle* handle)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->getVolumeMinimum();
	return 0.0f;
}

AUD_API int AUD_Handle_setVolumeMinimum(AUD_Handle* handle, float value)
{
	assert(handle);
	std::shared_ptr<I3DHandle> h = std::dynamic_pointer_cast<I3DHandle>(*handle);

	if(h.get())
		return h->setVolumeMinimum(value);
	return false;
}

AUD_API void AUD_Handle_free(AUD_Handle* handle)
{
	delete handle;
}
