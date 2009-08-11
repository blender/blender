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

#ifndef AUD_CONVERTERREADER
#define AUD_CONVERTERREADER

#include "AUD_EffectReader.h"
#include "AUD_ConverterFunctions.h"
class AUD_Buffer;

/**
 * This class converts a sound source from one to another format.
 */
class AUD_ConverterReader : public AUD_EffectReader
{
private:
	/**
	 * The sound output buffer.
	 */
	AUD_Buffer *m_buffer;

	/**
	 * The target specification.
	 */
	AUD_Specs m_specs;

	/**
	 * Converter function.
	 */
	AUD_convert_f m_convert;

public:
	/**
	 * Creates a converter reader.
	 * \param reader The reader to convert.
	 * \param specs The target specification.
	 * \exception AUD_Exception Thrown if the reader is NULL.
	 */
	AUD_ConverterReader(AUD_IReader* reader, AUD_Specs specs);
	/**
	 * Destroys the reader.
	 */
	~AUD_ConverterReader();

	virtual AUD_Specs getSpecs();
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_CONVERTERREADER
