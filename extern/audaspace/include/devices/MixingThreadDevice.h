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
 * @file MixingThreadDevice.h
 * @ingroup device
 * The MixingThreadDevice class.
 */

#include <condition_variable>
#include <thread>

#include "devices/SoftwareDevice.h"
#include "util/RingBuffer.h"

AUD_NAMESPACE_BEGIN

/**
 * This device extends the SoftwareDevice with code for running mixing in a separate thread.
 */
class AUD_PLUGIN_API MixingThreadDevice : public SoftwareDevice
{
private:
	/**
	 * Whether there is currently playback.
	 */
	volatile bool m_playback{false};

	/**
	 * The deinterleaving buffer.
	 */
	Buffer m_mixingBuffer;

	/**
	 * The mixing ring buffer.
	 */
	RingBuffer m_ringBuffer;

	/**
	 * Whether the device is valid.
	 */
	bool m_valid{false};

	/**
	 * The mixing thread.
	 */
	std::thread m_mixingThread;

	/**
	 * Mutex for mixing.
	 */
	std::mutex m_mixingLock;

	/**
	 * Condition for mixing.
	 */
	std::condition_variable m_mixingCondition;

	/**
	 * Updates the ring buffer.
	 */
	AUD_LOCAL void updateRingBuffer();

	// delete copy constructor and operator=
	MixingThreadDevice(const MixingThreadDevice&) = delete;
	MixingThreadDevice& operator=(const MixingThreadDevice&) = delete;

protected:
	/**
	 * Starts the streaming thread.
	 * @param buffersize Size of the ring buffer in bytes.
	 */
	void startMixingThread(size_t buffersize);

	/**
	 * Notify the mixing thread.
	 */
	void notifyMixingThread();

	/**
	 * Get ring buffer for reading.
	 */
	inline RingBuffer& getRingBuffer()
	{
		return m_ringBuffer;
	}

	/**
	 * Returns whether the thread is running or not.
	 */
	inline bool isMixingThreadRunning()
	{
		return m_valid;
	}

	virtual void playing(bool playing);

	/**
	 * Called every iteration in the mixing thread before mixing.
	 */
	virtual void preMixingWork(bool playing);

	/**
	 * Empty default constructor. To setup the device call the function create()
	 * and to uninitialize call destroy().
	 */
	MixingThreadDevice();

	/**
	 * Stops all playback and notifies the mixing thread to stop.
	 * \warning The device has to be unlocked to not run into a deadlock.
	 */
	void stopMixingThread();
};

AUD_NAMESPACE_END
