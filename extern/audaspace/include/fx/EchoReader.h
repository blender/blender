/*******************************************************************************
 * Copyright 2009-2025 Jörg Müller
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
 * @file EchoReader.h
 * @ingroup fx
 * The EchoReader class.
 */

#include <memory>

#include "fx/EffectReader.h"
#include "util/Buffer.h"

AUD_NAMESPACE_BEGIN

class AUD_API EchoReader : public EffectReader
{
private:
	float m_delay;
	float m_feedback;
	float m_mix;
	bool m_resetBuffer;

	Buffer m_inBuffer;
	Buffer m_delayBuffer;

	int m_writePosition{0};
	int m_samplesAvailable{0};

public:
	EchoReader(std::shared_ptr<IReader> reader, float delay, float feedback, float mix, bool resetBuffer = true);

	virtual void read(int& length, bool& eos, sample_t* buffer) override;
	virtual void seek(int position) override;
};

AUD_NAMESPACE_END
