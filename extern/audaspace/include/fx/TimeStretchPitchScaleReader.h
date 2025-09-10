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
 * @file TimeStretchPitchScaleReader.h
 * @ingroup fx
 * The TimeStretchPitchScaleReader class.
 */

#include "TimeStretchPitchScale.h"

#include "fx/EffectReader.h"
#include "rubberband/RubberBandStretcher.h"
#include "util/Buffer.h"

using namespace RubberBand;

AUD_NAMESPACE_BEGIN

/**
 * This class reads from another reader and applies time-stretching and pitch scaling.
 */
class AUD_API TimeStretchPitchScaleReader : public EffectReader
{
private:
	/**
	 * The input buffer for the reader.
	 */
	Buffer m_buffer;

	/**
	 * The input/output deinterleaved buffers for each channel.
	 */
	std::vector<Buffer> m_deinterleaved;

	/**
	 * The pointers to the input/output deinterleaved buffer data for processing/retrieving.
	 */
	std::vector<sample_t*> m_channelData;

	/**
	 * Number of samples that need to be dropped at the beginning or after a seek.
	 */
	int m_samplesToDrop;

	// delete copy constructor and operator=
	TimeStretchPitchScaleReader(const TimeStretchPitchScaleReader&) = delete;
	TimeStretchPitchScaleReader& operator=(const TimeStretchPitchScaleReader&) = delete;

protected:
	/**
	 * Feeds the number of required zero samples to the stretcher and queries the amount of samples to drop.
	 */
	void reset();

	/**
	 * Rubberband stretcher.
	 */
	std::unique_ptr<RubberBandStretcher> m_stretcher;

	/**
	 * The current position.
	 */
	int m_position;

	/**
	 * Whether the reader has reached the end of stream.
	 */
	bool m_finishedReader;

public:
	/**
	 * Creates a new stretcher reader.
	 * \param reader The reader to read from.
	 * \param timeRatio The factor by which to stretch or compress time.
	 * \param pitchScale The factor by which to adjust the pitch.
	 * \param quality The processing quality level of the stretcher.
	 * \param preserveFormant Whether to preserve the vocal formants for the stretcher.
	 */
	TimeStretchPitchScaleReader(std::shared_ptr<IReader> reader, double timeRatio, double pitchScale, StretcherQuality quality, bool preserveFormant);

	virtual void read(int& length, bool& eos, sample_t* buffer);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;

	/**
	 * Retrieves the current time ratio for the stretcher.
	 * \return The current time ratio value.
	 */
	double getTimeRatio() const;

	/**
	 * Sets the time ratio for the stretcher.
	 */
	void setTimeRatio(double timeRatio);

	/**
	 * Retrieves the pitch scale for the stretcher.
	 * \return The current pitch scale value.
	 */
	double getPitchScale() const;

	/**
	 * Sets the pitch scale for the stretcher.
	 */
	void setPitchScale(double pitchScale);
};

AUD_NAMESPACE_END
