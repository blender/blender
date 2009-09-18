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

#ifndef AUD_SNDFILEFACTORY
#define AUD_SNDFILEFACTORY

#include "AUD_IFactory.h"
#include "AUD_Reference.h"
class AUD_Buffer;

/**
 * This factory reads a sound file via libsndfile.
 */
class AUD_SndFileFactory : public AUD_IFactory
{
private:
	/**
	 * The filename of the sound source file.
	 */
	char* m_filename;

	/**
	 * The buffer to read from.
	 */
	AUD_Reference<AUD_Buffer> m_buffer;

public:
	/**
	 * Creates a new factory.
	 * \param filename The sound file path.
	 */
	AUD_SndFileFactory(const char* filename);

	/**
	 * Creates a new factory.
	 * \param buffer The buffer to read from.
	 * \param size The size of the buffer.
	 */
	AUD_SndFileFactory(unsigned char* buffer, int size);

	/**
	 * Destroys the factory.
	 */
	~AUD_SndFileFactory();

	virtual AUD_IReader* createReader();
};

#endif //AUD_SNDFILEFACTORY
