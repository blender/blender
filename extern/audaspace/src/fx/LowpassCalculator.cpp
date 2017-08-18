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

#include "fx/LowpassCalculator.h"

#include <cmath>

AUD_NAMESPACE_BEGIN

LowpassCalculator::LowpassCalculator(float frequency, float Q) :
	m_frequency(frequency),
	m_Q(Q)
{
}

void LowpassCalculator::recalculateCoefficients(SampleRate rate, std::vector<float> &b, std::vector<float> &a)
{
	float w0 = 2 * M_PI * m_frequency / rate;
	float alpha = std::sin(w0) / (2 * m_Q);
	float norm = 1 + alpha;
	float c = std::cos(w0);
	a.push_back(1);
	a.push_back(-2 * c / norm);
	a.push_back((1 - alpha) / norm);
	b.push_back((1 - c) / (2 * norm));
	b.push_back((1 - c) / norm);
	b.push_back(b[0]);
}

AUD_NAMESPACE_END
