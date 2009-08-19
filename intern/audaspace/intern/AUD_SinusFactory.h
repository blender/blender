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

#ifndef AUD_SINUSFACTORY
#define AUD_SINUSFACTORY

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
	double m_frequency;

	/**
	 * The target sample rate for output.
	 */
	AUD_SampleRate m_sampleRate;

public:
	/**
	 * Creates a new sine factory.
	 * \param frequency The desired frequency.
	 * \param sampleRate The target sample rate for playback.
	 */
	AUD_SinusFactory(double frequency,
					 AUD_SampleRate sampleRate = AUD_RATE_44100);

	/**
	 * Returns the frequency of the sine wave.
	 */
	double getFrequency();

	/**
	 * Sets the frequency.
	 * \param frequency The new frequency.
	 */
	void setFrequency(double frequency);

	virtual AUD_IReader* createReader();
};

#endif //AUD_SINUSFACTORY
