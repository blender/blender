/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
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

#include "fx/IIRFilterReader.h"

AUD_NAMESPACE_BEGIN

IIRFilterReader::IIRFilterReader(std::shared_ptr<IReader> reader, const std::vector<float>& b, const std::vector<float>& a) :
	BaseIIRFilterReader(reader, b.size(), a.size()), m_a(a), m_b(b)
{
	if(m_a.empty() == false)
	{
		for(int i = 1; i < m_a.size(); i++)
			m_a[i] /= m_a[0];
		for(int i = 0; i < m_b.size(); i++)
			m_b[i] /= m_a[0];
		m_a[0] = 1;
	}
}

sample_t IIRFilterReader::filter()
{
	sample_t out = 0;

	for(int i = 1; i < m_a.size(); i++)
		out -= y(-i) * m_a[i];
	for(int i = 0; i < m_b.size(); i++)
		out += x(-i) * m_b[i];

	return out;
}

void IIRFilterReader::setCoefficients(const std::vector<float>& b, const std::vector<float>& a)
{
	setLengths(b.size(), a.size());
	m_a = a;
	m_b = b;
}

AUD_NAMESPACE_END
