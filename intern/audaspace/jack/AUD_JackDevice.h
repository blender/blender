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

#ifndef AUD_JACKDEVICE
#define AUD_JACKDEVICE


#include "AUD_SoftwareDevice.h"
class AUD_Buffer;

#include <jack.h>

/**
 * This device plays back through Jack.
 */
class AUD_JackDevice : public AUD_SoftwareDevice
{
private:
	/**
	 * The output ports of jack.
	 */
	jack_port_t** m_ports;

	/**
	 * The jack client.
	 */
	jack_client_t* m_client;

	/**
	 * The output buffer.
	 */
	AUD_Buffer* m_buffer;

	/**
	 * Whether the device is valid.
	 */
	bool m_valid;

	/**
	 * Invalidates the jack device.
	 * \param data The jack device that gets invalidet by jack.
	 */
	static void jack_shutdown(void *data);

	/**
	 * Mixes the next bytes into the buffer.
	 * \param length The length in samples to be filled.
	 * \param data A pointer to the jack device.
	 * \return 0 what shows success.
	 */
	static int jack_mix(jack_nframes_t length, void *data);

protected:
	virtual void playing(bool playing);

public:
	/**
	 * Creates a Jack client for audio output.
	 * \param specs The wanted audio specification, where only the channel count is important.
	 * \exception AUD_Exception Thrown if the audio device cannot be opened.
	 */
	AUD_JackDevice(AUD_Specs specs);

	/**
	 * Closes the Jack client.
	 */
	virtual ~AUD_JackDevice();
};

#endif //AUD_JACKDEVICE
