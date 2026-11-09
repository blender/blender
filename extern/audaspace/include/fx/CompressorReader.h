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
 * @file CompressorReader.h
 * @ingroup fx
 * The CompressorReader class.
 */

#include <memory>
#include <vector>

#include "fx/EffectReader.h"
#include "util/Buffer.h"

AUD_NAMESPACE_BEGIN

/**
 * This class reads another reader and applies dynamic range compression.
 */
class AUD_API CompressorReader : public EffectReader
{
private:
	static constexpr float MINIMUM_AMPLITUDE = 1e-6f; /* Minimum amplitude floor to prevent log10(0) */

	std::shared_ptr<IReader> m_sidechain_reader; /* The sidechain reader whose amplitude determines the amount of compression (usually the output of a peak or RMS detector) */
	float m_makeupGain;                          /* Output gain ratio to apply after compression (1.0 for no gain) */

	float m_thresholdDb;      /* Threshold in decibels */
	float m_compressionSlope; /* Compression slope in dB: 1.0 - 1.0 / ratio */
	float m_alphaAttack;      /* Attack smoothing factor: exp(-1 / (rate * attack)) */
	float m_alphaRelease;     /* Release smoothing factor: exp(-1 / (rate * release)) */
	float m_kneeWidthHalfDb;  /* Half knee width in decibels for soft knee compression */
	Buffer m_buffer;

	std::vector<float> m_gainReductionDb; /* Smoothed gain reduction state in decibels for each channel */

	static inline float linearToDb(float linear);
	static inline float dbToLinear(float db);

public:
	/**
	 * Creates a new compressor reader.
	 * \param reader The reader to read from and compress.
	 * \param sidechain_reader The sidechain reader whose amplitude determines the amount of compression (usually the output of a peak or RMS detector).
	 * \param threshold The threshold ratio (0.0 to 1.0) over which to start compression.
	 * \param ratio The compression ratio (e.g., 4.0 for 4:1).
	 * \param attack The attack time in seconds.
	 * \param release The release time in seconds.
	 * \param makeupGain The output gain ratio to apply after compression (1.0 for no gain).
	 * \param knee The knee ratio (1.0 for hard knee, higher for soft knee).
	 */
	CompressorReader(std::shared_ptr<IReader> reader, std::shared_ptr<IReader> sidechain_reader, float threshold, float ratio, float attack, float release, float makeupGain,
	                 float knee);

	virtual void read(int& length, bool& eos, sample_t* buffer) override;
	virtual void seek(int position) override;
	virtual bool isSeekable() const override;
};

AUD_NAMESPACE_END
