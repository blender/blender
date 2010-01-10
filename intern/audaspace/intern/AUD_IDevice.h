/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_IDEVICE
#define AUD_IDEVICE

#include "AUD_Space.h"
class AUD_IFactory;

/// Handle structure, for inherition.
typedef struct
{
} AUD_Handle;

/**
 * This class represents an output device for sound sources.
 * Output devices may be several backends such as plattform independand like
 * SDL or OpenAL or plattform specific like DirectSound, but they may also be
 * files, RAM buffers or other types of streams.
 * \warning Thread safety must be insured so that no reader is beeing called
 *          twice at the same time.
 */
class AUD_IDevice
{
public:
	/**
	 * Destroys the device.
	 */
	virtual ~AUD_IDevice() {}

	/**
	 * Returns the specification of the device.
	 */
	virtual AUD_DeviceSpecs getSpecs()=0;

	/**
	 * Plays a sound source.
	 * \param factory The factory to create the reader for the sound source.
	 * \param keep When keep is true the sound source will not be deleted but
	 *             set to paused when its end has been reached.
	 * \return Returns a handle with which the playback can be controlled.
	 *         This is NULL if the sound couldn't be played back.
	 * \exception AUD_Exception Thrown if there's an unexpected (from the
	 *            device side) error during creation of the reader.
	 */
	virtual AUD_Handle* play(AUD_IFactory* factory, bool keep = false)=0;

	/**
	 * Pauses a played back sound.
	 * \param handle The handle returned by the play function.
	 * \return
	 *        - true if the sound has been paused.
	 *        - false if the sound isn't playing back or the handle is invalid.
	 */
	virtual bool pause(AUD_Handle* handle)=0;

	/**
	 * Resumes a paused sound.
	 * \param handle The handle returned by the play function.
	 * \return
	 *        - true if the sound has been resumed.
	 *        - false if the sound isn't paused or the handle is invalid.
	 */
	virtual bool resume(AUD_Handle* handle)=0;

	/**
	 * Stops a played back or paused sound. The handle is definitely invalid
	 * afterwards.
	 * \param handle The handle returned by the play function.
	 * \return
	 *        - true if the sound has been stopped.
	 *        - false if the handle is invalid.
	 */
	virtual bool stop(AUD_Handle* handle)=0;

	/**
	 * Sets the behaviour of the device for a played back sound when the sound
	 * doesn't return any more samples.
	 * \param handle The handle returned by the play function.
	 * \param keep True when the source should be paused and not deleted.
	 * \return
	 *        - true if the behaviour has been changed.
	 *        - false if the handle is invalid.
	 */
	virtual bool setKeep(AUD_Handle* handle, bool keep)=0;

	/**
	 * Sends a message to a sound or all sounds that are currently played or
	 * paused.
	 * \param handle The sound that should receive the message or NULL if all
	 *        sounds should receive it.
	 * \param message The message.
	 * \return True if the message has been read by at least one sound.
	 */
	virtual bool sendMessage(AUD_Handle* handle, AUD_Message &message)=0;

	/**
	 * Seeks in a played back sound.
	 * \param handle The handle returned by the play function.
	 * \param position The new position from where to play back, in seconds.
	 * \return
	 *        - true if the handle is valid.
	 *        - false if the handle is invalid.
	 * \warning Whether the seek works or not depends on the sound source.
	 */
	virtual bool seek(AUD_Handle* handle, float position)=0;

	/**
	 * Retrieves the current playback position of a sound.
	 * \param handle The handle returned by the play function.
	 * \return The playback position in seconds, or 0.0 if the handle is
	 *         invalid.
	 */
	virtual float getPosition(AUD_Handle* handle)=0;

	/**
	 * Returns the status of a played back sound.
	 * \param handle The handle returned by the play function.
	 * \return
	 *        - AUD_STATUS_INVALID if the sound has stopped or the handle is
	 *.         invalid
	 *        - AUD_STATUS_PLAYING if the sound is currently played back.
	 *        - AUD_STATUS_PAUSED if the sound is currently paused.
	 * \see AUD_Status
	 */
	virtual AUD_Status getStatus(AUD_Handle* handle)=0;

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
	 * Checks if a specific capability as available on a device.
	 * \param capability The capability.
	 * \return Whether it is available or not.
	 */
	virtual bool checkCapability(int capability)=0;

	/**
	 * Set a value of a capability. The data behind the pointer depends on the
	 * capability.
	 * \param capability The capability.
	 * \param value The value.
	 * \return Whether the action succeeded or not.
	 */
	virtual bool setCapability(int capability, void *value)=0;

	/**
	 * Retrieves a value of a capability. The data behind the pointer depends on
	 * the capability.
	 * \param capability The capability.
	 * \param value The value.
	 * \return Whether the action succeeded or not.
	 */
	virtual bool getCapability(int capability, void *value)=0;
};

#endif //AUD_IDevice
