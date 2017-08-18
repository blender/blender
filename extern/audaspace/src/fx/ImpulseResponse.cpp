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

#include "fx/ImpulseResponse.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cmath>

AUD_NAMESPACE_BEGIN
ImpulseResponse::ImpulseResponse(std::shared_ptr<StreamBuffer> impulseResponse) :
	ImpulseResponse(impulseResponse, std::make_shared<FFTPlan>(0.0))
{
}

ImpulseResponse::ImpulseResponse(std::shared_ptr<StreamBuffer> impulseResponse, std::shared_ptr<FFTPlan> plan)
{
	auto reader = impulseResponse->createReader();
	m_length = reader->getLength();
	processImpulseResponse(impulseResponse->createReader(), plan);
}

Specs ImpulseResponse::getSpecs()
{
	return m_specs;
}

int ImpulseResponse::getLength()
{
	return m_length;
}

std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::complex<sample_t>>>>> ImpulseResponse::getChannel(int n)
{
	return m_processedIR[n];
}

void ImpulseResponse::processImpulseResponse(std::shared_ptr<IReader> reader, std::shared_ptr<FFTPlan> plan)
{
	m_specs.channels = reader->getSpecs().channels;
	m_specs.rate = reader->getSpecs().rate;
	int N = plan->getSize();
	bool eos = false;
	int length = reader->getLength();
	sample_t* buffer = (sample_t*)std::malloc(length * m_specs.channels * sizeof(sample_t));
	int numParts = std::ceil((float)length / (plan->getSize() / 2));

	for(int i = 0; i < m_specs.channels; i++)
	{
		m_processedIR.push_back(std::make_shared<std::vector<std::shared_ptr<std::vector<std::complex<sample_t>>>>>());
		for(int j = 0; j < numParts; j++)
			(*m_processedIR[i]).push_back(std::make_shared<std::vector<std::complex<sample_t>>>((N / 2) + 1));
	}
	length += reader->getSpecs().rate;
	reader->read(length, eos, buffer);


	void* bufferFFT = plan->getBuffer();
	for(int i = 0; i < m_specs.channels; i++)
	{
		int partStart = 0;
		for(int h = 0; h < numParts; h++)
		{
			int k = 0;
			int len = std::min(partStart + ((N / 2)*m_specs.channels), length*m_specs.channels);
			std::memset(bufferFFT, 0, ((N / 2) + 1) * 2 * sizeof(fftwf_complex));
			for(int j = partStart; j < len; j += m_specs.channels)
			{
				((float*)bufferFFT)[k] = buffer[j + i];
				k++;
			}
			plan->FFT(bufferFFT);
			for(int j = 0; j < (N / 2) + 1; j++)
			{
				(*(*m_processedIR[i])[h])[j] = reinterpret_cast<std::complex<sample_t>*>(bufferFFT)[j];
			}
			partStart += N / 2 * m_specs.channels;
		}
	}
	plan->freeBuffer(bufferFFT);
	std::free(buffer);
}
AUD_NAMESPACE_END
