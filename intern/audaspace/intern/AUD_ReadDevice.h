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

/** \file audaspace/intern/AUD_ReadDevice.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_READDEVICE_H__
#define __AUD_READDEVICE_H__

#include "AUD_SoftwareDevice.h"

/**
 * This device enables to let the user read raw data out of it.
 */
class AUD_ReadDevice : public AUD_SoftwareDevice
{
private:
	/**
	 * Whether the device currently.
	 */
	bool m_playing;

	// hide copy constructor and operator=
	AUD_ReadDevice(const AUD_ReadDevice&);
	AUD_ReadDevice& operator=(const AUD_ReadDevice&);

protected:
	virtual void playing(bool playing);

public:
	/**
	 * Creates a new read device.
	 * \param specs The wanted audio specification.
	 */
	AUD_ReadDevice(AUD_DeviceSpecs specs);

	/**
	 * Creates a new read device.
	 * \param specs The wanted audio specification.
	 */
	AUD_ReadDevice(AUD_Specs specs);

	/**
	 * Closes the device.
	 */
	virtual ~AUD_ReadDevice();

	/**
	 * Reads the next bytes into the supplied buffer.
	 * \param buffer The target buffer.
	 * \param length The length in samples to be filled.
	 * \return True if the reading succeeded, false if there are no sounds
	 *         played back currently, in that case the buffer is filled with
	 *         silence.
	 */
	bool read(data_t* buffer, int length);

	/**
	 * Changes the output specification.
	 * \param specs The new audio data specification.
	 */
	void changeSpecs(AUD_Specs specs);
};

#endif //__AUD_READDEVICE_H__
