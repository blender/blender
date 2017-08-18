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

#include "respec/JOSResampleReader.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define RATE_MAX 256
#define SHIFT_BITS 12
#define double_to_fp(x) (lrint(x * double(1 << SHIFT_BITS)))
#define int_to_fp(x) (x << SHIFT_BITS)
#define fp_to_int(x) (x >> SHIFT_BITS)
#define fp_to_double(x) (x * 1.0/(1 << SHIFT_BITS))
#define fp_rest(x) (x & ((1 << SHIFT_BITS) - 1))
#define fp_rest_to_double(x) fp_to_double(fp_rest(x))

AUD_NAMESPACE_BEGIN

JOSResampleReader::JOSResampleReader(std::shared_ptr<IReader> reader, SampleRate rate) :
	ResampleReader(reader, rate),
	m_channels(CHANNELS_INVALID),
	m_n(0),
	m_P(0),
	m_cache_valid(0),
	m_last_factor(0)
{
}

void JOSResampleReader::reset()
{
	m_cache_valid = 0;
	m_n = 0;
	m_P = 0;
	m_last_factor = 0;
}

void JOSResampleReader::updateBuffer(int size, double factor, int samplesize)
{
	unsigned int len;
	double num_samples = double(m_len) / double(m_L);
	// first calculate what length we need right now
	if(factor >= 1)
		len = std::ceil(num_samples);
	else
		len = (unsigned int)(std::ceil(num_samples / factor));

	// then check if afterwards the length is enough for the maximum rate
	if(len + size < num_samples * RATE_MAX)
		len = num_samples * RATE_MAX - size;

	if(m_n > len)
	{
		sample_t* buf = m_buffer.getBuffer();
		len = m_n - len;
		std::memmove(buf, buf + len * m_channels, (m_cache_valid - len) * samplesize);
		m_n -= len;
		m_cache_valid -= len;
	}

	m_buffer.assureSize((m_cache_valid + size) * samplesize, true);
}

#define RESAMPLE_METHOD(name, left, right) void JOSResampleReader::name(double target_factor, int length, sample_t* buffer)\
{\
	sample_t* buf = m_buffer.getBuffer();\
\
	unsigned int P, l;\
	int end, channel, i;\
	double eta, v, f_increment, factor;\
\
	m_sums.assureSize(m_channels * sizeof(double));\
	double* sums = reinterpret_cast<double*>(m_sums.getBuffer());\
	sample_t* data;\
	const float* coeff = m_coeff;\
\
	unsigned int P_increment;\
\
	for(unsigned int t = 0; t < length; t++)\
	{\
		factor = (m_last_factor * (length - t - 1) + target_factor * (t + 1)) / length;\
\
		std::memset(sums, 0, sizeof(double) * m_channels);\
\
		if(factor >= 1)\
		{\
			P = double_to_fp(m_P * m_L);\
\
			end = std::floor(m_len / double(m_L) - m_P) - 1;\
			if(m_n < end)\
				end = m_n;\
\
			data = buf + (m_n - end) * m_channels;\
			l = fp_to_int(P);\
			eta = fp_rest_to_double(P);\
			l += m_L * end;\
\
			for(i = 0; i <= end; i++)\
			{\
				v = coeff[l] + eta * (coeff[l+1] - coeff[l]);\
				l -= m_L;\
				left\
			}\
\
			P = int_to_fp(m_L) - P;\
\
			end = std::floor((m_len - 1) / double(m_L) + m_P) - 1;\
			if(m_cache_valid - m_n - 2 < end)\
				end = m_cache_valid - m_n - 2;\
\
			data = buf + (m_n + 2 + end) * m_channels - 1;\
			l = fp_to_int(P);\
			eta = fp_rest_to_double(P);\
			l += m_L * end;\
\
			for(i = 0; i <= end; i++)\
			{\
				v = coeff[l] + eta * (coeff[l+1] - coeff[l]);\
				l -= m_L;\
				right\
			}\
\
			for(channel = 0; channel < m_channels; channel++)\
			{\
				*buffer = sums[channel];\
				buffer++;\
			}\
		}\
		else\
		{\
			f_increment = factor * m_L;\
			P_increment = double_to_fp(f_increment);\
			P = double_to_fp(m_P * f_increment);\
\
			end = (int_to_fp(m_len) - P) / P_increment - 1;\
			if(m_n < end)\
				end = m_n;\
\
			P += P_increment * end;\
			data = buf + (m_n - end) * m_channels;\
			l = fp_to_int(P);\
\
			for(i = 0; i <= end; i++)\
			{\
				eta = fp_rest_to_double(P);\
				v = coeff[l] + eta * (coeff[l+1] - coeff[l]);\
				P -= P_increment;\
				l = fp_to_int(P);\
				left\
			}\
\
			P = 0 - P;\
\
			end = (int_to_fp(m_len) - P) / P_increment - 1;\
			if(m_cache_valid - m_n - 2 < end)\
				end = m_cache_valid - m_n - 2;\
\
			P += P_increment * end;\
			data = buf + (m_n + 2 + end) * m_channels - 1;\
			l = fp_to_int(P);\
\
			for(i = 0; i <= end; i++)\
			{\
				eta = fp_rest_to_double(P);\
				v = coeff[l] + eta * (coeff[l+1] - coeff[l]);\
				P -= P_increment;\
				l = fp_to_int(P);\
				right\
			}\
\
			for(channel = 0; channel < m_channels; channel++)\
			{\
				*buffer = factor * sums[channel];\
				buffer++;\
			}\
		}\
\
		m_P += std::fmod(1.0 / factor, 1.0);\
		m_n += std::floor(1.0 / factor);\
\
		while(m_P >= 1.0)\
		{\
			m_P -= 1.0;\
			m_n++;\
		}\
	}\
}

RESAMPLE_METHOD(resample, {
				channel = 0;
				do
				{
					sums[channel] += *data * v;
					channel++;
					data++;
				}
				while(channel < m_channels);
}, {
				channel = m_channels;
				do
				{
					channel--;
					sums[channel] += *data * v;
					data--;
				}
				while(channel);
})

RESAMPLE_METHOD(resample_mono, {
				*sums += *data * v;
				data++;
}, {
				*sums += *data * v;
				data--;
})

RESAMPLE_METHOD(resample_stereo, {
				sums[0] += data[0] * v;
				sums[1] += data[1] * v;
				data+=2;
}, {
				data-=2;
				sums[0] += data[1] * v;
				sums[1] += data[2] * v;
})

void JOSResampleReader::seek(int position)
{
	position = std::floor(position * double(m_reader->getSpecs().rate) / double(m_rate));
	m_reader->seek(position);
	reset();
}

int JOSResampleReader::getLength() const
{
	return std::floor(m_reader->getLength() * double(m_rate) / double(m_reader->getSpecs().rate));
}

int JOSResampleReader::getPosition() const
{
	return std::floor((m_reader->getPosition() + double(m_P)) * m_rate / m_reader->getSpecs().rate);
}

Specs JOSResampleReader::getSpecs() const
{
	Specs specs = m_reader->getSpecs();
	specs.rate = m_rate;
	return specs;
}

void JOSResampleReader::read(int& length, bool& eos, sample_t* buffer)
{
	if(length == 0)
		return;

	Specs specs = m_reader->getSpecs();

	int samplesize = AUD_SAMPLE_SIZE(specs);
	double target_factor = double(m_rate) / double(specs.rate);
	eos = false;
	int len;
	double num_samples = double(m_len) / double(m_L);

	// check for channels changed
	if(specs.channels != m_channels)
	{
		m_channels = specs.channels;
		reset();

		switch(m_channels)
		{
		case CHANNELS_MONO:
			m_resample = &JOSResampleReader::resample_mono;
			break;
		case CHANNELS_STEREO:
			m_resample = &JOSResampleReader::resample_stereo;
			break;
		default:
			m_resample = &JOSResampleReader::resample;
			break;
		}
	}

	if(m_last_factor == 0)
		m_last_factor = target_factor;

	if(target_factor == 1 && m_last_factor == 1 && (m_P == 0))
	{
		// can read directly!

		len = length - (m_cache_valid - m_n);

		updateBuffer(len, target_factor, samplesize);
		sample_t* buf = m_buffer.getBuffer();

		m_reader->read(len, eos, buf + m_cache_valid * m_channels);
		m_cache_valid += len;

		length = m_cache_valid - m_n;

		if(length > 0)
		{
			std::memcpy(buffer, buf + m_n * m_channels, length * samplesize);
			m_n += length;
		}

		return;
	}

	// use minimum for the following calculations
	double factor = std::min(target_factor, m_last_factor);

	if(factor >= 1)
		len = (int(m_n) - m_cache_valid) + int(std::ceil(length / factor)) + std::ceil(num_samples);
	else
		len = (int(m_n) - m_cache_valid) + int(std::ceil(length / factor) + std::ceil(num_samples / factor));

	if(len > 0)
	{
		int should = len;

		updateBuffer(len, factor, samplesize);

		m_reader->read(len, eos, m_buffer.getBuffer() + m_cache_valid * m_channels);
		m_cache_valid += len;

		if(len < should)
		{
			if(len == 0 && eos)
				length = 0;
			else
			{
				// use maximum for the following calculations
				factor = std::max(target_factor, m_last_factor);

				if(eos)
				{
					// end of stream, let's check how many more samples we can produce
					len = std::floor((m_cache_valid - m_n) * factor);
					if(len < length)
						length = len;
				}
				else
				{
					// not enough data available yet, so we recalculate how 				many samples we can calculate
					if(factor >= 1)
						len = std::floor((num_samples + m_cache_valid - m_n) * factor);
					else
						len = std::floor((num_samples * factor + m_cache_valid - m_n) * factor);
					if(len < length)
						length = len;
				}
			}
		}
	}

	(this->*m_resample)(target_factor, length, buffer);

	m_last_factor = target_factor;

	if(m_n > m_cache_valid)
	{
		m_n = m_cache_valid;
	}

	eos = eos && ((m_n == m_cache_valid) || (length == 0));
}

AUD_NAMESPACE_END
