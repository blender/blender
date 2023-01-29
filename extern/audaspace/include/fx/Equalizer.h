/*******************************************************************************
 * Copyright 2022 Marcos Perez Gonzalez
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

/**
 * @file Equalizer.h
 * @ingroup fx
 * The Equalizer class.
 */

#include <memory>
#include <vector>

#include "ISound.h"
#include "ImpulseResponse.h"

AUD_NAMESPACE_BEGIN

class Buffer;
class ImpulseResponse;
/**
 * This class represents a sound that can be modified depending on a given impulse response.
 */
class AUD_API Equalizer : public ISound
{
private:
	/**
	 * A pointer to the imput sound.
	 */
	std::shared_ptr<ISound> m_sound;

	/**
	 * Local definition of Equalizer
	 */
	std::shared_ptr<Buffer> m_bufEQ;

	/**
	 * A pointer to the impulse response.
	 */
	std::shared_ptr<ImpulseResponse> m_impulseResponse;

	/**
	 * delete copy constructor and operator=
	 */
	Equalizer(const Equalizer&) = delete;
	Equalizer& operator=(const Equalizer&) = delete;

	/**
	 * Create ImpulseResponse from the definition in the Buffer,
	 * using at the end a minimum phase change
	 */
	std::shared_ptr<ImpulseResponse> createImpulseResponse();

	/**
	 * Create an Impulse Response with minimum phase distortion using Homomorphic
	 * The input is an Impulse Response
	 */
	std::shared_ptr<Buffer> minimumPhaseFilterHomomorphic(std::shared_ptr<Buffer> original, int lOriginal, int lWork);

	/**
	 * Create an Impulse Response with minimum phase distortion using Hilbert
	 * The input is an Impulse Response
	 */
	std::shared_ptr<Buffer> minimumPhaseFilterHilbert(std::shared_ptr<Buffer> original, int lOriginal, int lWork);

public:
	/**
	 * Creates a new Equalizer.
	 * \param sound The sound that will be equalized
	 */
	Equalizer(std::shared_ptr<ISound> sound, std::shared_ptr<Buffer> bufEQ, int externalSizeEq, float maxFreqEq, int sizeConversion);

	virtual ~Equalizer();
	virtual std::shared_ptr<IReader> createReader();

	/*
	 * Length of the external equalizer definition. It must be the number of "float" positions of the Buffer
	 */
	int external_size_eq;

	/*
	 * Length of the internal equalizer definition
	 */
	int filter_length;

	/*
	 * Maximum frequency used in the equalizer definition
	 */
	float maxFreqEq;
};

AUD_NAMESPACE_END
