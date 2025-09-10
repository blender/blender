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

#include "fx/AnimateableTimeStretchPitchScaleReader.h"

#include "IReader.h"

AUD_NAMESPACE_BEGIN

AnimateableTimeStretchPitchScaleReader::AnimateableTimeStretchPitchScaleReader(std::shared_ptr<IReader> reader, float fps, std::shared_ptr<AnimateableProperty> timeStretch,
                                                                               std::shared_ptr<AnimateableProperty> pitchScale, StretcherQuality quality, bool preserveFormant) :
    TimeStretchPitchScaleReader(reader, timeStretch->readSingle(0), pitchScale->readSingle(0), quality, preserveFormant),
    m_fps(fps),
    m_timeStretch(timeStretch),
    m_pitchScale(pitchScale)
{
}

void AnimateableTimeStretchPitchScaleReader::read(int& length, bool& eos, sample_t* buffer)
{
	int position = getPosition();

	double time = double(position) / double(m_reader->getSpecs().rate);
	float frame = time * m_fps;

	float timeRatio = m_timeStretch->readSingle(frame);
	setTimeRatio(timeRatio);
	float pitchScale = m_pitchScale->readSingle(frame);
	setPitchScale(pitchScale);
	TimeStretchPitchScaleReader::read(length, eos, buffer);
}

void AnimateableTimeStretchPitchScaleReader::seek(int position)
{
	const double sampleRate = double(m_reader->getSpecs().rate);
	const double samplesPerFrame = sampleRate / m_fps;
	const double frame = double(position) / samplesPerFrame;

	float timeRatio = m_timeStretch->readSingle(frame);
	setTimeRatio(timeRatio);
	float pitchScale = m_pitchScale->readSingle(frame);
	setPitchScale(pitchScale);

	const int totalFrames = static_cast<int>(frame);

	float ratio = 1.0f;

	double inputSamplePos = 0.0;

	const sample_t* animationSamples = m_timeStretch->getBuffer().getBuffer();

	const int bufferFrames = m_timeStretch->getBuffer().getSize() / (sizeof(sample_t) * m_timeStretch->getCount());

	for(int frameIndex = 0; frameIndex < std::min(bufferFrames, totalFrames); frameIndex++)
	{
		ratio = std::max(animationSamples[frameIndex], 1.0f / 256.0f);
		inputSamplePos += samplesPerFrame / ratio;
	}

	if(totalFrames > bufferFrames)
	{
		// The position is past the end of animation buffer and so use the last read ratio
		// This already includes the fractional frame
		inputSamplePos += (samplesPerFrame * (frame - bufferFrames)) / ratio;
	}
	else
	{
		// The position is before the end of the animation buffer and so read one last time for the remaining fractional frame
		double remainderFrame = frame - totalFrames;

		float remainderRatio = std::max(m_timeStretch->readSingle(frame), 1.0f / 256.0f);

		inputSamplePos += (samplesPerFrame * remainderFrame) / remainderRatio;
	}

	m_reader->seek(static_cast<int>(inputSamplePos));
	m_finishedReader = false;
	m_stretcher->reset();
	reset();
	m_position = position;
}

AUD_NAMESPACE_END
