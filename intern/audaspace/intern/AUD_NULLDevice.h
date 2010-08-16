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
