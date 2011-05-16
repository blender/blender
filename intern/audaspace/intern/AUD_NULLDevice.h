/*
 * $Id$
 *
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


#ifndef AUD_NULLDEVICE
#define AUD_NULLDEVICE

#include "AUD_IDevice.h"

/**
 * This device plays nothing.
 */
class AUD_NULLDevice : public AUD_IDevice
{
public:
	/**
	 * Creates a new NULL device.
	 */
	AUD_NULLDevice();

	virtual AUD_DeviceSpecs getSpecs() const;
	virtual AUD_Handle* play(AUD_IReader* reader, bool keep = false);
	virtual AUD_Handle* play(AUD_IFactory* factory, bool keep = false);
	virtual bool pause(AUD_Handle* handle);
	virtual bool resume(AUD_Handle* handle);
	virtual bool stop(AUD_Handle* handle);
	virtual bool getKeep(AUD_Handle* handle);
	virtual bool setKeep(AUD_Handle* handle, bool keep);
	virtual bool seek(AUD_Handle* handle, float position);
	virtual float getPosition(AUD_Handle* handle);
	virtual AUD_Status getStatus(AUD_Handle* handle);
	virtual void lock();
	virtual void unlock();
	virtual float getVolume() const;
	virtual void setVolume(float volume);
	virtual float getVolume(AUD_Handle* handle);
	virtual bool setVolume(AUD_Handle* handle, float volume);
	virtual float getPitch(AUD_Handle* handle);
	virtual bool setPitch(AUD_Handle* handle, float pitch);
	virtual int getLoopCount(AUD_Handle* handle);
	virtual bool setLoopCount(AUD_Handle* handle, int count);
	virtual bool setStopCallback(AUD_Handle* handle, stopCallback callback = 0, void* data = 0);
};

#endif //AUD_NULLDEVICE
