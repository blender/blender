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

/** \file audaspace/FX/AUD_EffectReader.h
 *  \ingroup audfx
 */


#ifndef __AUD_EFFECTREADER_H__
#define __AUD_EFFECTREADER_H__

#include "AUD_IReader.h"

#include <boost/shared_ptr.hpp>

/**
 * This reader is a base class for all effect readers that take one other reader
 * as input.
 */
class AUD_EffectReader : public AUD_IReader
{
private:
	// hide copy constructor and operator=
	AUD_EffectReader(const AUD_EffectReader&);
	AUD_EffectReader& operator=(const AUD_EffectReader&);

protected:
	/**
	 * The reader to read from.
	 */
	boost::shared_ptr<AUD_IReader> m_reader;

public:
	/**
	 * Creates a new effect reader.
	 * \param reader The reader to read from.
	 */
	AUD_EffectReader(boost::shared_ptr<AUD_IReader> reader);

	/**
	 * Destroys the reader.
	 */
	virtual ~AUD_EffectReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_EFFECTREADER_H__
