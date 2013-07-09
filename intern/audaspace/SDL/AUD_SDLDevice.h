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

/** \file audaspace/SDL/AUD_SDLDevice.h
 *  \ingroup audsdl
 */


#ifndef __AUD_SDLDEVICE_H__
#define __AUD_SDLDEVICE_H__

#include "AUD_SoftwareDevice.h"

#include <SDL.h>

/**
 * This device plays back through SDL, the simple direct media layer.
 */
class AUD_SDLDevice : public AUD_SoftwareDevice
{
private:
	/**
	 * Mixes the next bytes into the buffer.
	 * \param data The SDL device.
	 * \param buffer The target buffer.
	 * \param length The length in bytes to be filled.
	 */
	static void SDL_mix(void *data, Uint8* buffer, int length);

	// hide copy constructor and operator=
	AUD_SDLDevice(const AUD_SDLDevice&);
	AUD_SDLDevice& operator=(const AUD_SDLDevice&);

protected:
	virtual void playing(bool playing);

public:
	/**
	 * Opens the SDL audio device for playback.
	 * \param specs The wanted audio specification.
	 * \param buffersize The size of the internal buffer.
	 * \note The specification really used for opening the device may differ.
	 * \exception AUD_Exception Thrown if the audio device cannot be opened.
	 */
	AUD_SDLDevice(AUD_DeviceSpecs specs,
				  int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Closes the SDL audio device.
	 */
	virtual ~AUD_SDLDevice();
};

#endif //__AUD_SDLDEVICE_H__
