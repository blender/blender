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

/** \file audaspace/intern/AUD_IReader.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_IREADER_H__
#define __AUD_IREADER_H__

#include "AUD_Space.h"

/**
 * This class represents a sound source as stream or as buffer which can be read
 * for example by another reader, a device or whatever.
 */
class AUD_IReader
{
public:
	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_IReader() {}

	/**
	 * Tells whether the source provides seeking functionality or not.
	 * \warning This doesn't mean that the seeking always has to succeed.
	 * \return Always returns true for readers of buffering types.
	 */
	virtual bool isSeekable() const=0;

	/**
	 * Seeks to a specific position in the source.
	 * \param position The position to seek for measured in samples. To get
	 *        from a given time to the samples you simply have to multiply the
	 *        time value in seconds with the sample rate of the reader.
	 * \warning This may work or not, depending on the actual reader.
	 */
	virtual void seek(int position)=0;

	/**
	 * Returns an approximated length of the source in samples.
	 * \return The length as sample count. May be negative if unknown.
	 */
	virtual int getLength() const=0;

	/**
	 * Returns the position of the source as a sample count value.
	 * \return The current position in the source. A negative value indicates
	 *         that the position is unknown.
	 * \warning The value returned doesn't always have to be correct for readers,
	 *          especially after seeking.
	 */
	virtual int getPosition() const=0;

	/**
	 * Returns the specification of the reader.
	 * \return The AUD_Specs structure.
	 */
	virtual AUD_Specs getSpecs() const=0;

	/**
	 * Request to read the next length samples out of the source.
	 * The buffer supplied has the needed size.
	 * \param[in,out] length The count of samples that should be read. Shall
	 *                contain the real count of samples after reading, in case
	 *                there were only fewer samples available.
	 *                A smaller value also indicates the end of the reader.
	 * \param[out] eos End of stream, whether the end is reached or not.
	 * \param[in] buffer The pointer to the buffer to read into.
	 */
	virtual void read(int& length, bool& eos, sample_t* buffer)=0;
};

#endif //__AUD_IREADER_H__
