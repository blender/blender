/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_JOSResampleReader.cpp
 *  \ingroup audaspaceintern
 */

#include "AUD_JOSResampleReader.h"

#include "AUD_JOSResampleReaderCoeff.cpp"

#include <cmath>
#include <cstring>
#include <iostream>

/* MSVC does not have lrint */
#ifdef _MSC_VER
#ifdef _M_X64
#include <emmintrin.h>
static inline int lrint(double d)
{
		return _mm_cvtsd_si32(_mm_load_sd(&d));
}
#else
static inline int lrint(double d)
{
	int i;

	_asm{
		fld d
		fistp i
	};

	return i;
}
#endif
#endif

#define CC m_channels + channel

#define AUD_RATE_MAX 256
#define SHIFT_BITS 12
#define double_to_fp(x) (lrint(x * double(1 << SHIFT_BITS)))
#define int_to_fp(x) (x << SHIFT_BITS)
#define fp_to_int(x) (x >> SHIFT_BITS)
#define fp_to_double(x) (x * 1.0/(1 << SHIFT_BITS))
#define fp_rest(x) (x & ((1 << SHIFT_BITS) - 1))
#define fp_rest_to_double(x) fp_to_double(fp_rest(x))

AUD_JOSResampleReader::AUD_JOSResampleReader(AUD_Reference<AUD_IReader> reader, AUD_Specs specs) :
	AUD_ResampleReader(reader, specs.rate),
	m_channels(AUD_CHANNELS_INVALID),
	m_n(0),
	m_P(0),
	m_cache_valid(0),
	m_last_factor(0)
{
}

void AUD_JOSResampleReader::reset()
{
	m_cache_valid = 0;
	m_n = 0;
	m_P = 0;
	m_last_factor = 0;
}

void AUD_JOSResampleReader::updateBuffer(int size, double factor, int samplesize)
{
	unsigned int len;
	double num_samples = double(m_len) / double(m_L);
	// first calculate what length we need right now
	if(factor >= 1)
		len = ceil(num_samples);
	else
		len = (unsigned int)(ceil(num_samples / factor));

	// then check if afterwards the length is enough for the maximum rate
	if(len + size < num_samples * AUD_RATE_MAX)
		len = num_samples * AUD_RATE_MAX - size;

	if(m_n > len)
	{
		sample_t* buf = m_buffer.getBuffer();
		len = m_n - len;
		memmove(buf, buf + len * m_channels, (m_cache_valid - len) * samplesize);
		m_n -= len;
		m_cache_valid -= len;
	}

	m_buffer.assureSize((m_cache_valid + size) * samplesize, true);
}

#define RESAMPLE_METHOD(name, left, right) void AUD_JOSResampleReader::name(double target_factor, int length, sample_t* buffer)\
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
		memset(sums, 0, sizeof(double) * m_channels);\
\
		if(factor >= 1)\
		{\
			P = double_to_fp(m_P * m_L);\
\
			end = floor(m_len / double(m_L) - m_P) - 1;\
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
			end = floor((m_len - 1) / double(m_L) + m_P) - 1;\
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
		m_P += fmod(1.0 / factor, 1.0);\
		m_n += floor(1.0 / factor);\
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

void AUD_JOSResampleReader::seek(int position)
{
	position = floor(position * double(m_reader->getSpecs().rate) / double(m_rate));
	m_reader->seek(position);
	reset();
}

int AUD_JOSResampleReader::getLength() const
{
	return floor(m_reader->getLength() * double(m_rate) / double(m_reader->getSpecs().rate));
}

int AUD_JOSResampleReader::getPosition() const
{
	return floor((m_reader->getPosition() + double(m_P))
				 * m_rate / m_reader->getSpecs().rate);
}

AUD_Specs AUD_JOSResampleReader::getSpecs() const
{
	AUD_Specs specs = m_reader->getSpecs();
	specs.rate = m_rate;
	return specs;
}

void AUD_JOSResampleReader::read(int& length, bool& eos, sample_t* buffer)
{
	if(length == 0)
		return;

	AUD_Specs specs = m_reader->getSpecs();

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
		case AUD_CHANNELS_MONO:
			m_resample = &AUD_JOSResampleReader::resample_mono;
			break;
		case AUD_CHANNELS_STEREO:
			m_resample = &AUD_JOSResampleReader::resample_stereo;
			break;
		default:
			m_resample = &AUD_JOSResampleReader::resample;
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
			memcpy(buffer, buf + m_n * m_channels, length * samplesize);
			m_n += length;
		}

		return;
	}

	// use minimum for the following calculations
	double factor = AUD_MIN(target_factor, m_last_factor);

	if(factor >= 1)
		len = (int(m_n) - m_cache_valid) + int(ceil(length / factor)) + ceil(num_samples);
	else
		len = (int(m_n) - m_cache_valid) + int(ceil(length / factor) + ceil(num_samples / factor));

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
				factor = AUD_MAX(target_factor, m_last_factor);

				if(eos)
				{
					// end of stream, let's check how many more samples we can produce
					len = floor((m_cache_valid - m_n) * factor);
					if(len < length)
						length = len;
				}
				else
				{
					// not enough data available yet, so we recalculate how many samples we can calculate
					if(factor >= 1)
						len = floor((num_samples + m_cache_valid - m_n) * factor);
					else
						len = floor((num_samples * factor + m_cache_valid - m_n) * factor);
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
