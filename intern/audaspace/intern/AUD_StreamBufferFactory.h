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

/** \file audaspace/intern/AUD_StreamBufferFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_STREAMBUFFERFACTORY_H__
#define __AUD_STREAMBUFFERFACTORY_H__

#include "AUD_IFactory.h"
#include "AUD_Reference.h"
#include "AUD_Buffer.h"

/**
 * This factory creates a buffer out of a reader. This way normally streamed
 * sound sources can be loaded into memory for buffered playback.
 */
class AUD_StreamBufferFactory : public AUD_IFactory
{
private:
	/**
	 * The buffer that holds the audio data.
	 */
	AUD_Reference<AUD_Buffer> m_buffer;

	/**
	 * The specification of the samples.
	 */
	AUD_Specs m_specs;

	// hide copy constructor and operator=
	AUD_StreamBufferFactory(const AUD_StreamBufferFactory&);
	AUD_StreamBufferFactory& operator=(const AUD_StreamBufferFactory&);

public:
	/**
	 * Creates the factory and reads the reader created by the factory supplied
	 * to the buffer.
	 * \param factory The factory that creates the reader for buffering.
	 * \exception AUD_Exception Thrown if the reader cannot be created.
	 */
	AUD_StreamBufferFactory(AUD_Reference<AUD_IFactory> factory);

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //__AUD_STREAMBUFFERFACTORY_H__
