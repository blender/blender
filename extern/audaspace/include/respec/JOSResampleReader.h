/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
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
 * @file JOSResampleReader.h
 * @ingroup respec
 * The JOSResampleReader class.
 */

#include "respec/ResampleReader.h"
#include "util/Buffer.h"

AUD_NAMESPACE_BEGIN

/**
 * This resampling reader uses Julius O. Smith's resampling algorithm.
 */
class AUD_API JOSResampleReader : public ResampleReader
{
private:
	typedef void (JOSResampleReader::*resample_f)(double target_factor, int length, sample_t* buffer);

	/**
	 * The half filter length for Quality::HIGH setting. 
	 */
	static const int m_len_high;
	/**
	 * The half filter length for Quality::MEDIUM setting.
	 */
	static const int m_len_medium;
	/**
	 * The half filter length for Quality::LOW setting.
	 */
	static const int m_len_low;
	/**
	 * The filter sample step size for Quality::HIGH setting.
	 */
	static const int m_L_high;
	/**
	 * The filter sample step size for Quality::MEDIUM setting.
	 */
	static const int m_L_medium;
	/**
	 * The filter sample step size for Quality::LOW setting.
	 */
	static const int m_L_low;
	/**
	 * The filter coefficients for Quality::HIGH setting.
	 */
	static const float m_coeff_high[];
	/**
	 * The filter coefficients for Quality::MEDIUM setting.
	 */
	static const float m_coeff_medium[];
	/**
	 * The filter coefficients for Quality::LOW setting.
	 */
	static const float m_coeff_low[];

	/**
	 * The half filter length.
	 */
	int m_len;

	/**
	 * The sample step size for the filter.
	 */
	int m_L;

	/**
	 * The filter coefficients.
	 */
	const float* m_coeff;

	/**
	 * The reader channels.
	 */
	Channels m_channels;

	/**
	 * The sample position in the cache.
	 */
	unsigned int m_n;

	/**
	 * The subsample position in the cache.
	 */
	double m_P;

	/**
	 * The input data buffer.
	 */
	Buffer m_buffer;

	/**
	 * Double buffer for the sums.
	 */
	Buffer m_sums;

	/**
	 * How many samples in the cache are valid.
	 */
	int m_cache_valid;

	/**
	 * Resample function.
	 */
	resample_f m_resample;

	/**
	 * Last resampling factor.
	 */
	double m_last_factor;

	// delete copy constructor and operator=
	JOSResampleReader(const JOSResampleReader&) = delete;
	JOSResampleReader& operator=(const JOSResampleReader&) = delete;

	/**
	 * Resets the resampler to its initial state.
	 */
	void AUD_LOCAL reset();

	/**
	 * Updates the buffer to be as small as possible for the coming reading.
	 * \param size The size of samples to be read.
	 * \param factor The next resampling factor.
	 * \param samplesize The size of a sample.
	 */
	void AUD_LOCAL updateBuffer(int size, double factor, int samplesize);

	void AUD_LOCAL resample_generic(double target_factor, int length, sample_t* buffer);
	void AUD_LOCAL resample_mono(double target_factor, int length, sample_t* buffer);
	void AUD_LOCAL resample_stereo(double target_factor, int length, sample_t* buffer);

	template <typename T>
	void AUD_LOCAL resample(double target_factor, int length, sample_t* buffer);

public:
	enum class Quality
	{
		LOW = 0,
		MEDIUM,
		HIGH,
	};

	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param rate The target sampling rate.
	 */
	JOSResampleReader(std::shared_ptr<IReader> reader, SampleRate rate, Quality = Quality::MEDIUM);

	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
