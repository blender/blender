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

/** \file audaspace/jack/AUD_JackDevice.h
 *  \ingroup audjack
 */


#ifndef __AUD_JACKDEVICE_H__
#define __AUD_JACKDEVICE_H__


#include "AUD_SoftwareDevice.h"
#include "AUD_Buffer.h"

#include <string>

#if defined(__APPLE__) // always first include for jack weaklinking !
#include <weakjack.h>
#endif

#include <jack.h>
#include <ringbuffer.h>

typedef void (*AUD_syncFunction)(void*, int, float);

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
	AUD_Buffer m_buffer;

	/**
	 * The deinterleaving buffer.
	 */
	AUD_Buffer m_deinterleavebuf;

	jack_ringbuffer_t** m_ringbuffers;

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

	static int jack_sync(jack_transport_state_t state, jack_position_t* pos, void* data);

	/**
	 * Next Jack Transport state (-1 if not expected to change).
	 */
	jack_transport_state_t m_nextState;

	/**
	 * Current jack transport status.
	 */
	jack_transport_state_t m_state;

	/**
	 * Syncronisation state.
	 */
	int m_sync;

	/**
	 * External syncronisation callback function.
	 */
	AUD_syncFunction m_syncFunc;

	/**
	 * Data for the sync function.
	 */
	void* m_syncFuncData;

	/**
	 * The mixing thread.
	 */
	pthread_t m_mixingThread;

	/**
	 * Mutex for mixing.
	 */
	pthread_mutex_t m_mixingLock;

	/**
	 * Condition for mixing.
	 */
	pthread_cond_t m_mixingCondition;

	/**
	 * Mixing thread function.
	 * \param device The this pointer.
	 * \return NULL.
	 */
	static void* runMixingThread(void* device);

	/**
	 * Updates the ring buffers.
	 */
	void updateRingBuffers();

	// hide copy constructor and operator=
	AUD_JackDevice(const AUD_JackDevice&);
	AUD_JackDevice& operator=(const AUD_JackDevice&);

protected:
	virtual void playing(bool playing);

public:
	/**
	 * Creates a Jack client for audio output.
	 * \param name The client name.
	 * \param specs The wanted audio specification, where only the channel count
	 *              is important.
	 * \param buffersize The size of the internal buffer.
	 * \exception AUD_Exception Thrown if the audio device cannot be opened.
	 */
	AUD_JackDevice(std::string name, AUD_DeviceSpecs specs, int buffersize = AUD_DEFAULT_BUFFER_SIZE);

	/**
	 * Closes the Jack client.
	 */
	virtual ~AUD_JackDevice();

	/**
	 * Starts jack transport playback.
	 */
	void startPlayback();

	/**
	 * Stops jack transport playback.
	 */
	void stopPlayback();

	/**
	 * Seeks jack transport playback.
	 * \param time The time to seek to.
	 */
	void seekPlayback(float time);

	/**
	 * Sets the sync callback for jack transport playback.
	 * \param sync The callback function.
	 * \param data The data for the function.
	 */
	void setSyncCallback(AUD_syncFunction sync, void* data);

	/**
	 * Retrieves the jack transport playback time.
	 * \return The current time position.
	 */
	float getPlaybackPosition();

	/**
	 * Returns whether jack transport plays back.
	 * \return Whether jack transport plays back.
	 */
	bool doesPlayback();
};

#endif //__AUD_JACKDEVICE_H__
