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

/** \file audaspace/sndfile/AUD_SndFileWriter.h
 *  \ingroup audsndfile
 */


#ifndef __AUD_SNDFILEWRITER_H__
#define __AUD_SNDFILEWRITER_H__

#include "AUD_IWriter.h"

#include <string>
#include <sndfile.h>

/**
 * This class writes a sound file via libsndfile.
 */
class AUD_SndFileWriter : public AUD_IWriter
{
private:
	/**
	 * The current position in samples.
	 */
	int m_position;

	/**
	 * The specification of the audio data.
	 */
	AUD_DeviceSpecs m_specs;

	/**
	 * The sndfile.
	 */
	SNDFILE* m_sndfile;

	// hide copy constructor and operator=
	AUD_SndFileWriter(const AUD_SndFileWriter&);
	AUD_SndFileWriter& operator=(const AUD_SndFileWriter&);

public:
	/**
	 * Creates a new writer.
	 * \param filename The path to the file to be read.
	 * \param specs The file's audio specification.
	 * \param format The file's container format.
	 * \param codec The codec used for encoding the audio data.
	 * \param bitrate The bitrate for encoding.
	 * \exception AUD_Exception Thrown if the file specified cannot be written
	 *                          with libsndfile.
	 */
	AUD_SndFileWriter(std::string filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate);

	/**
	 * Destroys the writer and closes the file.
	 */
	virtual ~AUD_SndFileWriter();

	virtual int getPosition() const;
	virtual AUD_DeviceSpecs getSpecs() const;
	virtual void write(unsigned int length, sample_t* buffer);
};

#endif //__AUD_SNDFILEWRITER_H__
