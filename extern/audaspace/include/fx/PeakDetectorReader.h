/*******************************************************************************
 * Copyright 2026 Mayank
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
 * @file PeakDetectorReader.h
 * @ingroup fx
 * The PeakDetectorReader class.
 */

#include <memory>

#include "fx/EffectReader.h"
#include "util/Buffer.h"

AUD_NAMESPACE_BEGIN
/**
 * This reader reads another reader and calculates the peak amplitude across all channels for each frame of sound, outputting a mono sound.
 */
class AUD_API PeakDetectorReader : public EffectReader
{
private:
	Buffer m_buffer;

public:
	/**
	 * Creates a new peak detector reader.
	 * \param reader The reader to read sound from.
	 */
	PeakDetectorReader(std::shared_ptr<IReader> reader);

	virtual void read(int& length, bool& eos, sample_t* buffer) override;
	virtual Specs getSpecs() const override;
};

AUD_NAMESPACE_END
