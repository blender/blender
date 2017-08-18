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
 * @file NULLDevice.h
 * @ingroup devices
 * The NULLDevice class.
 */

#include "devices/IDevice.h"
#include "devices/IHandle.h"

AUD_NAMESPACE_BEGIN

class IReader;

/**
 * This device plays nothing.
 * It is similar to the linux device /dev/null.
 */
class AUD_API NULLDevice : public IDevice
{
private:
	class AUD_LOCAL NULLHandle : public IHandle
	{
	private:
		// delete copy constructor and operator=
		NULLHandle(const NULLHandle&) = delete;
		NULLHandle& operator=(const NULLHandle&) = delete;

	public:

		NULLHandle();

		virtual ~NULLHandle() {}
		virtual bool pause();
		virtual bool resume();
		virtual bool stop();
		virtual bool getKeep();
		virtual bool setKeep(bool keep);
		virtual bool seek(float position);
		virtual float getPosition();
		virtual Status getStatus();
		virtual float getVolume();
		virtual bool setVolume(float volume);
		virtual float getPitch();
		virtual bool setPitch(float pitch);
		virtual int getLoopCount();
		virtual bool setLoopCount(int count);
		virtual bool setStopCallback(stopCallback callback = 0, void* data = 0);
	};

	// delete copy constructor and operator=
	NULLDevice(const NULLDevice&) = delete;
	NULLDevice& operator=(const NULLDevice&) = delete;

public:
	/**
	 * Creates a new NULLDevice.
	 */
	NULLDevice();

	virtual ~NULLDevice();

	virtual DeviceSpecs getSpecs() const;
	virtual std::shared_ptr<IHandle> play(std::shared_ptr<IReader> reader, bool keep = false);
	virtual std::shared_ptr<IHandle> play(std::shared_ptr<ISound> sound, bool keep = false);
	virtual void stopAll();
	virtual void lock();
	virtual void unlock();
	virtual float getVolume() const;
	virtual void setVolume(float volume);
	virtual ISynchronizer* getSynchronizer();

	/**
	 * Registers this plugin.
	 */
	static void registerPlugin();
};

AUD_NAMESPACE_END
