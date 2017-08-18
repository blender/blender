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
* @file BinauralReader.h
* @ingroup fx
* The BinauralReader class.
*/

#include "IReader.h"
#include "ISound.h"
#include "Convolver.h"
#include "HRTF.h"
#include "Source.h"
#include "util/FFTPlan.h"
#include "util/ThreadPool.h"

#include <memory>
#include <vector>
#include <future>

AUD_NAMESPACE_BEGIN

/**
* This class represents a reader for a sound that can sound different depending on its realtive position with the listener.
*/
class AUD_API BinauralReader : public IReader
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
	* The HRTF set.
	*/
	std::shared_ptr<HRTF> m_hrtfs;

	/**
	* A Source object that will be used to change the source position of the sound.
	*/
	std::shared_ptr<Source> m_source;

	/**
	* The intended azimuth.
	*/
	float m_Azimuth;

	/**
	* The intended elevation.
	*/
	float m_Elevation;

	/**
	* The real azimuth being used.
	*/
	float m_RealAzimuth;

	/**
	* The real elevation being used.
	*/
	float m_RealElevation;

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
	* True if a transition is happening.
	*/
	bool m_transition;

	/**
	* The position of the current transition (decreasing)
	*/
	int m_transPos;

	/**
	* The output buffer in which the convolved data will be written and from which the reader will read.
	*/
	sample_t* m_outBuffer;

	/**
	* The input buffer that will hold the data to be convolved.
	*/
	sample_t* m_inBuffer;

	/**
	* Current position in which the m_outBuffer is being read.
	*/
	int m_outBufferPos;

	/**
	* Length of rhe m_outBuffer.
	*/
	int m_outBufLen;

	/**
	* Effective length of rhe m_outBuffer.
	*/
	int m_eOutBufLen;

	/**
	* Flag indicating whether the end of the sound has been reached or not.
	*/
	bool m_eosReader;

	/**
	* Flag indicating whether the end of the extra data generated in the convolution has been reached or not.
	*/
	bool m_eosTail;

	/**
	* A vector of buffers (one per channel) on which the audio signal will be separated per channel so it can be convolved.
	*/
	std::vector<sample_t*> m_vecOut;

	/**
	* A shared ptr to a thread pool.
	*/
	std::shared_ptr<ThreadPool> m_threadPool;

	/**
	* Length of the input data to be used by the channel threads.
	*/
	int m_lastLengthIn;

	/**
	* A vector of futures to sync tasks.
	*/
	std::vector<std::future<int>> m_futures;

	// delete copy constructor and operator=
	BinauralReader(const BinauralReader&) = delete;
	BinauralReader& operator=(const BinauralReader&) = delete;

public:
	/**
	* Creates a new convolver reader.
	* \param reader A reader of the input sound to be assigned to this reader. It must have one channel.
	* \param hrtfs A shared pointer to an HRTF object that will be used to get a particular impulse response depending on the source.
	* \param source A shared pointer to a Source object that will be used to change the source position of the sound.
	* \param threadPool A shared pointer to a ThreadPool object with 1 or more threads.
	* \param plan A shared pointer to and FFT plan that will be used for convolution.
	* \exception Exception thrown if the specs of the HRTFs and the sound don't match or if the provided HRTF object is empty.
	*/
	BinauralReader(std::shared_ptr<IReader> reader, std::shared_ptr<HRTF> hrtfs, std::shared_ptr<Source> source, std::shared_ptr<ThreadPool> threadPool, std::shared_ptr<FFTPlan> plan);
	virtual ~BinauralReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);

private:
	/**
	* Joins several buffers (one per channel) into the m_outBuffer.
	* \param start The starting position from which the m_outBuffer will be written.
	* \param len The amout of samples that will be joined.
	* \param nConvolvers The number of convolvers that have been used. Only use 2 or 4 as possible values.
						 If the value is 4 the result will be interpolated.
	*/
	void joinByChannel(int start, int len, int nConvolvers);

	/**
	* Loads the m_outBuffer with data.
	* \param nConvolvers The number of convolver objects that will be used. Only 2 or 4 should be used.
	*/
	void loadBuffer(int nConvolvers);

	/**
	* The function that the threads will run. It will process a subset of channels.
	* \param id An id number that will determine which subset of channels will be processed.
	* \param input A flag that will indicate if thare is input data.
	*		-If true there is new input data.
	*		-If false there isn't new input data.
	* \return The number of samples obtained.
	*/
	int threadFunction(int id, bool input);

	bool checkSource();
};

AUD_NAMESPACE_END