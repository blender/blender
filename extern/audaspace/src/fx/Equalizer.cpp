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

#include "fx/Equalizer.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "Exception.h"

#include "fx/ConvolverReader.h"
#include "fx/ImpulseResponse.h"
#include "util/Buffer.h"
#include "util/FFTPlan.h"
#include "util/ThreadPool.h"

AUD_NAMESPACE_BEGIN

Equalizer::Equalizer(std::shared_ptr<ISound> sound, std::shared_ptr<Buffer> bufEQ, int externalSizeEq, float maxFreqEq, int sizeConversion) : m_sound(sound), m_bufEQ(bufEQ)
{
	this->maxFreqEq = maxFreqEq;
	this->external_size_eq = externalSizeEq;

	filter_length = sizeConversion;
}

Equalizer::~Equalizer()
{
}

std::shared_ptr<IReader> Equalizer::createReader()
{
	std::shared_ptr<FFTPlan> fp = std::shared_ptr<FFTPlan>(new FFTPlan(filter_length));
	// 2 threads to start with
	return std::shared_ptr<ConvolverReader>(new ConvolverReader(m_sound->createReader(), createImpulseResponse(), std::shared_ptr<ThreadPool>(new ThreadPool(2)), fp));
}

float calculateValueArray(float* data, float minX, float maxX, int length, float posX)
{
	if(posX < minX)
		return 1.0;
	if(posX > maxX)
		return data[length - 1];
	float interval = (maxX - minX) / (float) length;
	int idx = (int) ((posX - minX) / interval);
	return data[idx];
}

void complex_prod(float a, float b, float c, float d, float* r, float* imag)
{
	float prod1 = a * c;
	float prod2 = b * d;
	float prod3 = (a + b) * (c + d);

	// Real Part
	*r = prod1 - prod2;

	// Imaginary Part
	*imag = prod3 - (prod1 + prod2);
}

/**
 * The creation of the ImpuseResponse which will be convoluted with the sound
 *
 * The implementation is based on scikit-signal
 */
std::shared_ptr<ImpulseResponse> Equalizer::createImpulseResponse()
{
	std::shared_ptr<FFTPlan> fp = std::shared_ptr<FFTPlan>(new FFTPlan(filter_length));
	fftwf_complex* buffer = (fftwf_complex*) fp->getBuffer();
	std::memset(buffer, 0, filter_length * sizeof(fftwf_complex));
	std::shared_ptr<IReader> soundReader = m_sound.get()->createReader();
	Specs specsSound = soundReader.get()->getSpecs();

	int sampleRate = specsSound.rate;

	for(unsigned i = 0; i < filter_length / 2; i++)
	{
		double freq = (((float) i) / (float) filter_length) * (float) sampleRate;

		double dbGain = calculateValueArray(m_bufEQ->getBuffer(), 0.0, maxFreqEq, external_size_eq, freq);

		// gain = 10^(decibels / 20.0)
		// 0 db = 1
		// 20 db = 10
		// 40 db = 100
		float gain = (float) pow(10.0, dbGain / 20.0);

		if(i == filter_length / 2 - 1)
		{
			gain = 0;
		}
		// IMPORTANT!!!! It is needed for the minimum phase step.
		// Without this, the amplitude would be square rooted
		//
		gain *= gain;

		// Calculation of exponential with std.. or "by hand"
		/*
		std::complex<float> preShift= std::complex<float>(0.0, -(filter_length - 1)
		/ 2. * M_PI * freq / ( sampleRate/2)); std::complex<float> shift =
		std::exp(preShift);

		std::complex<float> cGain = gain * shift;
		*/

		float imaginary_shift = -(filter_length - 1) / 2. * M_PI * freq / (sampleRate / 2);
		float cGain_real = gain * cos(imaginary_shift);
		float cGain_imag = gain * sin(imaginary_shift);

		int i2 = filter_length - i - 1;

		buffer[i][0] = cGain_real; // Real
		buffer[i][1] = cGain_imag; // Imag

		if(i > 0 && i2 < filter_length)
		{
			buffer[i2][0] = cGain_real; // Real
			buffer[i2][1] = cGain_imag; // Imag
		}
	}

	// In place. From Complex to sample_t
	fp->IFFT(buffer);

	// Window Hamming
	sample_t* pt_sample_t = (sample_t*) buffer;
	float half_filter = ((float) filter_length) / 2.0;
	for(int i = 0; i < filter_length; i++)
	{
		// Centered in filter_length/2
		float window = 0.54 - 0.46 * cos((2 * M_PI * (float) i) / (float) (filter_length - 1));
		pt_sample_t[i] *= window;
	}

	std::shared_ptr<Buffer> b2 = std::shared_ptr<Buffer>(new Buffer(filter_length * sizeof(sample_t)));

	sample_t* buffer_real = (sample_t*) buffer;
	sample_t* buffer2 = b2->getBuffer();
	float normaliziter = (float) filter_length;
	for(int i = 0; i < filter_length; i++)
	{
		buffer2[i] = (buffer_real[i] / normaliziter);
	}

	fp->freeBuffer(buffer);

	//
	// Here b2 is the buffer with a "valid" FIR (remember the squared amplitude
	//
	std::shared_ptr<Buffer> ir_minimum = minimumPhaseFilterHomomorphic(b2, filter_length, -1);

	Specs specsIR;
	specsIR.rate = sampleRate;
	specsIR.channels = CHANNELS_MONO;

	return std::shared_ptr<ImpulseResponse>(new ImpulseResponse(std::shared_ptr<StreamBuffer>(new StreamBuffer(ir_minimum, specsIR)), fp));
}

std::shared_ptr<Buffer> Equalizer::minimumPhaseFilterHomomorphic(std::shared_ptr<Buffer> original, int lOriginal, int lWork)
{
	void* b_orig = original->getBuffer();

	if(lWork < lOriginal || lWork < 0)
	{
		lWork = (int) pow(2, ceil(log2((float) (2 * (lOriginal - 1) / 0.01))));
	}

	std::shared_ptr<FFTPlan> fp = std::shared_ptr<FFTPlan>(new FFTPlan(lWork, 0.1));
	fftwf_complex* buffer = (fftwf_complex*) fp->getBuffer();
	sample_t* b_work = (sample_t*) buffer;
	// Padding with 0
	std::memset(b_work, 0, lWork * sizeof(sample_t));
	std::memcpy(b_work, b_orig, lOriginal * sizeof(sample_t));

	fp->FFT(b_work);

	for(int i = 0; i < lWork / 2; i++)
	{
		buffer[i][0] = fabs(sqrt(buffer[i][0] * buffer[i][0] + buffer[i][1] * buffer[i][1]));
		buffer[i][1] = 0.0;
		int conjugate = lWork - i - 1;
		buffer[conjugate][0] = buffer[i][0];
		buffer[conjugate][1] = 0.0;
	}

	double threshold = pow(10.0, -7);
	float logThreshold = (float) log(threshold);
	// take 0.25*log(|H|**2) = 0.5*log(|H|)
	for(int i = 0; i < lWork; i++)
	{
		if(buffer[i][0] < threshold)
		{
			buffer[i][0] = 0.5 * logThreshold;
		}
		else
		{
			buffer[i][0] = 0.5 * log(buffer[i][0]);
		}
	}

	fp->IFFT(buffer);

	// homomorphic filter
	int stop = (lOriginal + 1) / 2;
	b_work[0] = b_work[0] / (float) lWork;
	for(int i = 1; i < stop; i++)
	{
		b_work[i] = b_work[i] / (float) lWork * 2.0;
	}
	for(int i = stop; i < lWork; i++)
	{
		b_work[i] = 0;
	}

	fp->FFT(buffer);
	// EXP
	// e^x = e^ (a+bi)= e^a * e^bi = e^a * (cos b + i sin b)
	for(int i = 0; i < lWork / 2; i++)
	{
		float new_real;
		float new_imag;
		new_real = exp(buffer[i][0]) * cos(buffer[i][1]);
		new_imag = exp(buffer[i][0]) * sin(buffer[i][1]);

		buffer[i][0] = new_real;
		buffer[i][1] = new_imag;
		int conjugate = lWork - i - 1;
		buffer[conjugate][0] = new_real;
		buffer[conjugate][1] = new_imag;
	}

	// IFFT
	fp->IFFT(buffer);

	// Create new clean Buffer with only the result and normalization
	int lOut = (lOriginal / 2) + lOriginal % 2;
	std::shared_ptr<Buffer> bOut = std::shared_ptr<Buffer>(new Buffer(sizeof(float) * lOut));
	float* bbOut = (float*) bOut->getBuffer();

	// Copy and normalize
	for(int i = 0; i < lOut; i++)
	{
		bbOut[i] = b_work[i] / (float) lWork;
	}

	fp->freeBuffer(buffer);
	return bOut;
}

std::shared_ptr<Buffer> Equalizer::minimumPhaseFilterHilbert(std::shared_ptr<Buffer> original, int lOriginal, int lWork)
{
	void* b_orig = original->getBuffer();

	if(lWork < lOriginal || lWork < 0)
	{
		lWork = (int) pow(2, ceil(log2((float) (2 * (lOriginal - 1) / 0.01))));
	}

	std::shared_ptr<FFTPlan> fp = std::shared_ptr<FFTPlan>(new FFTPlan(lWork, 0.1));
	fftwf_complex* buffer = (fftwf_complex*) fp->getBuffer();
	sample_t* b_work = (sample_t*) buffer;
	// Padding with 0
	std::memset(b_work, 0, lWork * sizeof(sample_t));
	std::memcpy(b_work, b_orig, lOriginal * sizeof(sample_t));

	fp->FFT(b_work);
	float mymax, mymin;
	float n_half = (float) (lOriginal >> 1);
	for(int i = 0; i < lWork; i++)
	{
		float w = ((float) i) * 2.0 * M_PI / (float) lWork * n_half;
		float f1 = cos(w);
		float f2 = sin(w);
		float f3, f4;
		complex_prod(buffer[i][0], buffer[i][1], f1, f2, &f3, &f4);
		buffer[i][0] = f3;
		buffer[i][1] = 0.0;
		if(i == 0)
		{
			mymax = f3;
			mymin = f3;
		}
		else
		{
			if(f3 < mymin)
				mymin = f3;
			if(f3 > mymax)
				mymax = f3;
		}
	}
	float dp = mymax - 1;
	float ds = 0 - mymin;
	float S = 4.0 / pow(2, (sqrt(1 + dp + ds) + sqrt(1 - dp + ds)));
	for(int i = 0; i < lWork; i++)
	{
		buffer[i][0] = sqrt((buffer[i][0] + ds) * S) + 1.0E-10;
	}

	fftwf_complex* buffer_tmp = (fftwf_complex*) std::malloc(lWork * sizeof(fftwf_complex));
	std::memcpy(buffer_tmp, buffer, lWork * sizeof(fftwf_complex));

	//
	// Hilbert transform
	//
	int midpt = lWork >> 1;
	for(int i = 0; i < lWork; i++)
		buffer[i][0] = log(buffer[i][0]);
	fp->IFFT(buffer);
	b_work[0] = 0.0;
	for(int i = 1; i < midpt; i++)
	{
		b_work[i] /= (float) lWork;
	}
	b_work[midpt] = 0.0;
	for(int i = midpt + 1; i < lWork; i++)
	{
		b_work[i] /= (-1.0 * lWork);
	}

	fp->FFT(b_work);

	// Exp
	for(int i = 0; i < lWork; i++)
	{
		float base = exp(buffer[i][0]);
		buffer[i][0] = base * cos(buffer[i][1]);
		buffer[i][1] = base * sin(buffer[i][1]);
		complex_prod(buffer_tmp[i][0], buffer_tmp[i][1], buffer[i][0], buffer[i][1], &(buffer[i][0]), &(buffer[i][1]));
	}
	std::free(buffer_tmp);

	fp->IFFT(buffer);

	//
	// Copy and normalization
	//
	int n_out = n_half + lOriginal % 2;
	std::shared_ptr<Buffer> b_minimum = std::shared_ptr<Buffer>(new Buffer(n_out * sizeof(sample_t)));
	std::memcpy(b_minimum->getBuffer(), buffer, n_out * sizeof(sample_t));
	sample_t* b_final = (sample_t*) b_minimum->getBuffer();
	for(int i = 0; i < n_out; i++)
	{
		b_final[i] /= (float) lWork;
	}
	return b_minimum;
}

AUD_NAMESPACE_END
