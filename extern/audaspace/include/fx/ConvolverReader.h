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
* @file ConvolverReader.h
* @ingroup fx
* The ConvolverReader class.
*/

#include "IReader.h"
#include "ISound.h"
#include "Convolver.h"
#include "ImpulseResponse.h"
#include "util/FFTPlan.h"
#include "util/ThreadPool.h"

#include <memory>
#include <vector>
#include <future>

AUD_NAMESPACE_BEGIN

/**
* This class represents a reader for a sound that can be modified depending on a given impulse response.
*/
class AUD_API ConvolverReader : public IReader
{
private:
	/**
	* The current position.
	*/
	int m_position;

	/**
	* The reader of the input sound.
	*/
	std::shared_ptr<IReader> m_reader;

	/**
	* The impulse response in the frequency domain.
	*/
	std::shared_ptr<ImpulseResponse> m_ir;
	
	/**
	* The FFT size, given by the FFTPlan.
	*/
	int m_N;

	/**
	* The length of the impulse response fragments, m_N/2 will be used.
	*/
	int m_M;

	/**
	* The max length of the input slices, m_N/2 will be used.
	*/
	int m_L;

	/**
	* The array of convolvers that will be used, one per channel.
	*/
	std::vector<std::unique_ptr<Convolver>> m_convolvers;

	/**
	* The output buffer in which the convolved data will be written and from which the reader will read.
	*/
	sample_t* m_outBuffer;

	/**
	* A vector of buffers (one per channel) on which the audio signal will be separated per channel so it can be convolved.
	*/
	std::vector<sample_t*> m_vecInOut;

	/**
	* Current position in which the m_outBuffer is being read.
	*/
	int m_outBufferPos;
	
	/**
	* Effective length of the m_outBuffer.
	*/
	int m_eOutBufLen;

	/**
	* Real length of the m_outBuffer.
	*/
	int m_outBufLen;

	/**
	* Flag indicating whether the end of the sound has been reached or not.
	*/
	bool m_eosReader;

	/**
	* Flag indicating whether the end of the extra data generated in the convolution has been reached or not.
	*/
	bool m_eosTail;

	/**
	* The number of channels of the sound to be convolved.
	*/
	int m_inChannels;

	/**
	* The number of channels of the impulse response.
	*/
	int m_irChannels;

	/**
	* The number of threads used for channels.
	*/
	int m_nChannelThreads;

	/**
	* Length of the input data to be used by the channel threads.
	*/
	int m_lastLengthIn;

	/**
	* A shared ptr to a thread pool.
	*/
	std::shared_ptr<ThreadPool> m_threadPool;

	/** 
	* A vector of futures to sync tasks.
	*/
	std::vector<std::future<int>> m_futures;

	// delete copy constructor and operator=
	ConvolverReader(const ConvolverReader&) = delete;
	ConvolverReader& operator=(const ConvolverReader&) = delete;

public:
	/**
	* Creates a new convolver reader.
	* \param reader A reader of the input sound to be assigned to this reader.
	* \param ir A shared pointer to an impulseResponse object that will be used to convolve the sound.
	* \param threadPool A shared pointer to a ThreadPool object with 1 or more threads.
	* \param plan A shared pointer to and FFT plan that will be used for convolution.
	* \exception Exception thrown if impulse response doesn't match the specs (number fo channels and rate) of the input reader.
	*/
	ConvolverReader(std::shared_ptr<IReader> reader, std::shared_ptr<ImpulseResponse> ir, std::shared_ptr<ThreadPool> threadPool, std::shared_ptr<FFTPlan> plan);
	virtual ~ConvolverReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);

private:
	/**
	* Divides a sound buffer in several buffers, one per channel.
	* \param buffer The buffer that will be divided.
	* \param len The length of the buffer.
	*/
	void divideByChannel(const sample_t* buffer, int len);

	/**
	* Joins several buffers (one per channel) into the m_outBuffer.
	* \param start The starting position from which the m_outBuffer will be written.
	* \param len The amout of samples that will be joined.
	*/
	void joinByChannel(int start, int len);

	/**
	* Loads the m_outBuffer with data.
	*/
	void loadBuffer();

	/**
	* The function that the threads will run. It will process a subset of channels.
	* \param id An id number that will determine which subset of channels will be processed.
	* \param input A flag that will indicate if thare is input data.
	*		-If true there is new input data.
	*		-If false there isn't new input data.
	* \return The number of samples obtained.
	*/
	int threadFunction(int id, bool input);
};

AUD_NAMESPACE_END