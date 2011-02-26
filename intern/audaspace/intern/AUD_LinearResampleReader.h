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

/** \file audaspace/intern/AUD_LinearResampleReader.h
 *  \ingroup audaspaceintern
 */


#ifndef AUD_LINEARRESAMPLEREADER
#define AUD_LINEARRESAMPLEREADER

#include "AUD_EffectReader.h"
#include "AUD_Buffer.h"

/**
 * This resampling reader uses libsamplerate for resampling.
 */
class AUD_LinearResampleReader : public AUD_EffectReader
{
private:
	/**
	 * The sample specification of the source.
	 */
	const AUD_Specs m_sspecs;

	/**
	 * The resampling factor.
	 */
	const float m_factor;

	/**
	 * The target specification.
	 */
	AUD_Specs m_tspecs;

	/**
	 * The current position.
	 */
	int m_position;

	/**
	 * The current reading source position.
	 */
	int m_sposition;

	/**
	 * The sound output buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The input caching buffer.
	 */
	AUD_Buffer m_cache;

	// hide copy constructor and operator=
	AUD_LinearResampleReader(const AUD_LinearResampleReader&);
	AUD_LinearResampleReader& operator=(const AUD_LinearResampleReader&);

public:
	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param specs The target specification.
	 */
	AUD_LinearResampleReader(AUD_IReader* reader, AUD_Specs specs);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_LINEARRESAMPLEREADER
