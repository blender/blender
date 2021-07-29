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

/** \file audaspace/SRC/AUD_SRCResampleReader.h
 *  \ingroup audsrc
 */


#ifndef __AUD_SRCRESAMPLEREADER_H__
#define __AUD_SRCRESAMPLEREADER_H__

#include "AUD_ResampleReader.h"
#include "AUD_Buffer.h"

#include <samplerate.h>

/**
 * This resampling reader uses libsamplerate for resampling.
 */
class AUD_SRCResampleReader : public AUD_ResampleReader
{
private:
	/**
	 * The sound output buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The reader channels.
	 */
	AUD_Channels m_channels;

	/**
	 * The src state structure.
	 */
	SRC_STATE* m_src;

	/**
	 * The current playback position;
	 */
	int m_position;

	/**
	 * Whether reader reached end of stream.
	 */
	bool m_eos;

	// hide copy constructor and operator=
	AUD_SRCResampleReader(const AUD_SRCResampleReader&);
	AUD_SRCResampleReader& operator=(const AUD_SRCResampleReader&);

public:
	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param specs The target specification.
	 * \exception AUD_Exception Thrown if the source specification cannot be
	 *            resampled to the target specification.
	 */
	AUD_SRCResampleReader(AUD_Reference<AUD_IReader> reader, AUD_Specs specs);

	/**
	 * Destroys the reader.
	 */
	~AUD_SRCResampleReader();

	/**
	 * The callback function for SRC.
	 * \warning Do not call!
	 * \param data The pointer to the float data.
	 * \return The count of samples in the float data.
	 */
	long doCallback(float** data);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_SRCRESAMPLEREADER_H__
