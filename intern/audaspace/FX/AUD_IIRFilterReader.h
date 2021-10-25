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

/** \file audaspace/FX/AUD_IIRFilterReader.h
 *  \ingroup audfx
 */


#ifndef __AUD_IIRFILTERREADER_H__
#define __AUD_IIRFILTERREADER_H__

#include "AUD_BaseIIRFilterReader.h"

#include <vector>

/**
 * This class is for infinite impulse response filters with simple coefficients.
 */
class AUD_IIRFilterReader : public AUD_BaseIIRFilterReader
{
private:
	/**
	 * Output filter coefficients.
	 */
	std::vector<float> m_a;

	/**
	 * Input filter coefficients.
	 */
	std::vector<float> m_b;

	// hide copy constructor and operator=
	AUD_IIRFilterReader(const AUD_IIRFilterReader&);
	AUD_IIRFilterReader& operator=(const AUD_IIRFilterReader&);

public:
	/**
	 * Creates a new IIR filter reader.
	 * \param reader The reader to read from.
	 * \param b The input filter coefficients.
	 * \param a The output filter coefficients.
	 */
	AUD_IIRFilterReader(boost::shared_ptr<AUD_IReader> reader, const std::vector<float>& b,
						const std::vector<float>& a);

	virtual sample_t filter();

	void setCoefficients(const std::vector<float>& b,
						 const std::vector<float>& a);
};

#endif //__AUD_IIRFILTERREADER_H__
