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

#include "AUD_AccumulatorFactory.h"
#include "AUD_CallbackIIRFilterReader.h"

sample_t accumulatorFilterAdditive(AUD_CallbackIIRFilterReader* reader, void* useless)
{
	float in = reader->x(0);
	float lastin = reader->x(-1);
	float out = reader->y(-1) + in - lastin;
	if(in > lastin)
		out += in - lastin;
	return out;
}

sample_t accumulatorFilter(AUD_CallbackIIRFilterReader* reader, void* useless)
{
	float in = reader->x(0);
	float lastin = reader->x(-1);
	float out = reader->y(-1);
	if(in > lastin)
		out += in - lastin;
	return out;
}

AUD_AccumulatorFactory::AUD_AccumulatorFactory(AUD_IFactory* factory,
											   bool additive) :
		AUD_EffectFactory(factory),
		m_additive(additive)
{
}

AUD_IReader* AUD_AccumulatorFactory::createReader() const
{
	return new AUD_CallbackIIRFilterReader(getReader(), 2, 2,
							m_additive ? accumulatorFilterAdditive : accumulatorFilter);
}
