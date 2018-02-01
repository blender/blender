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
 * @file IHandle.h
 * @ingroup devices
 * Defines the IHandle interface as well as possible states of the handle.
 */

#include "Audaspace.h"

AUD_NAMESPACE_BEGIN

/// Status of a playback handle.
enum Status
{
	STATUS_INVALID = 0,			/// Invalid handle. Maybe due to stopping.
	STATUS_PLAYING,				/// Sound is playing.
	STATUS_PAUSED,				/// Sound is being paused.
	STATUS_STOPPED				/// Sound is stopped but kept in the device.
};

/**
 * The stopCallback is called when a handle reaches the end of the stream and
 * thus gets stopped. A user defined pointer is supplied to the callback.
 */
typedef void (*stopCallback)(void*);

/**
 * @interface IHandle
 * The IHandle interface represents a playback handles of a specific device.
 */
class AUD_API IHandle
{
public:
	/**
	 * Destroys the handle.
	 */
	virtual ~IHandle() {}

	/**
	 * Pauses a played back sound.
	 * \return
	 *        - true if the sound has been paused.
	 *        - false if the sound isn't playing back or the handle is invalid.
	 */
	virtual bool pause()=0;

	/**
	 * Resumes a paused sound.
	 * \return
	 *        - true if the sound has been resumed.
	 *        - false if the sound isn't paused or the handle is invalid.
	 */
	virtual bool resume()=0;

	/**
	 * Stops a played back or paused sound. The handle is definitely invalid
	 * afterwards.
	 * \return
	 *        - true if the sound has been stopped.
	 *        - false if the handle is invalid.
	 */
	virtual bool stop()=0;

	/**
	 * Gets the behaviour of the device for a played back sound when the sound
	 * doesn't return any more samples.
	 * \return
	 *        - true if the source will be paused when it's end is reached
	 *        - false if the handle won't kept or is invalid.
	 */
	virtual bool getKeep()=0;

	/**
	 * Sets the behaviour of the device for a played back sound when the sound
	 * doesn't return any more samples.
	 * \param keep True when the source should be paused and not deleted.
	 * \return
	 *        - true if the behaviour has been changed.
	 *        - false if the handle is invalid.
	 */
	virtual bool setKeep(bool keep)=0;

	/**
	 * Seeks in a played back sound.
	 * \param position The new position from where to play back, in seconds.
	 * \return
	 *        - true if the handle is valid.
	 *        - false if the handle is invalid.
	 * \warning Whether the seek works or not depends on the sound source.
	 */
	virtual bool seek(float position)=0;

	/**
	 * Retrieves the current playback position of a sound.
	 * \return The playback position in seconds, or 0.0 if the handle is
	 *         invalid.
	 */
	virtual float getPosition()=0;

	/**
	 * Returns the status of a played back sound.
	 * \return
	 *        - STATUS_INVALID if the sound has stopped or the handle is
	 *.         invalid
	 *        - STATUS_PLAYING if the sound is currently played back.
	 *        - STATUS_PAUSED if the sound is currently paused.
	 *        - STATUS_STOPPED if the sound finished playing and is still
	 *          kept in the device.
	 * \see Status
	 */
	virtual Status getStatus()=0;

	/**
	 * Retrieves the volume of a playing sound.
	 * \return The volume.
	 */
	virtual float getVolume()=0;

	/**
	 * Sets the volume of a playing sound.
	 * \param volume The volume.
	 * \return
	 *        - true if the handle is valid.
	 *        - false if the handle is invalid.
	 */
	virtual bool setVolume(float volume)=0;

	/**
	 * Retrieves the pitch of a playing sound.
	 * \return The pitch.
	 */
	virtual float getPitch()=0;

	/**
	 * Sets the pitch of a playing sound.
	 * \param pitch The pitch.
	 * \return
	 *        - true if the handle is valid.
	 *        - false if the handle is invalid.
	 */
	virtual bool setPitch(float pitch)=0;

	/**
	 * Retrieves the loop count of a playing sound.
	 * A negative value indicates infinity.
	 * \return The remaining loop count.
	 */
	virtual int getLoopCount()=0;

	/**
	 * Sets the loop count of a playing sound.
	 * A negative value indicates infinity.
	 * \param count The new loop count.
	 * \return
	 *        - true if the handle is valid.
	 *        - false if the handle is invalid.
	 */
	virtual bool setLoopCount(int count)=0;

	/**
	 * Sets the callback function that's called when the end of a playing sound
	 * is reached.
	 * \param callback The callback function.
	 * \param data The data that should be passed to the callback function.
	 * \return
	 *        - true if the handle is valid.
	 *        - false if the handle is invalid.
	 */
	virtual bool setStopCallback(stopCallback callback = 0, void* data = 0)=0;
};

AUD_NAMESPACE_END
