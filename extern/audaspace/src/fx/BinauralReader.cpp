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

#include "fx/BinauralReader.h"
#include "Exception.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>

#define NUM_OUTCHANNELS 2
#define NUM_CONVOLVERS 4
#define CROSSFADE_SAMPLES 1024

AUD_NAMESPACE_BEGIN
BinauralReader::BinauralReader(std::shared_ptr<IReader> reader, std::shared_ptr<HRTF> hrtfs, std::shared_ptr<Source> source, std::shared_ptr<ThreadPool> threadPool, std::shared_ptr<FFTPlan> plan) :
	m_position(0), m_reader(reader), m_hrtfs(hrtfs), m_source(source), m_N(plan->getSize()), m_transition(false), m_transPos(CROSSFADE_SAMPLES*NUM_OUTCHANNELS), m_eosReader(false), m_eosTail(false), m_threadPool(threadPool)
{
	if(m_hrtfs->isEmpty())
		AUD_THROW(StateException, "The provided HRTF object is empty");
	if(m_reader->getSpecs().channels != 1) 
		AUD_THROW(StateException, "The sound must have only one channel");
	if(m_reader->getSpecs().rate != m_hrtfs->getSpecs().rate)
		AUD_THROW(StateException, "The sound and the HRTFs must have the same rate");
	m_M = m_L = m_N / 2;
	
	m_RealAzimuth = m_Azimuth = m_source->getAzimuth();
	m_RealElevation = m_Elevation = m_source->getElevation();
	auto irs = m_hrtfs->getImpulseResponse(m_RealAzimuth, m_RealElevation);
	for(unsigned int i = 0; i < NUM_CONVOLVERS; i++)
		if(i%NUM_OUTCHANNELS==0)
			m_convolvers.push_back(std::unique_ptr<Convolver>(new Convolver(irs.first->getChannel(0), irs.first->getLength(), m_threadPool, plan)));
		else
			m_convolvers.push_back(std::unique_ptr<Convolver>(new Convolver(irs.second->getChannel(0), irs.second->getLength(), m_threadPool, plan)));
	m_futures.resize(NUM_CONVOLVERS);

	m_outBuffer = (sample_t*)std::malloc(m_L*NUM_OUTCHANNELS*sizeof(sample_t));
	m_eOutBufLen = m_outBufLen = m_outBufferPos = m_L * NUM_OUTCHANNELS;
	m_inBuffer = (sample_t*)std::malloc(m_L * sizeof(sample_t));
	for(int i = 0; i < NUM_CONVOLVERS; i++)
		m_vecOut.push_back((sample_t*)std::calloc(m_L, sizeof(sample_t)));
}

BinauralReader::~BinauralReader()
{
	std::free(m_outBuffer);
	std::free(m_inBuffer);
	for(int i = 0; i < m_vecOut.size(); i++)
		std::free(m_vecOut[i]);
}

bool BinauralReader::isSeekable() const
{
	return m_reader->isSeekable();
}

void BinauralReader::seek(int position)
{
	m_position = position;
	m_reader->seek(position);
	for(int i = 0; i < NUM_CONVOLVERS; i++)
		m_convolvers[i]->reset();
	m_eosTail = false;
	m_eosReader = false;
	m_outBufferPos = m_eOutBufLen = m_outBufLen;
	m_transition = false;
	m_transPos = CROSSFADE_SAMPLES*NUM_OUTCHANNELS;
}

int BinauralReader::getLength() const
{
	return m_reader->getLength();
}

int BinauralReader::getPosition() const
{
	return m_position;
}

Specs BinauralReader::getSpecs() const
{
	Specs specs = m_reader->getSpecs();
	specs.channels = CHANNELS_STEREO;
	return specs;
}

void BinauralReader::read(int& length, bool& eos, sample_t* buffer)
{
	int samples = 0;
	int iteration = 0;
	if(length <= 0)
	{
		length = 0;
		eos = (m_eosTail && m_outBufferPos >= m_eOutBufLen);
		return;
	}

	eos = false;
	int writePos = 0;
	do
	{
		int bufRest = m_eOutBufLen - m_outBufferPos;
		int writeLength = std::min((length*NUM_OUTCHANNELS) - writePos, m_eOutBufLen + bufRest);
		if(bufRest < writeLength || (m_eOutBufLen == 0 && m_eosTail))
		{
			if(bufRest > 0)
				std::memcpy(buffer + writePos, m_outBuffer + m_outBufferPos, bufRest*sizeof(sample_t));
			if(!m_eosTail)
			{
				int n = NUM_OUTCHANNELS;
				if(m_transition)
					n = NUM_CONVOLVERS;
				else if(checkSource())
					n = NUM_CONVOLVERS;
				loadBuffer(n);

				int len = std::min(std::abs(writeLength - bufRest), m_eOutBufLen);
				std::memcpy(buffer + writePos + bufRest, m_outBuffer, len*sizeof(sample_t));
				samples += len;
				m_outBufferPos = len;
				writeLength = std::min((length*NUM_OUTCHANNELS) - writePos, m_eOutBufLen + bufRest);
			}
			else
			{
				m_outBufferPos += bufRest;
				length = (writePos+bufRest) / NUM_OUTCHANNELS;
				eos = true;
				return;
			}
		}
		else
		{
			std::memcpy(buffer + writePos, m_outBuffer + m_outBufferPos, writeLength*sizeof(sample_t));
			m_outBufferPos += writeLength;
		}
		writePos += writeLength;
		iteration++;
	} while(writePos < length*NUM_OUTCHANNELS);
	m_position += length;
}

bool BinauralReader::checkSource()
{
	if((m_Azimuth != m_source->getAzimuth() || m_Elevation != m_source->getElevation()) && (!m_eosReader && !m_eosTail))
	{
		float az = m_Azimuth = m_source->getAzimuth();
		float el = m_Elevation = m_source->getElevation();
		auto irs = m_hrtfs->getImpulseResponse(az, el);
		if(az != m_RealAzimuth || el != m_RealElevation)
		{
			m_RealAzimuth = az;
			m_RealElevation = el;
			for(int i = 0; i < NUM_OUTCHANNELS; i++)
			{
				auto temp = std::move(m_convolvers[i]);
				m_convolvers[i] = std::move(m_convolvers[i + NUM_OUTCHANNELS]);
				m_convolvers[i + NUM_OUTCHANNELS] = std::move(temp);
			}
			for(int i = 0; i < NUM_OUTCHANNELS; i++)
				if(i%NUM_OUTCHANNELS == 0)
					m_convolvers[i]->setImpulseResponse(irs.first->getChannel(0));
				else
					m_convolvers[i]->setImpulseResponse(irs.second->getChannel(0));

			m_transPos = CROSSFADE_SAMPLES*NUM_OUTCHANNELS;
			m_transition = true;
			return true;
		}
	}
	return false;
}

void BinauralReader::loadBuffer(int nConvolvers)
{
	m_lastLengthIn = m_L;
	m_reader->read(m_lastLengthIn, m_eosReader, m_inBuffer);
	if(!m_eosReader || m_lastLengthIn > 0)
	{
		int len = m_lastLengthIn;
		for(int i = 0; i < nConvolvers; i++)
			m_futures[i] = m_threadPool->enqueue(&BinauralReader::threadFunction, this, i, true);
		for(int i = 0; i < nConvolvers; i++)
			len = m_futures[i].get();

		joinByChannel(0, len, nConvolvers);
		m_eOutBufLen = len*NUM_OUTCHANNELS;
	}
	else if(!m_eosTail)
	{
		int len = m_lastLengthIn = m_L;
		for(int i = 0; i < nConvolvers; i++)
			m_futures[i] = m_threadPool->enqueue(&BinauralReader::threadFunction, this, i, false);
		for(int i = 0; i < nConvolvers; i++)
			len = m_futures[i].get();

		joinByChannel(0, len, nConvolvers);
		m_eOutBufLen = len*NUM_OUTCHANNELS;
	}
}

void BinauralReader::joinByChannel(int start, int len, int nConvolvers)
{
	int k = 0;
	float vol = 0;
	const int l = CROSSFADE_SAMPLES*NUM_OUTCHANNELS;
	for(int i = 0; i < len*NUM_OUTCHANNELS; i += NUM_OUTCHANNELS)
	{
		if(m_transition)
		{
			vol = (m_transPos - i) / (float)l;
			if(vol > 1.0f)
				vol = 1.0f;
			else if(vol < 0.0f)
				vol = 0.0f;
		}

		for(int j = 0; j < NUM_OUTCHANNELS; j++)
			m_outBuffer[i + j + start] = ((m_vecOut[j][k] * (1.0f - vol)) + (m_vecOut[j + NUM_OUTCHANNELS][k] * vol))*m_source->getVolume();
		k++;
	}
	if(m_transition)
	{
		m_transPos -= len*NUM_OUTCHANNELS;
		if(m_transPos <= 0)
		{
			m_transition = false;
			m_transPos = l;
		}
	}
}

int BinauralReader::threadFunction(int id, bool input)
{
	int l = m_lastLengthIn;
	if(input)
		m_convolvers[id]->getNext(m_inBuffer, m_vecOut[id], l, m_eosTail);
	else
		m_convolvers[id]->getNext(nullptr, m_vecOut[id], l, m_eosTail);
	return l;
}

AUD_NAMESPACE_END
