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

#include "fx/FFTConvolver.h"

#include <cstring>
#include <cstdlib>

AUD_NAMESPACE_BEGIN

FFTConvolver::FFTConvolver(std::shared_ptr<std::vector<std::complex<sample_t>>> ir, std::shared_ptr<FFTPlan> plan) :
	m_plan(plan), m_N(plan->getSize()), m_M(plan->getSize()/2), m_L(plan->getSize()/2), m_tailPos(0), m_irBuffer(ir)
{
	m_tail = (float*)calloc(m_M - 1, sizeof(float));
	m_realBufLen = ((m_N / 2) + 1) * 2;
	m_inBuffer = nullptr;
	m_shiftBuffer = (sample_t*)std::calloc(m_N, sizeof(sample_t));
}

FFTConvolver::~FFTConvolver()
{
	std::free(m_tail);
	std::free(m_shiftBuffer);
	if(m_inBuffer != nullptr)
		m_plan->freeBuffer(m_inBuffer);
}

void FFTConvolver::getNext(const sample_t* inBuffer, sample_t* outBuffer, int& length)
{
	if(length > m_L || length <= 0)
	{
		length = 0;
		return;
	}
	if(m_inBuffer == nullptr)
		m_inBuffer = reinterpret_cast<std::complex<sample_t>*>(m_plan->getBuffer());

	std::memset(m_inBuffer, 0, m_realBufLen * sizeof(fftwf_complex));
	std::memcpy(m_inBuffer, inBuffer, length*sizeof(sample_t));

	m_plan->FFT(m_inBuffer);
	for(int i = 0; i < m_realBufLen / 2; i++)
	{
		m_inBuffer[i] = m_inBuffer[i] * (*m_irBuffer)[i] / sample_t(m_N);
	}
	m_plan->IFFT(m_inBuffer);

	for(int i = 0; i < m_M - 1; i++)
		((float*)m_inBuffer)[i] += m_tail[i];

	for(int i = 0; i < m_M - 1; i++)
		m_tail[i] = ((float*)m_inBuffer)[i + length];

	std::memcpy(outBuffer, m_inBuffer, length * sizeof(sample_t));
}

void FFTConvolver::getNext(const sample_t* inBuffer, sample_t* outBuffer, int& length, fftwf_complex* transformedData)
{
	if(length > m_L || length <= 0)
	{
		length = 0;
		return;
	}
	if(m_inBuffer == nullptr)
		m_inBuffer = reinterpret_cast<std::complex<sample_t>*>(m_plan->getBuffer());

	std::memset(m_inBuffer, 0, m_realBufLen * sizeof(fftwf_complex));
	std::memcpy(m_inBuffer, inBuffer, length*sizeof(sample_t));

	m_plan->FFT(m_inBuffer);
	std::memcpy(transformedData, m_inBuffer, (m_realBufLen / 2)*sizeof(fftwf_complex));
	for(int i = 0; i < m_realBufLen / 2; i++)
	{
		m_inBuffer[i] = m_inBuffer[i] * (*m_irBuffer)[i] / sample_t(m_N);
	}
	m_plan->IFFT(m_inBuffer);

	for(int i = 0; i < m_M - 1; i++)
		((float*)m_inBuffer)[i] += m_tail[i];

	for(int i = 0; i < m_M - 1; i++)
		m_tail[i] = ((float*)m_inBuffer)[i + length];

	std::memcpy(outBuffer, m_inBuffer, length * sizeof(sample_t));
}

void FFTConvolver::getNext(const fftwf_complex* inBuffer, sample_t* outBuffer, int& length)
{
	if(length > m_L || length <= 0)
	{
		length = 0;
		return;
	}
	if(m_inBuffer == nullptr)
		m_inBuffer = reinterpret_cast<std::complex<sample_t>*>(m_plan->getBuffer());

	std::memset(m_inBuffer, 0, m_realBufLen * sizeof(fftwf_complex));
	for(int i = 0; i < m_realBufLen / 2; i++)
	{
		m_inBuffer[i] = m_inBuffer[i] * (*m_irBuffer)[i] / sample_t(m_N);
	}
	m_plan->IFFT(m_inBuffer);

	for(int i = 0; i < m_M - 1; i++)
		((float*)m_inBuffer)[i] += m_tail[i];

	for(int i = 0; i < m_M - 1; i++)
		m_tail[i] = ((float*)m_inBuffer)[i + length];

	std::memcpy(outBuffer, m_inBuffer, length * sizeof(sample_t));
}

void FFTConvolver::getTail(int& length, bool& eos, sample_t* buffer)
{
	if(length <= 0)
	{
		length = 0;
		eos = m_tailPos >= m_M - 1;
		return;
	}

	eos = false;
	if(m_tailPos + length > m_M - 1)
	{
		length = m_M - 1 - m_tailPos;
		if(length < 0)
			length = 0;
		eos = true;
		m_tailPos = m_M - 1;
	}
	else
		m_tailPos += length;
	std::memcpy(buffer, m_tail, length*sizeof(sample_t));
}

void FFTConvolver::clear()
{
	std::memset(m_shiftBuffer, 0, m_N * sizeof(sample_t));
	std::memset(m_tail, 0, m_M - 1);
}

void FFTConvolver::IFFT_FDL(const fftwf_complex* inBuffer, sample_t* outBuffer, int& length)
{
	if(length > m_L || length <= 0)
	{
		length = 0;
		return;
	}
	if(m_inBuffer == nullptr)
		m_inBuffer = reinterpret_cast<std::complex<sample_t>*>(m_plan->getBuffer());

	std::memset(m_inBuffer, 0, m_realBufLen * sizeof(fftwf_complex));
	std::memcpy(m_inBuffer, inBuffer, (m_realBufLen / 2)*sizeof(fftwf_complex));
	m_plan->IFFT(m_inBuffer);
	std::memcpy(outBuffer, ((sample_t*)m_inBuffer)+m_L, length*sizeof(sample_t));
}

void FFTConvolver::getNextFDL(const std::complex<sample_t>* inBuffer, std::complex<sample_t>* accBuffer)
{
	for(int i = 0; i < m_realBufLen / 2; i++)
	{
		accBuffer[i] += (inBuffer[i] * (*m_irBuffer)[i]) / sample_t(m_N);
	}
}

void FFTConvolver::getNextFDL(const sample_t* inBuffer, std::complex<sample_t>* accBuffer, int& length, fftwf_complex* transformedData)
{
	if(length > m_L || length <= 0)
	{
		length = 0;
		return;
	}
	if(m_inBuffer == nullptr)
		m_inBuffer = reinterpret_cast<std::complex<sample_t>*>(m_plan->getBuffer());

	std::memcpy(m_shiftBuffer, m_shiftBuffer + m_L, m_L*sizeof(sample_t));
	std::memcpy(m_shiftBuffer + m_L, inBuffer, length*sizeof(sample_t));

	std::memset(m_inBuffer, 0, m_realBufLen * sizeof(fftwf_complex));
	std::memcpy(m_inBuffer, m_shiftBuffer, (m_L+length)*sizeof(sample_t));

	m_plan->FFT(m_inBuffer);
	std::memcpy(transformedData, m_inBuffer, (m_realBufLen / 2)*sizeof(fftwf_complex));
	for(int i = 0; i < m_realBufLen / 2; i++)
	{
		accBuffer[i] += (m_inBuffer[i] * (*m_irBuffer)[i]) / sample_t(m_N);
	}
}


void FFTConvolver::setImpulseResponse(std::shared_ptr<std::vector<std::complex<sample_t>>> ir)
{
	clear();
	m_irBuffer = ir;
}

std::shared_ptr<std::vector<std::complex<sample_t>>> FFTConvolver::getImpulseResponse()
{
	return m_irBuffer;
}
AUD_NAMESPACE_END
