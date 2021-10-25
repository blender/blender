/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_IHandle.h
 *  \ingroup audaspaceintern
 */

#ifndef __AUD_IHANDLE_H__
#define __AUD_IHANDLE_H__

//#include "AUD_Space.h"
//#include "AUD_Reference.h"

typedef void (*stopCallback)(void*);

/**
 * This class represents a playback handles for specific devices.
 */
class AUD_IHandle
{
public:
	/**
	 * Destroys the handle.
	 */
	virtual ~AUD_IHandle() {}

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
	 *        - AUD_STATUS_INVALID if the sound has stopped or the handle is
	 *.         invalid
	 *        - AUD_STATUS_PLAYING if the sound is currently played back.
	 *        - AUD_STATUS_PAUSED if the sound is currently paused.
	 *        - AUD_STATUS_STOPPED if the sound finished playing and is still
	 *          kept in the device.
	 * \see AUD_Status
	 */
	virtual AUD_Status getStatus()=0;

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

#endif //AUD_IHandle
