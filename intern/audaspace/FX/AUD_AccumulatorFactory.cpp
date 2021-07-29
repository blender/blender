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

/** \file audaspace/FX/AUD_AccumulatorFactory.cpp
 *  \ingroup audfx
 */


#include "AUD_AccumulatorFactory.h"
#include "AUD_CallbackIIRFilterReader.h"

sample_t AUD_AccumulatorFactory::accumulatorFilterAdditive(AUD_CallbackIIRFilterReader* reader, void* useless)
{
	float in = reader->x(0);
	float lastin = reader->x(-1);
	float out = reader->y(-1) + in - lastin;
	if(in > lastin)
		out += in - lastin;
	return out;
}

sample_t AUD_AccumulatorFactory::accumulatorFilter(AUD_CallbackIIRFilterReader* reader, void* useless)
{
	float in = reader->x(0);
	float lastin = reader->x(-1);
	float out = reader->y(-1);
	if(in > lastin)
		out += in - lastin;
	return out;
}

AUD_AccumulatorFactory::AUD_AccumulatorFactory(boost::shared_ptr<AUD_IFactory> factory,
											   bool additive) :
		AUD_EffectFactory(factory),
		m_additive(additive)
{
}

boost::shared_ptr<AUD_IReader> AUD_AccumulatorFactory::createReader()
{
	return boost::shared_ptr<AUD_IReader>(new AUD_CallbackIIRFilterReader(getReader(), 2, 2,
							m_additive ? accumulatorFilterAdditive : accumulatorFilter));
}
