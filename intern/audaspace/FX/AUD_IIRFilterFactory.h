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

/** \file audaspace/FX/AUD_IIRFilterFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_IIRFILTERFACTORY_H__
#define __AUD_IIRFILTERFACTORY_H__

#include "AUD_EffectFactory.h"

#include <vector>

/**
 * This factory creates a IIR filter reader.
 */
class AUD_IIRFilterFactory : public AUD_EffectFactory
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
	AUD_IIRFilterFactory(const AUD_IIRFilterFactory&);
	AUD_IIRFilterFactory& operator=(const AUD_IIRFilterFactory&);

public:
	/**
	 * Creates a new IIR filter factory.
	 * \param factory The input factory.
	 * \param b The input filter coefficients.
	 * \param a The output filter coefficients.
	 */
	AUD_IIRFilterFactory(AUD_Reference<AUD_IFactory> factory, std::vector<float> b,
						 std::vector<float> a);

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //__AUD_IIRFILTERFACTORY_H__
