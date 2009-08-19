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

#ifndef AUD_STREAMBUFFERFACTORY
#define AUD_STREAMBUFFERFACTORY

#include "AUD_IFactory.h"
#include "AUD_Reference.h"
class AUD_Buffer;

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

public:
	/**
	 * Creates the factory and reads the reader created by the factory supplied
	 * to the buffer.
	 * \param factory The factory that creates the reader for buffering.
	 * \exception AUD_Exception Thrown if the reader cannot be created.
	 */
	AUD_StreamBufferFactory(AUD_IFactory* factory);

	virtual AUD_IReader* createReader();
};

#endif //AUD_STREAMBUFFERFACTORY
