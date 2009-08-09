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

#ifndef AUD_EFFECTREADER
#define AUD_EFFECTREADER

#include "AUD_IReader.h"

/**
 * This reader is a base class for all effect readers that take one other reader
 * as input.
 */
class AUD_EffectReader : public AUD_IReader
{
protected:
	/**
	 * The reader to read from.
	 */
	AUD_IReader* m_reader;

public:
	/**
	 * Creates a new effect reader.
	 * \param reader The reader to read from.
	 * \exception AUD_Exception Thrown if the reader specified is NULL.
	 */
	AUD_EffectReader(AUD_IReader* reader);

	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_EffectReader();

	virtual bool isSeekable();
	virtual void seek(int position);
	virtual int getLength();
	virtual int getPosition();
	virtual AUD_Specs getSpecs();
	virtual AUD_ReaderType getType();
	virtual bool notify(AUD_Message &message);
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_EFFECTREADER
