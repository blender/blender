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

/** \file audaspace/sndfile/AUD_SndFileReader.h
 *  \ingroup audsndfile
 */


#ifndef __AUD_SNDFILEREADER_H__
#define __AUD_SNDFILEREADER_H__

#include "AUD_IReader.h"
#include "AUD_Reference.h"
#include "AUD_Buffer.h"

#include <string>
#include <sndfile.h>

typedef sf_count_t (*sf_read_f)(SNDFILE *sndfile, void *ptr, sf_count_t frames);

/**
 * This class reads a sound file via libsndfile.
 */
class AUD_SndFileReader : public AUD_IReader
{
private:
	/**
	 * The current position in samples.
	 */
	int m_position;

	/**
	 * The sample count in the file.
	 */
	int m_length;

	/**
	 * Whether the file is seekable.
	 */
	bool m_seekable;

	/**
	 * The specification of the audio data.
	 */
	AUD_Specs m_specs;

	/**
	 * The sndfile.
	 */
	SNDFILE* m_sndfile;

	/**
	 * The virtual IO structure for memory file reading.
	 */
	SF_VIRTUAL_IO m_vio;

	/**
	 * The pointer to the memory file.
	 */
	AUD_Reference<AUD_Buffer> m_membuffer;

	/**
	 * The current reading pointer of the memory file.
	 */
	int m_memoffset;

	// Functions for libsndfile virtual IO functionality
	static sf_count_t vio_get_filelen(void *user_data);
	static sf_count_t vio_seek(sf_count_t offset, int whence, void *user_data);
	static sf_count_t vio_read(void *ptr, sf_count_t count, void *user_data);
	static sf_count_t vio_tell(void *user_data);

	// hide copy constructor and operator=
	AUD_SndFileReader(const AUD_SndFileReader&);
	AUD_SndFileReader& operator=(const AUD_SndFileReader&);

public:
	/**
	 * Creates a new reader.
	 * \param filename The path to the file to be read.
	 * \exception AUD_Exception Thrown if the file specified does not exist or
	 *            cannot be read with libsndfile.
	 */
	AUD_SndFileReader(std::string filename);

	/**
	 * Creates a new reader.
	 * \param buffer The buffer to read from.
	 * \exception AUD_Exception Thrown if the buffer specified cannot be read
	 *                          with libsndfile.
	 */
	AUD_SndFileReader(AUD_Reference<AUD_Buffer> buffer);

	/**
	 * Destroys the reader and closes the file.
	 */
	virtual ~AUD_SndFileReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_SNDFILEREADER_H__
