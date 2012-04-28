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

/** \file audaspace/intern/AUD_FileWriter.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_FILEWRITER_H__
#define __AUD_FILEWRITER_H__

#include <string>
#include <vector>

#include "AUD_Reference.h"

#include "AUD_IWriter.h"
#include "AUD_IReader.h"

/**
 * This class is able to create IWriter classes as well as write reads to them.
 */
class AUD_FileWriter
{
private:
	// hide default constructor, copy constructor and operator=
	AUD_FileWriter();
	AUD_FileWriter(const AUD_FileWriter&);
	AUD_FileWriter& operator=(const AUD_FileWriter&);

public:
	/**
	 * Creates a new IWriter.
	 * \param filename The file to write to.
	 * \param specs The file's audio specification.
	 * \param format The file's container format.
	 * \param codec The codec used for encoding the audio data.
	 * \param bitrate The bitrate for encoding.
	 * \return The writer to write data to.
	 */
	static AUD_Reference<AUD_IWriter> createWriter(std::string filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate);

	/**
	 * Writes a reader to a writer.
	 * \param reader The reader to read from.
	 * \param writer The writer to write to.
	 * \param length How many samples should be transfered.
	 * \param buffersize How many samples should be transfered at once.
	 */
	static void writeReader(AUD_Reference<AUD_IReader> reader, AUD_Reference<AUD_IWriter> writer, unsigned int length, unsigned int buffersize);

	/**
	 * Writes a reader to several writers.
	 * \param reader The reader to read from.
	 * \param writers The writers to write to.
	 * \param length How many samples should be transfered.
	 * \param buffersize How many samples should be transfered at once.
	 */
	static void writeReader(AUD_Reference<AUD_IReader> reader, std::vector<AUD_Reference<AUD_IWriter> >& writers, unsigned int length, unsigned int buffersize);
};

#endif //__AUD_FILEWRITER_H__
