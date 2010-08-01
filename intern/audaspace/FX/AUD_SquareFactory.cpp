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

#include "AUD_SquareFactory.h"
#include "AUD_CallbackIIRFilterReader.h"

sample_t squareFilter(AUD_CallbackIIRFilterReader* reader, float* threshold)
{
	float in = reader->x(0);
	if(in >= *threshold)
		return 1;
	else if(in <= -*threshold)
		return -1;
	else
		return 0;
}

void endSquareFilter(float* threshold)
{
	delete threshold;
}

AUD_SquareFactory::AUD_SquareFactory(AUD_IFactory* factory, float threshold) :
		AUD_EffectFactory(factory),
		m_threshold(threshold)
{
}

float AUD_SquareFactory::getThreshold() const
{
	return m_threshold;
}

AUD_IReader* AUD_SquareFactory::createReader() const
{
	return new AUD_CallbackIIRFilterReader(getReader(), 1, 1,
										   (doFilterIIR) squareFilter,
										   (endFilterIIR) endSquareFilter,
										   new float(m_threshold));
}
