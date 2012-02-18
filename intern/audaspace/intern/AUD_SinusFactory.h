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

/** \file audaspace/intern/AUD_SinusFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SINUSFACTORY_H__
#define __AUD_SINUSFACTORY_H__

#include "AUD_IFactory.h"

/**
 * This factory creates a reader that plays a sine tone.
 */
class AUD_SinusFactory : public AUD_IFactory
{
private:
	/**
	 * The frequence of the sine wave.
	 */
	const float m_frequency;

	/**
	 * The target sample rate for output.
	 */
	const AUD_SampleRate m_sampleRate;

	// hide copy constructor and operator=
	AUD_SinusFactory(const AUD_SinusFactory&);
	AUD_SinusFactory& operator=(const AUD_SinusFactory&);

public:
	/**
	 * Creates a new sine factory.
	 * \param frequency The desired frequency.
	 * \param sampleRate The target sample rate for playback.
	 */
	AUD_SinusFactory(float frequency,
					 AUD_SampleRate sampleRate = AUD_RATE_44100);

	/**
	 * Returns the frequency of the sine wave.
	 */
	float getFrequency() const;

	virtual AUD_Reference<AUD_IReader> createReader();
};

#endif //__AUD_SINUSFACTORY_H__
