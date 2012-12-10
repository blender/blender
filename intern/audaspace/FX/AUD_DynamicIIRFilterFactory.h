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

/** \file audaspace/FX/AUD_DynamicIIRFilterFactory.h
 *  \ingroup audfx
 */

#ifndef __AUD_DYNAMICIIRFILTERFACTORY_H__
#define __AUD_DYNAMICIIRFILTERFACTORY_H__

#include "AUD_EffectFactory.h"
#include "AUD_IDynamicIIRFilterCalculator.h"
#include <vector>

/**
 * This factory creates a IIR filter reader.
 *
 * This means that on sample rate change the filter recalculates its
 * coefficients.
 */
class AUD_DynamicIIRFilterFactory : public AUD_EffectFactory
{
protected:
	boost::shared_ptr<AUD_IDynamicIIRFilterCalculator> m_calculator;

public:
	/**
	 * Creates a new Dynmic IIR filter factory.
	 * \param factory The input factory.
	 */
	AUD_DynamicIIRFilterFactory(boost::shared_ptr<AUD_IFactory> factory,
								boost::shared_ptr<AUD_IDynamicIIRFilterCalculator> calculator);

	virtual boost::shared_ptr<AUD_IReader> createReader();
};

#endif // __AUD_DYNAMICIIRFILTERFACTORY_H__
