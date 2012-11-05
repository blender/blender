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

/** \file audaspace/intern/AUD_NULLDevice.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_NULLDEVICE_H__
#define __AUD_NULLDEVICE_H__

#include "AUD_IReader.h"
#include "AUD_IDevice.h"
#include "AUD_IHandle.h"

/**
 * This device plays nothing.
 */
class AUD_NULLDevice : public AUD_IDevice
{
private:
	class AUD_NULLHandle : public AUD_IHandle
	{
	public:

		AUD_NULLHandle();

		virtual ~AUD_NULLHandle() {}
		virtual bool pause();
		virtual bool resume();
		virtual bool stop();
		virtual bool getKeep();
		virtual bool setKeep(bool keep);
		virtual bool seek(float position);
		virtual float getPosition();
		virtual AUD_Status getStatus();
		virtual float getVolume();
		virtual bool setVolume(float volume);
		virtual float getPitch();
		virtual bool setPitch(float pitch);
		virtual int getLoopCount();
		virtual bool setLoopCount(int count);
		virtual bool setStopCallback(stopCallback callback = 0, void* data = 0);
	};
public:
	/**
	 * Creates a new NULL device.
	 */
	AUD_NULLDevice();

	virtual ~AUD_NULLDevice();

	virtual AUD_DeviceSpecs getSpecs() const;
	virtual boost::shared_ptr<AUD_IHandle> play(boost::shared_ptr<AUD_IReader> reader, bool keep = false);
	virtual boost::shared_ptr<AUD_IHandle> play(boost::shared_ptr<AUD_IFactory> factory, bool keep = false);
	virtual void stopAll();
	virtual void lock();
	virtual void unlock();
	virtual float getVolume() const;
	virtual void setVolume(float volume);
};

#endif //__AUD_NULLDEVICE_H__
