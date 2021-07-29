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

/** \file audaspace/intern/AUD_Buffer.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_BUFFER_H__
#define __AUD_BUFFER_H__

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

	// hide copy constructor and operator=
	AUD_Buffer(const AUD_Buffer&);
	AUD_Buffer& operator=(const AUD_Buffer&);

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
	sample_t* getBuffer() const;

	/**
	 * Returns the size of the buffer in bytes.
	 */
	int getSize() const;

	/**
	 * Resizes the buffer.
	 * \param size The new size of the buffer, measured in bytes.
	 * \param keep Whether to keep the old data. If the new buffer is smaller,
	 *        the data at the end will be lost.
	 */
	void resize(int size, bool keep = false);

	/**
	 * Makes sure the buffer has a minimum size.
	 * If size is >= current size, nothing will happen.
	 * Otherwise the buffer is resized with keep as parameter.
	 * \param size The new minimum size of the buffer, measured in bytes.
	 * \param keep Whether to keep the old data. If the new buffer is smaller,
	 *        the data at the end will be lost.
	 */
	void assureSize(int size, bool keep = false);
};

#endif //__AUD_BUFFER_H__
