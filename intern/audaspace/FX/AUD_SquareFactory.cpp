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

/** \file audaspace/FX/AUD_SquareFactory.cpp
 *  \ingroup audfx
 */


#include "AUD_SquareFactory.h"
#include "AUD_CallbackIIRFilterReader.h"

sample_t AUD_SquareFactory::squareFilter(AUD_CallbackIIRFilterReader* reader, float* threshold)
{
	float in = reader->x(0);
	if(in >= *threshold)
		return 1;
	else if(in <= -*threshold)
		return -1;
	else
		return 0;
}

void AUD_SquareFactory::endSquareFilter(float* threshold)
{
	delete threshold;
}

AUD_SquareFactory::AUD_SquareFactory(boost::shared_ptr<AUD_IFactory> factory, float threshold) :
		AUD_EffectFactory(factory),
		m_threshold(threshold)
{
}

float AUD_SquareFactory::getThreshold() const
{
	return m_threshold;
}

boost::shared_ptr<AUD_IReader> AUD_SquareFactory::createReader()
{
	return boost::shared_ptr<AUD_IReader>(new AUD_CallbackIIRFilterReader(getReader(), 1, 1,
										   (doFilterIIR) squareFilter,
										   (endFilterIIR) endSquareFilter,
										   new float(m_threshold)));
}
