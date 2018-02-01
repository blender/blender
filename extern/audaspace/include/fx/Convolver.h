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
* @file Convolver.h
* @ingroup fx
* The Convolver class.
*/

#include "FFTConvolver.h"
#include "util/ThreadPool.h"
#include "util/FFTPlan.h"

#include <memory>
#include <vector>
#include <mutex>
#include <future>
#include <atomic>
#include <deque>

AUD_NAMESPACE_BEGIN
/**
* This class allows to convolve a sound with a very large impulse response.
*/
class AUD_API Convolver
{
private:
	/**
	* The FFT size, must be at least M+L-1.
	*/
	int m_N;

	/**
	* The length of the impulse response parts.
	*/
	int m_M;

	/**
	* The max length of the input slices.
	*/
	int m_L;

	/**
	* The impulse response divided in parts.
	*/
	std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::complex<sample_t>>>>> m_irBuffers;

	/**
	* Accumulation buffers for the threads.
	*/
	std::vector<fftwf_complex*> m_threadAccBuffers;

	/**
	* A vector of FFTConvolvers used to calculate the partial convolutions.
	*/
	std::vector<std::unique_ptr<FFTConvolver>> m_fftConvolvers;

	/**
	* The actual number of threads being used.
	*/
	int m_numThreads;

	/**
	* A pool of threads that will be used for convolution.
	*/
	std::shared_ptr<ThreadPool> m_threadPool;

	/**
	* A vector of futures used for thread sync
	*/
	std::vector<std::future<bool>> m_futures;

	/**
	* A mutex for the sum of thread accumulators.
	*/
	std::mutex m_sumMutex;

	/**
	* A flag to control thread execution when a reset is scheduled.
	*/
	std::atomic_bool m_resetFlag;

	/**
	* Global accumulation buffer.
	*/
	fftwf_complex* m_accBuffer;

	/**
	* Delay line.
	*/
	std::deque<fftwf_complex*> m_delayLine;

	/**
	* The complete length of the impulse response.
	*/
	int m_irLength;

	/**
	* Counter for the tail;
	*/
	int m_tailCounter;

	/**
	* Flag end of sound;
	*/
	bool m_eos;

	// delete copy constructor and operator=
	Convolver(const Convolver&) = delete;
	Convolver& operator=(const Convolver&) = delete;

public:

	/**
	* Creates a new FFTConvolver.
	* \param ir A shared pointer to a vector with the data of the various impulse response parts in the frequency domain (see ImpulseResponse class for an easy way to obtain it).
	* \param irLength The length of the full impulse response.
	* \param threadPool A shared pointer to a ThreadPool object with 1 or more threads.
	* \param plan A shared pointer to a FFT plan that will be used for convolution.
	*/
	Convolver(std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::complex<sample_t>>>>> ir, int irLength, std::shared_ptr<ThreadPool> threadPool, std::shared_ptr<FFTPlan> plan);

	virtual ~Convolver();

	/**
	* Convolves the data that is provided with the inpulse response.
	* Given a plan of size N, the amount of samples convolved by one call to this method will be N/2.
	* \param[in] inBuffer A buffer with the input data to be convolved, nullptr if the source sound has ended (the convolved sound is larger than the source sound).
	* \param[in] outBuffer A buffer in which the convolved data will be written. Its size must be at least N/2.
	* \param[in,out] length The number of samples you wish to obtain. If an inBuffer is provided this argument must match its length.
	*						When this method returns, the value of length represents the number of samples written into the outBuffer.
	* \param[out] eos True if the end of the sound is reached, false otherwise.
	*/
	void getNext(sample_t* inBuffer, sample_t* outBuffer, int& length, bool& eos);

	/**
	* Resets all the internally stored data so the convolution of a new sound can be started. 
	*/
	void reset();

	/**
	* Retrieves the current impulse response being used.
	* \return The current impulse response.
	*/
	std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::complex<sample_t>>>>> getImpulseResponse();

	/**
	* Changes the impulse response and resets the convolver.
	* \param ir A shared pointer to a vector with the data of the various impulse response parts in the frequency domain (see ImpulseResponse class for an easy way to obtain it).
	*/
	void setImpulseResponse(std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::complex<sample_t>>>>> ir);

private:

	/**
	* This function will be enqueued into the thread pool, and will process the input signal with a subset of the impulse response parts.
	* \param id The id of the thread, starting with 0.
	*/
	bool threadFunction(int id);
};

AUD_NAMESPACE_END
