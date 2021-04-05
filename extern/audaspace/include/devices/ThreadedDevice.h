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
 * @file ThreadedDevice.h
 * @ingroup plugin
 * The ThreadedDevice class.
 */

#include "devices/SoftwareDevice.h"

#include <thread>

AUD_NAMESPACE_BEGIN

/**
 * This device extends the SoftwareDevice with code for running mixing in a separate thread.
 */
class AUD_PLUGIN_API ThreadedDevice : public SoftwareDevice
{
private:
	/**
	 * Whether there is currently playback.
	 */
	bool m_playing;

	/**
	 * Whether the current playback should stop.
	 */
	bool m_stop;

	/**
	 * The streaming thread.
	 */
	std::thread m_thread;

	/**
	 * Starts the streaming thread.
	 */
	AUD_LOCAL void start();

	/**
	 * Streaming thread main function.
	 */
	AUD_LOCAL virtual void runMixingThread()=0;

	// delete copy constructor and operator=
	ThreadedDevice(const ThreadedDevice&) = delete;
	ThreadedDevice& operator=(const ThreadedDevice&) = delete;

protected:
	virtual void playing(bool playing);

	/**
	 * Empty default constructor. To setup the device call the function create()
	 * and to uninitialize call destroy().
	 */
	ThreadedDevice();

	/**
	 * Indicates that the mixing thread should be stopped.
	 * \return Whether the mixing thread should be stopping.
	 * \warning For thread safety, the device needs to be locked, when this method is called.
	 */
	inline bool shouldStop() { return m_stop; }

	/**
	 * This method needs to be called when the mixing thread is stopping.
	 * \warning For thread safety, the device needs to be locked, when this method is called.
	 */
	inline void doStop() { m_stop = m_playing = false; }

	/**
	 * Stops all playback and notifies the mixing thread to stop.
	 * \warning The device has to be unlocked to not run into a deadlock.
	 */
	void stopMixingThread();
};

AUD_NAMESPACE_END
