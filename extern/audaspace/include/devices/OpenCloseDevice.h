/*******************************************************************************
 * Copyright 2009-2024 Jörg Müller
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
 * @file OpenCloseDevice.h
 * @ingroup devices
 * The OpenCloseDevice class.
 */

#include <thread>
#include <chrono>

#include "devices/SoftwareDevice.h"

AUD_NAMESPACE_BEGIN

/**
 * This device extends the SoftwareDevice with code for running mixing in a separate thread.
 */
class AUD_PLUGIN_API OpenCloseDevice : public SoftwareDevice
{
private:
	/**
	 * Whether the device is opened.
	 */
	bool m_device_opened{false};

	/**
	 * Whether there is currently playback.
	 */
	bool m_playing{false};

	/**
	 * Whether thread released the device.
	 */
	bool m_delayed_close_finished{false};

	/**
	 * Thread used to release the device after time delay.
	 */
	std::thread m_delayed_close_thread;

	/**
	 * How long to wait until closing the device..
	 */
	std::chrono::milliseconds m_device_close_delay{std::chrono::milliseconds(10000)};

	/**
	 * Time when playback has stopped.
	 */
	std::chrono::time_point<std::chrono::steady_clock> m_playback_stopped_time;

	/**
	 * Releases the device after time delay.
	 */
	void closeAfterDelay();

	/**
	 * Starts the playback.
	 */
	AUD_LOCAL virtual void start() = 0;

	/**
	 * Stops the playbsck.
	 */
	AUD_LOCAL virtual void stop() = 0;

	/**
	 * Acquires the device.
	 */
	AUD_LOCAL virtual void open() = 0;

	/**
	 * Releases the device.
	 */
	AUD_LOCAL virtual void close() = 0;

	// delete copy constructor and operator=
	OpenCloseDevice(const OpenCloseDevice&) = delete;
	OpenCloseDevice& operator=(const OpenCloseDevice&) = delete;

protected:
	OpenCloseDevice() = default;

	virtual void playing(bool playing);
};

AUD_NAMESPACE_END
