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

/** \file audaspace/FX/AUD_IDynamicIIRFilterCalculator.h
 *  \ingroup audfx
 */

#ifndef AUD_IDYNAMICIIRFILTERCALCULATOR_H
#define AUD_IDYNAMICIIRFILTERCALCULATOR_H

#include "AUD_Space.h"

#include <vector>

/**
 * This interface calculates dynamic filter coefficients which depend on the
 * sampling rate for AUD_DynamicIIRFilterReaders.
 */
class AUD_IDynamicIIRFilterCalculator
{
public:
	virtual ~AUD_IDynamicIIRFilterCalculator() {}

	/**
	 * Recalculates the filter coefficients.
	 * \param rate The sample rate of the audio data.
	 * \param[out] b The input filter coefficients.
	 * \param[out] a The output filter coefficients.
	 */
	virtual void recalculateCoefficients(AUD_SampleRate rate,
										 std::vector<float>& b,
										 std::vector<float>& a)=0;
};

#endif // AUD_IDYNAMICIIRFILTERCALCULATOR_H
