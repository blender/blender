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

/** \file audaspace/FX/AUD_EnvelopeFactory.h
 *  \ingroup audfx
 */


#ifndef __AUD_ENVELOPEFACTORY_H__
#define __AUD_ENVELOPEFACTORY_H__

#include "AUD_EffectFactory.h"
class AUD_CallbackIIRFilterReader;
struct EnvelopeParameters;

/**
 * This factory creates an envelope follower reader.
 */
class AUD_EnvelopeFactory : public AUD_EffectFactory
{
private:
	/**
	 * The attack value in seconds.
	 */
	const float m_attack;

	/**
	 * The release value in seconds.
	 */
	const float m_release;

	/**
	 * The threshold value.
	 */
	const float m_threshold;

	/**
	 * The attack/release threshold value.
	 */
	const float m_arthreshold;

	// hide copy constructor and operator=
	AUD_EnvelopeFactory(const AUD_EnvelopeFactory&);
	AUD_EnvelopeFactory& operator=(const AUD_EnvelopeFactory&);

public:
	/**
	 * Creates a new envelope factory.
	 * \param factory The input factory.
	 * \param attack The attack value in seconds.
	 * \param release The release value in seconds.
	 * \param threshold The threshold value.
	 * \param arthreshold The attack/release threshold value.
	 */
	AUD_EnvelopeFactory(boost::shared_ptr<AUD_IFactory> factory, float attack, float release,
						float threshold, float arthreshold);

	virtual boost::shared_ptr<AUD_IReader> createReader();

	static sample_t envelopeFilter(AUD_CallbackIIRFilterReader* reader, EnvelopeParameters* param);
	static void endEnvelopeFilter(EnvelopeParameters* param);
};

#endif //__AUD_ENVELOPEFACTORY_H__
