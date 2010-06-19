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

#ifndef AUD_IREADER
#define AUD_IREADER

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
	virtual ~AUD_IReader(){}

	/**
	 * Tells whether the source provides seeking functionality or not.
	 * \warning This doesn't mean that the seeking always has to succeed.
	 * \return Always returns true for readers of the buffer type.
	 * \see getType
	 */
	virtual bool isSeekable()=0;

	/**
	 * Seeks to a specific position in the source.
	 * This function must work for buffer type readers.
	 * \param position The position to seek for measured in samples. To get
	 *        from a given time to the samples you simply have to multiply the
	 *        time value in seconds with the sample rate of the reader.
	 * \warning This may work or not, depending on the actual reader.
	 * \see getType
	 */
	virtual void seek(int position)=0;

	/**
	 * Returns an aproximated length of the source in samples.
	 * For readers of the type buffer this has to return a correct value!
	 * \return The length as sample count. May be negative if unknown.
	 * \see getType
	 */
	virtual int getLength()=0;

	/**
	 * Returns the position of the source as a sample count value.
	 * \return The current position in the source. A negative value indicates
	 *         that the position is unknown.
	 * \warning The value returned doesn't always have to be correct for readers
	 *          of the stream type, especially after seeking, it must though for
	 *          the buffer ones.
	 * \see getType
	 */
	virtual int getPosition()=0;

	/**
	 * Returns the specification of the reader.
	 * \return The AUD_Specs structure.
	 */
	virtual AUD_Specs getSpecs()=0;

	/**
	 * Returns the type of the reader. There are special conditions for the
	 * readers of the buffer type. Those have to return correct position and
	 * length values as well as they must be seekable.
	 * \return AUD_TYPE_BUFFER or AUD_TYPE_STREAM.
	 */
	virtual AUD_ReaderType getType()=0;

	/**
	 * Sends a message to this reader and if it has subreaders it broadcasts
	 * the message to them.
	 * \param message The message.
	 * \return Whether the message has been read by the reader or one of his
	 *         subreaders.
	 */
	virtual bool notify(AUD_Message &message)=0;

	/**
	 * Request to read the next length samples out of the source.
	 * The buffer for reading has to stay valid until the next call of this
	 * method or until the reader is deleted.
	 * \param[in,out] length The count of samples that should be read. Shall
	 *                contain the real count of samples after reading, in case
	 *                there were only fewer samples available.
	 *                A smaller value also indicates the end of the reader.
	 * \param[out] buffer The pointer to the buffer with the samples.
	 */
	virtual void read(int & length, sample_t* & buffer)=0;
};

#endif //AUD_IREADER
