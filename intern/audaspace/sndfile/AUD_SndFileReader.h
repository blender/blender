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

#ifndef AUD_SNDFILEREADER
#define AUD_SNDFILEREADER

#include "AUD_IReader.h"
#include "AUD_Reference.h"
class AUD_Buffer;

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
	 * The playback buffer.
	 */
	AUD_Buffer* m_buffer;

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

public:
	/**
	 * Creates a new reader.
	 * \param filename The path to the file to be read.
	 * \exception AUD_Exception Thrown if the file specified does not exist or
	 *            cannot be read with libsndfile.
	 */
	AUD_SndFileReader(const char* filename);

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

	virtual bool isSeekable();
	virtual void seek(int position);
	virtual int getLength();
	virtual int getPosition();
	virtual AUD_Specs getSpecs();
	virtual AUD_ReaderType getType();
	virtual bool notify(AUD_Message &message);
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_SNDFILEREADER
