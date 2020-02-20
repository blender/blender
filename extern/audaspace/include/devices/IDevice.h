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
 * @file IDevice.h
 * @ingroup devices
 * The IDevice interface.
 */

#include "respec/Specification.h"
#include "util/ILockable.h"

#include <memory>

AUD_NAMESPACE_BEGIN

class IHandle;
class IReader;
class ISound;
class ISynchronizer;

/**
 * @interface IDevice
 * The IDevice interface represents an output device for sound sources.
 * Output devices may be several backends such as platform independand like
 * SDL or OpenAL or platform specific like ALSA, but they may also be
 * files, RAM buffers or other types of streams.
 * \warning Thread safety must be insured so that no reader is being called
 *          twice at the same time.
 */
class IDevice : public ILockable
{
public:
	/**
	 * Destroys the device.
	 */
	virtual ~IDevice() {}

	/**
	 * Returns the specification of the device.
	 */
	virtual DeviceSpecs getSpecs() const=0;

	/**
	 * Plays a sound source.
	 * \param reader The reader to play.
	 * \param keep When keep is true the sound source will not be deleted but
	 *             set to paused when its end has been reached.
	 * \return Returns a handle with which the playback can be controlled.
	 *         This is nullptr if the sound couldn't be played back.
	 * \exception Exception Thrown if there's an unexpected (from the
	 *            device side) error during creation of the reader.
	 */
	virtual std::shared_ptr<IHandle> play(std::shared_ptr<IReader> reader, bool keep = false)=0;

	/**
	 * Plays a sound source.
	 * \param sound The sound to create the reader for the sound source.
	 * \param keep When keep is true the sound source will not be deleted but
	 *             set to paused when its end has been reached.
	 * \return Returns a handle with which the playback can be controlled.
	 *         This is nullptr if the sound couldn't be played back.
	 * \exception Exception Thrown if there's an unexpected (from the
	 *            device side) error during creation of the reader.
	 */
	virtual std::shared_ptr<IHandle> play(std::shared_ptr<ISound> sound, bool keep = false)=0;

	/**
	 * Stops all playing sounds.
	 */
	virtual void stopAll()=0;

	/**
	 * Locks the device.
	 * Used to make sure that between lock and unlock, no buffers are read, so
	 * that it is possible to start, resume, pause, stop or seek several
	 * playback handles simultaneously.
	 * \warning Make sure the locking time is as small as possible to avoid
	 *          playback delays that result in unexpected noise and cracks.
	 */
	virtual void lock()=0;

	/**
	 * Unlocks the previously locked device.
	 */
	virtual void unlock()=0;

	/**
	 * Retrieves the overall device volume.
	 * \return The overall device volume.
	 */
	virtual float getVolume() const=0;

	/**
	 * Sets the overall device volume.
	 * \param volume The overall device volume.
	 */
	virtual void setVolume(float volume)=0;

	/**
	 * Retrieves the synchronizer for this device, which enables accurate synchronization
	 * between audio playback and video playback for example.
	 * @return The synchronizer which will be the DefaultSynchronizer if synchonization is not supported.
	 */
	virtual ISynchronizer* getSynchronizer()=0;
};

AUD_NAMESPACE_END
