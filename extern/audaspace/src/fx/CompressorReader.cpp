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

#include "fx/CompressorReader.h"

#include <cmath>
#include <cstring>

#include "Exception.h"

AUD_NAMESPACE_BEGIN

inline float CompressorReader::linearToDb(float linear)
{
	if(linear <= MINIMUM_AMPLITUDE)
	{
		return 20.0f * std::log10(MINIMUM_AMPLITUDE);
	}
	return 20.0f * std::log10(linear);
}

inline float CompressorReader::dbToLinear(float db)
{
	if(db <= 20.0f * std::log10(MINIMUM_AMPLITUDE))
	{
		return 0.0f;
	}
	return std::pow(10.0f, db / 20.0f);
}

CompressorReader::CompressorReader(std::shared_ptr<IReader> reader, std::shared_ptr<IReader> sidechain_reader, float threshold, float ratio, float attack, float release,
                                   float makeupGain, float knee) :
    EffectReader(reader), m_sidechain_reader(sidechain_reader), m_makeupGain(makeupGain)
{
	// Precalculate compression parameters and smoothing factors
	m_thresholdDb = linearToDb(threshold);
	m_compressionSlope = (ratio > 1.0f) ? (1.0f - 1.0f / ratio) : 0.0f;

	Specs specs = m_reader->getSpecs();
	SampleRate sampleRate = specs.rate;

	m_alphaAttack = (sampleRate > 0 && attack > 0) ? std::exp(-1.0f / (sampleRate * attack)) : 0.0f;
	m_alphaRelease = (sampleRate > 0 && release > 0) ? std::exp(-1.0f / (sampleRate * release)) : 0.0f;
	m_gainReductionDb.resize(m_sidechain_reader->getSpecs().channels, 0.0f);

	m_kneeWidthHalfDb = (knee > 1.0f) ? 10.0f * std::log10(knee) : 0.0f;
}

void CompressorReader::read(int& length, bool& eos, sample_t* buffer)
{
	Specs specs = m_reader->getSpecs();
	Specs sidechainSpecs = m_sidechain_reader->getSpecs();
	if(specs.rate != sidechainSpecs.rate)
	{
		AUD_THROW(StateException, "The sound and the sidechain must have the same rate.");
	}
	if(specs.channels != sidechainSpecs.channels && sidechainSpecs.channels != CHANNELS_MONO)
	{
		AUD_THROW(StateException, "The sidechain and the sound must either have the same amount of channels or the sidechain must be mono.");
	}
	m_reader->read(length, eos, buffer);

	if(length == 0)
	{
		return;
	}

	int sidechainSamplesize = AUD_SAMPLE_SIZE(sidechainSpecs);
	m_buffer.assureSize(length * sidechainSamplesize);
	sample_t* buf = m_buffer.getBuffer();

	int len2 = length;
	bool eos2 = false;
	m_sidechain_reader->read(len2, eos2, buf);

	if(len2 < length)
	{
		std::memset(buf + len2 * sidechainSpecs.channels, 0, (length - len2) * sidechainSamplesize);
	}

	const Channels channels = specs.channels;
	const Channels sidechainChannels = sidechainSpecs.channels;

	if(sidechainChannels == CHANNELS_MONO)
	{
		for(int frame = 0; frame < length; frame++)
		{
			const float levelDb = linearToDb(buf[frame]);

			// Calculate target gain reduction in decibels
			const float overshootDb = levelDb - m_thresholdDb;
			float targetReductionDb = 0.0f;

			// Smooth gain reduction within the knee region
			if(m_kneeWidthHalfDb > 0.0f && std::fabs(overshootDb) <= m_kneeWidthHalfDb)
			{
				const float x = overshootDb + m_kneeWidthHalfDb;
				targetReductionDb = (x * x / (4.0f * m_kneeWidthHalfDb)) * m_compressionSlope;
			}
			// Standard gain reduction above the knee region
			else if(overshootDb > m_kneeWidthHalfDb)
			{
				targetReductionDb = overshootDb * m_compressionSlope;
			}

			// Apply attack or release smoothing
			if(targetReductionDb >= m_gainReductionDb[0])
			{
				m_gainReductionDb[0] = m_alphaAttack * m_gainReductionDb[0] + (1.0f - m_alphaAttack) * targetReductionDb;
			}
			else
			{
				m_gainReductionDb[0] = m_alphaRelease * m_gainReductionDb[0] + (1.0f - m_alphaRelease) * targetReductionDb;
			}

			// Calculate final gain and apply to all channels.
			const float gain = dbToLinear(-m_gainReductionDb[0]) * m_makeupGain;
			for(int ch = 0; ch < channels; ch++)
			{
				buffer[frame * channels + ch] *= gain;
			}
		}
	}
	else
	{
		for(int frame = 0; frame < length; frame++)
		{
			for(int ch = 0; ch < channels; ch++)
			{
				const float levelDb = linearToDb(buf[frame * sidechainChannels + ch]);

				// Calculate target gain reduction in decibels
				const float overshootDb = levelDb - m_thresholdDb;
				float targetReductionDb = 0.0f;

				// Smooth gain reduction within the knee region
				if(m_kneeWidthHalfDb > 0.0f && std::fabs(overshootDb) <= m_kneeWidthHalfDb)
				{
					const float x = overshootDb + m_kneeWidthHalfDb;
					targetReductionDb = (x * x / (4.0f * m_kneeWidthHalfDb)) * m_compressionSlope;
				}
				// Standard gain reduction above the knee region
				else if(overshootDb > m_kneeWidthHalfDb)
				{
					targetReductionDb = overshootDb * m_compressionSlope;
				}

				// Apply attack or release smoothing
				if(targetReductionDb >= m_gainReductionDb[ch])
				{
					m_gainReductionDb[ch] = m_alphaAttack * m_gainReductionDb[ch] + (1.0f - m_alphaAttack) * targetReductionDb;
				}
				else
				{
					m_gainReductionDb[ch] = m_alphaRelease * m_gainReductionDb[ch] + (1.0f - m_alphaRelease) * targetReductionDb;
				}

				// Calculate final gain and apply to all channels.
				const float gain = dbToLinear(-m_gainReductionDb[ch]) * m_makeupGain;
				buffer[frame * channels + ch] *= gain;
			}
		}
	}
}

void CompressorReader::seek(int position)
{
	m_reader->seek(position);
	m_sidechain_reader->seek(position);

	std::fill(m_gainReductionDb.begin(), m_gainReductionDb.end(), 0.0f);
}

bool CompressorReader::isSeekable() const
{
	return m_reader->isSeekable() && m_sidechain_reader->isSeekable();
}

AUD_NAMESPACE_END
