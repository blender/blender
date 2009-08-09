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

#ifndef AUD_SDLMIXER
#define AUD_SDLMIXER

#include "AUD_IMixer.h"
class AUD_SDLMixerFactory;
#include <list>

struct AUD_SDLMixerBuffer
{
	sample_t* buffer;
	int length;
	float volume;
};

/**
 * This class is able to mix audiosignals with the help of SDL.
 */
class AUD_SDLMixer : public AUD_IMixer
{
private:
	/**
	 * The mixer factory that prepares all readers for superposition.
	 */
	AUD_SDLMixerFactory* m_factory;

	/**
	 * The list of buffers to superpose.
	 */
	std::list<AUD_SDLMixerBuffer> m_buffers;

	/**
	 * The size of an output sample.
	 */
	int m_samplesize;

public:
	/**
	 * Creates the mixer.
	 */
	AUD_SDLMixer();

	virtual ~AUD_SDLMixer();

	virtual AUD_IReader* prepare(AUD_IReader* reader);
	virtual void setSpecs(AUD_Specs specs);
	virtual void add(sample_t* buffer, AUD_Specs specs, int length,
					 float volume);
	virtual void superpose(sample_t* buffer, int length, float volume);
};

#endif //AUD_SDLMIXER
