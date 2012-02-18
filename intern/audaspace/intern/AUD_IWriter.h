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

/** \file audaspace/intern/AUD_IWriter.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_IWRITER_H__
#define __AUD_IWRITER_H__

#include "AUD_Space.h"

/**
 * This class represents a sound sink where audio data can be written to.
 */
class AUD_IWriter
{
public:
	/**
	 * Destroys the writer.
	 */
	virtual ~AUD_IWriter(){}

	/**
	 * Returns how many samples have been written so far.
	 * \return The writing position as sample count. May be negative if unknown.
	 */
	virtual int getPosition() const=0;

	/**
	 * Returns the specification of the audio data being written into the sink.
	 * \return The AUD_DeviceSpecs structure.
	 * \note Regardless of the format the input still has to be float!
	 */
	virtual AUD_DeviceSpecs getSpecs() const=0;

	/**
	 * Request to write the next length samples out into the sink.
	 * \param length The count of samples to write.
	 * \param buffer The pointer to the buffer containing the data.
	 */
	virtual void write(unsigned int length, sample_t* buffer)=0;
};

#endif //__AUD_IWRITER_H__
