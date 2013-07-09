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

/** \file audaspace/intern/AUD_IDevice.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_IDEVICE_H__
#define __AUD_IDEVICE_H__

#include "AUD_Space.h"
#include "AUD_IFactory.h"
#include "AUD_IReader.h"
#include "AUD_IHandle.h"
#include "AUD_ILockable.h"

#include <boost/shared_ptr.hpp>

/**
 * This class represents an output device for sound sources.
 * Output devices may be several backends such as plattform independand like
 * SDL or OpenAL or plattform specific like DirectSound, but they may also be
 * files, RAM buffers or other types of streams.
 * \warning Thread safety must be insured so that no reader is beeing called
 *          twice at the same time.
 */
class AUD_IDevice : public AUD_ILockable
{
public:
	/**
	 * Destroys the device.
	 */
	virtual ~AUD_IDevice() {}

	/**
	 * Returns the specification of the device.
	 */
	virtual AUD_DeviceSpecs getSpecs() const=0;

	/**
	 * Plays a sound source.
	 * \param reader The reader to play.
	 * \param keep When keep is true the sound source will not be deleted but
	 *             set to paused when its end has been reached.
	 * \return Returns a handle with which the playback can be controlled.
	 *         This is NULL if the sound couldn't be played back.
	 * \exception AUD_Exception Thrown if there's an unexpected (from the
	 *            device side) error during creation of the reader.
	 */
	virtual boost::shared_ptr<AUD_IHandle> play(boost::shared_ptr<AUD_IReader> reader, bool keep = false)=0;

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
	virtual boost::shared_ptr<AUD_IHandle> play(boost::shared_ptr<AUD_IFactory> factory, bool keep = false)=0;

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
	 * \param handle The sound handle.
	 * \param volume The overall device volume.
	 */
	virtual void setVolume(float volume)=0;
};

#endif //AUD_IDevice
