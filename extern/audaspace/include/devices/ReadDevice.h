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
 * @file ReadDevice.h
 * @ingroup devices
 * The ReadDevice class.
 */

#include "devices/SoftwareDevice.h"

AUD_NAMESPACE_BEGIN

/**
 * This device enables to let the user read raw data out of it.
 */
class AUD_API ReadDevice : public SoftwareDevice
{
private:
	/**
	 * Whether the device is currently playing back.
	 */
	bool m_playing;

	// delete copy constructor and operator=
	ReadDevice(const ReadDevice&) = delete;
	ReadDevice& operator=(const ReadDevice&) = delete;

protected:
	virtual void AUD_LOCAL playing(bool playing);

public:
	/**
	 * Creates a new read device.
	 * \param specs The wanted audio specification.
	 */
	ReadDevice(DeviceSpecs specs);

	/**
	 * Creates a new read device.
	 * \param specs The wanted audio specification.
	 */
	ReadDevice(Specs specs);

	/**
	 * Closes the device.
	 */
	virtual ~ReadDevice();

	/**
	 * Reads the next bytes into the supplied buffer.
	 * \param buffer The target buffer.
	 * \param length The length in samples to be filled.
	 * \return True if the reading succeeded, false if there are no sounds
	 *         played back currently, in that case the buffer is filled with
	 *         silence.
	 */
	bool read(data_t* buffer, int length);

	/**
	 * Changes the output specification.
	 * \param specs The new audio data specification.
	 */
	void changeSpecs(Specs specs);
};

AUD_NAMESPACE_END
