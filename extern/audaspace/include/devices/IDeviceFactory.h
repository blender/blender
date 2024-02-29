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

#pragma once

/**
 * @file IDeviceFactory.h
 * @ingroup devices
 * The IDeviceFactory interface.
 */

#include "respec/Specification.h"

#include <memory>

AUD_NAMESPACE_BEGIN

/**
 * @interface IDeviceFactory
 * The IDeviceFactory interface opens an output device.
 */
class AUD_API IDeviceFactory
{
public:
	/**
	 * Destroys the device factory.
	 */
	virtual ~IDeviceFactory() {}

	/**
	 * Opens an audio device for playback.
	 * \exception Exception Thrown if the audio device cannot be opened.
	 */
	virtual std::shared_ptr<IDevice> openDevice()=0;

	/**
	 * Returns the priority of the device to be the default device for a system.
	 * The higher the priority the more likely it is for this device to be used as the default device.
	 * \return Priority to be the default device.
	 */
	virtual int getPriority()=0;

	/**
	 * Sets the wanted device specifications for opening the device.
	 * \param specs The wanted audio specification.
	 */
	virtual void setSpecs(DeviceSpecs specs)=0;

	/**
	 * Sets the size for the internal playback buffers.
	 * The bigger the buffersize, the less likely clicks happen,
	 * but the latency increases too.
	 * \param buffersize The size of the internal buffer.
	 */
	virtual void setBufferSize(int buffersize)=0;

	/**
	 * Sets a name for the device.
	 * \param name The internal name for the device.
	 */
	virtual void setName(const std::string &name)=0;
};

AUD_NAMESPACE_END
