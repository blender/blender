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

/** \file audaspace/FX/AUD_DynamicIIRFilterReader.h
 *  \ingroup audfx
 */

#ifndef __AUD_DYNAMICIIRFILTERREADER_H__
#define __AUD_DYNAMICIIRFILTERREADER_H__

#include "AUD_IIRFilterReader.h"
#include "AUD_IDynamicIIRFilterCalculator.h"

/**
 * This class is for dynamic infinite impulse response filters with simple
 * coefficients that change depending on the sample rate.
 */
class AUD_DynamicIIRFilterReader : public AUD_IIRFilterReader
{
private:
	/**
	 * The factory for dynamically recalculating filter coefficients.
	 */
	boost::shared_ptr<AUD_IDynamicIIRFilterCalculator> m_calculator;

public:
	AUD_DynamicIIRFilterReader(boost::shared_ptr<AUD_IReader> reader,
							   boost::shared_ptr<AUD_IDynamicIIRFilterCalculator> calculator);

	virtual void sampleRateChanged(AUD_SampleRate rate);
};

#endif // __AUD_DYNAMICIIRFILTERREADER_H__
