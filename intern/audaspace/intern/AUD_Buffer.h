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

#ifndef AUD_BUFFER
#define AUD_BUFFER

#include "AUD_Space.h"

/**
 * This class is a simple buffer in RAM which is 16 Byte aligned and provides
 * resize functionality.
 */
class AUD_Buffer
{
private:
	/// The size of the buffer in bytes.
	int m_size;

	/// The pointer to the buffer memory.
	data_t* m_buffer;

public:
	/**
	 * Creates a new buffer.
	 * \param size The size of the buffer in bytes.
	 */
	AUD_Buffer(int size = 0);

	/**
	 * Destroys the buffer.
	 */
	~AUD_Buffer();

	/**
	 * Returns the pointer to the buffer in memory.
	 */
	sample_t* getBuffer();

	/**
	 * Returns the size of the buffer in bytes.
	 */
	int getSize();

	/**
	 * Resizes the buffer.
	 * \param size The new size of the buffer, measured in bytes.
	 * \param keep Whether to keep the old data. If the new buffer is smaller,
	 *        the data at the end will be lost.
	 */
	void resize(int size, bool keep = false);
};

#endif //AUD_BUFFER
