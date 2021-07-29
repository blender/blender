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

/** \file audaspace/intern/AUD_ConverterReader.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_CONVERTERREADER_H__
#define __AUD_CONVERTERREADER_H__

#include "AUD_EffectReader.h"
#include "AUD_ConverterFunctions.h"
#include "AUD_Buffer.h"

/**
 * This class converts a sound source from one to another format.
 */
class AUD_ConverterReader : public AUD_EffectReader
{
private:
	/**
	 * The sound output buffer.
	 */
	AUD_Buffer m_buffer;

	/**
	 * The target specification.
	 */
	AUD_SampleFormat m_format;

	/**
	 * Converter function.
	 */
	AUD_convert_f m_convert;

	// hide copy constructor and operator=
	AUD_ConverterReader(const AUD_ConverterReader&);
	AUD_ConverterReader& operator=(const AUD_ConverterReader&);

public:
	/**
	 * Creates a converter reader.
	 * \param reader The reader to convert.
	 * \param specs The target specification.
	 */
	AUD_ConverterReader(boost::shared_ptr<AUD_IReader> reader, AUD_DeviceSpecs specs);

	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_CONVERTERREADER_H__
