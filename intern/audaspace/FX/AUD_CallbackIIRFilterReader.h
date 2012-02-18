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

/** \file audaspace/FX/AUD_CallbackIIRFilterReader.h
 *  \ingroup audfx
 */


#ifndef __AUD_CALLBACKIIRFILTERREADER_H__
#define __AUD_CALLBACKIIRFILTERREADER_H__

#include "AUD_BaseIIRFilterReader.h"
#include "AUD_Buffer.h"

class AUD_CallbackIIRFilterReader;

typedef sample_t (*doFilterIIR)(AUD_CallbackIIRFilterReader*, void*);
typedef void (*endFilterIIR)(void*);

/**
 * This class provides an interface for infinite impulse response filters via a
 * callback filter function.
 */
class AUD_CallbackIIRFilterReader : public AUD_BaseIIRFilterReader
{
private:
	/**
	 * Filter function.
	 */
	const doFilterIIR m_filter;

	/**
	 * End filter function.
	 */
	const endFilterIIR m_endFilter;

	/**
	 * Data pointer.
	 */
	void* m_data;

	// hide copy constructor and operator=
	AUD_CallbackIIRFilterReader(const AUD_CallbackIIRFilterReader&);
	AUD_CallbackIIRFilterReader& operator=(const AUD_CallbackIIRFilterReader&);

public:
	/**
	 * Creates a new callback IIR filter reader.
	 * \param reader The reader to read from.
	 * \param in The count of past input samples needed.
	 * \param out The count of past output samples needed.
	 * \param doFilter The filter callback.
	 * \param endFilter The finishing callback.
	 * \param data Data pointer for the callbacks.
	 */
	AUD_CallbackIIRFilterReader(AUD_Reference<AUD_IReader> reader, int in, int out,
								doFilterIIR doFilter,
								endFilterIIR endFilter = 0,
								void* data = 0);

	virtual ~AUD_CallbackIIRFilterReader();

	virtual sample_t filter();
};

#endif //__AUD_CALLBACKIIRFILTERREADER_H__
