/*******************************************************************************
* Copyright 2015-2016 Juan Francisco Crespo Galán
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
* @file FFTPlan.h
* @ingroup util
* The FFTPlan class.
*/

#include <complex>
#include <fftw3.h>
#include "Audaspace.h"

#include <memory>
#include <vector>

/**Default FFT size.*/
#define DEFAULT_N 4096

AUD_NAMESPACE_BEGIN

/**
* Thas class represents an plan object that allows to calculate FFTs and IFFTs.
*/
class AUD_API FFTPlan
{
private:
	/**
	* The size of the FFT plan.
	*/
	int m_N;

	/**
	* The plan to transform the input to the frequency domain.
	*/
	fftwf_plan m_fftPlanR2C;

	/**
	* The plan to transform the input to the time domain again.
	*/
	fftwf_plan m_fftPlanC2R;

	/**
	* The size of a buffer for its use with the FFT plan (in bytes).
	*/
	unsigned int m_bufferSize;

	// delete copy constructor and operator=
	FFTPlan(const FFTPlan&) = delete;
	FFTPlan& operator=(const FFTPlan&) = delete;

public:
	/**
	* Creates a new FFTPlan object with DEFAULT_N size (4096).
	* \param measureTime The aproximate amount of seconds that FFTW will spend searching for the optimal plan,
	*		which means faster FFTs and IFFTs while using this plan. If measureTime is negative, it will take all the time it needs.
	*/
	FFTPlan(double measureTime = 0);

	/**
	* Creates a new FFTPlan object with a custom size.
	* \param n The size of the FFT plan. Values that are a power of two are faster. 
	*		The useful range usually is between 2048 and 8192, but bigger values can be useful
	*		in certain situations (when using the StreamBuffer class per example). 
	*		Generally, low values use more CPU power and are a bit faster than large ones, 
	*		there is also a huge decrease in efficiency when n is lower than 2048.
	* \param measureTime The aproximate amount of seconds that FFTW will spend searching for the optimal plan,
	*		which means faster FFTs while using this plan. If measureTime is negative, it will take all the time it needs.
	*/
	FFTPlan(int n, double measureTime = 0);
	~FFTPlan();

	/**
	* Retrieves the size of the FFT plan.
	* \return The size of the plan.
	*/
	int getSize();

	/**
	* Calculates the FFT of an input buffer with the current plan.
	* \param[in,out] buffer A buffer with the input data an in which the output data will be written.
	*/
	void FFT(void* buffer);

	/**
	* Calculates the IFFT of an input buffer with the current plan.
	* \param[in,out] buffer A buffer with the input data an in which the output data will be written.
	*/
	void IFFT(void* buffer);

	/**
	* Reserves memory for a buffer that can be used for inplace transformations with this plan.
	* \return A pointer to a buffer of size ((N/2)+1)*2*sizeof(fftwf_complex).
	* \warning The returned buffer must be freed with the freeBuffer method of this class.
	*/
	void* getBuffer();

	/**
	* Frees one of the buffers reserved with the getRealOnlyBuffer(), getComplexOnlyBuffer() or getInplaceBuffer() method.
	* \param buffer A pointer to the buufer taht must be freed.
	*/
	void freeBuffer(void* buffer);
};

AUD_NAMESPACE_END
