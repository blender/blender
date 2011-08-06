/*
 * $Id$
 *
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


#ifndef AUD_FILEWRITER
#define AUD_FILEWRITER

#include <string>

#include "AUD_Reference.h"

#include "AUD_IWriter.h"
#include "AUD_IReader.h"

/**
 * This factory tries to read a sound file via all available file readers.
 */
class AUD_FileWriter
{
private:
	// hide default constructor, copy constructor and operator=
	AUD_FileWriter();
	AUD_FileWriter(const AUD_FileWriter&);
	AUD_FileWriter& operator=(const AUD_FileWriter&);

public:
	static AUD_Reference<AUD_IWriter> createWriter(std::string filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate);
	static void writeReader(AUD_Reference<AUD_IReader> reader, AUD_Reference<AUD_IWriter> writer, unsigned int length, unsigned int buffersize);
};

#endif //AUD_FILEWRITER
