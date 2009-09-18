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
private:
	/**
	 * The specs of the device.
	 */
	AUD_Specs m_specs;

public:
	/**
	 * Creates a new NULL device.
	 */
	AUD_NULLDevice();

	virtual AUD_Specs getSpecs();
	virtual AUD_Handle* play(AUD_IFactory* factory, bool keep = false);
	virtual bool pause(AUD_Handle* handle);
	virtual bool resume(AUD_Handle* handle);
	virtual bool stop(AUD_Handle* handle);
	virtual bool setKeep(AUD_Handle* handle, bool keep);
	virtual bool sendMessage(AUD_Handle* handle, AUD_Message &message);
	virtual bool seek(AUD_Handle* handle, float position);
	virtual float getPosition(AUD_Handle* handle);
	virtual AUD_Status getStatus(AUD_Handle* handle);
	virtual void lock();
	virtual void unlock();
	virtual bool checkCapability(int capability);
	virtual bool setCapability(int capability, void *value);
	virtual bool getCapability(int capability, void *value);
};

#endif //AUD_NULLDEVICE
