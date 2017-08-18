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

#include "fx/Convolver.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <cstring>

AUD_NAMESPACE_BEGIN
Convolver::Convolver(std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::complex<sample_t>>>>> ir, int irLength, std::shared_ptr<ThreadPool> threadPool, std::shared_ptr<FFTPlan> plan) :
	m_N(plan->getSize()), m_M(plan->getSize()/2), m_L(plan->getSize()/2), m_irBuffers(ir), m_irLength(irLength), m_threadPool(threadPool), m_numThreads(std::min(threadPool->getNumOfThreads(), static_cast<unsigned int>(m_irBuffers->size() - 1))), m_tailCounter(0), m_eos(false)
	
{
	m_resetFlag = false;
	m_futures.resize(m_numThreads);
	for(int i = 0; i < m_irBuffers->size(); i++)
	{
		m_fftConvolvers.push_back(std::unique_ptr<FFTConvolver>(new FFTConvolver((*m_irBuffers)[i], plan)));
		m_delayLine.push_front((fftwf_complex*)std::calloc((m_N / 2) + 1, sizeof(fftwf_complex)));
	}

	m_accBuffer = (fftwf_complex*)std::calloc((m_N / 2) + 1, sizeof(fftwf_complex));
	for(int i = 0; i < m_numThreads; i++)
		m_threadAccBuffers.push_back((fftwf_complex*)std::calloc((m_N / 2) + 1, sizeof(fftwf_complex)));
}

Convolver::~Convolver()
{
	m_resetFlag = true;
	for(auto &fut : m_futures)
		if(fut.valid())
			fut.get();

	std::free(m_accBuffer);
	for(auto buf : m_threadAccBuffers)
		std::free(buf);
	while(!m_delayLine.empty())
	{
		std::free(m_delayLine.front());
		m_delayLine.pop_front();
	}
}

void Convolver::getNext(sample_t* inBuffer, sample_t* outBuffer, int& length, bool& eos)
{
	if(length > m_L)
	{
		length = 0;
		eos = m_eos;
		return;
	}
	if(m_eos)
	{
		eos = m_eos;
		length = 0;
		return;
	}

	eos = false;
	for(auto &fut : m_futures)
		if(fut.valid())
			fut.get();
	
	if(inBuffer != nullptr)
		m_fftConvolvers[0]->getNextFDL(inBuffer, reinterpret_cast<std::complex<sample_t>*>(m_accBuffer), length, m_delayLine[0]);
	else
	{
		m_tailCounter++;
		std::memset(outBuffer, 0, m_L*sizeof(sample_t));
		m_fftConvolvers[0]->getNextFDL(outBuffer, reinterpret_cast<std::complex<sample_t>*>(m_accBuffer), length, m_delayLine[0]);
	}
	m_delayLine.push_front(m_delayLine.back());
	m_delayLine.pop_back();
	length = m_L;
	m_fftConvolvers[0]->IFFT_FDL(m_accBuffer, outBuffer, length);
	std::memset(m_accBuffer, 0, ((m_N / 2) + 1)*sizeof(fftwf_complex));

	if(m_tailCounter >= m_delayLine.size() && inBuffer == nullptr)
	{
		eos = m_eos = true;
		length = m_irLength%m_M;
		if(length == 0)
			length = m_M;
	}
	else
		for(int i = 0; i < m_futures.size(); i++)
			m_futures[i] = m_threadPool->enqueue(&Convolver::threadFunction, this, i);
}

void Convolver::reset()
{
	m_resetFlag = true;
	for(auto &fut : m_futures)
		if(fut.valid())
			fut.get();

	for(int i = 0; i < m_delayLine.size();i++)
		std::memset(m_delayLine[i], 0, ((m_N / 2) + 1)*sizeof(fftwf_complex));
	for(int i = 0; i < m_fftConvolvers.size(); i++)
		m_fftConvolvers[i]->clear();
	std::memset(m_accBuffer, 0, ((m_N / 2) + 1)*sizeof(fftwf_complex));
	m_tailCounter = 0;
	m_eos = false;
	m_resetFlag = false;
}

std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::complex<sample_t>>>>> Convolver::getImpulseResponse()
{
	return m_irBuffers;
}

void Convolver::setImpulseResponse(std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::complex<sample_t>>>>> ir)
{
	reset();
	m_irBuffers = ir;
	for(int i = 0; i < m_irBuffers->size(); i++)
		m_fftConvolvers[i]->setImpulseResponse((*m_irBuffers)[i]);
}

bool Convolver::threadFunction(int id)
{
	int total = m_irBuffers->size();
	int share = std::ceil(((float)total - 1) / (float)m_numThreads);
	int start = id*share + 1;
	int end = std::min(start + share, total);

	std::memset(m_threadAccBuffers[id], 0, ((m_N / 2) + 1)*sizeof(fftwf_complex));

	for(int i = start; i < end && !m_resetFlag; i++)
		m_fftConvolvers[i]->getNextFDL(reinterpret_cast<std::complex<sample_t>*>(m_delayLine[i]), reinterpret_cast<std::complex<sample_t>*>(m_threadAccBuffers[id]));

	m_sumMutex.lock();
	for(int i = 0; (i < m_N / 2 + 1) && !m_resetFlag; i++)
	{
		m_accBuffer[i][0] += m_threadAccBuffers[id][i][0];
		m_accBuffer[i][1] += m_threadAccBuffers[id][i][1];
	}
	m_sumMutex.unlock();
	return true;
}
AUD_NAMESPACE_END
