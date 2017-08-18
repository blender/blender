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

#include "fx/ButterworthCalculator.h"

#include <cmath>

#define BWPB41 0.76536686473
#define BWPB42 1.84775906502

AUD_NAMESPACE_BEGIN

ButterworthCalculator::ButterworthCalculator(float frequency) :
	m_frequency(frequency)
{
}

void ButterworthCalculator::recalculateCoefficients(SampleRate rate, std::vector<float> &b, std::vector<float> &a)
{
	float omega = 2 * std::tan(m_frequency * M_PI / rate);
	float o2 = omega * omega;
	float o4 = o2 * o2;
	float x1 = o2 + 2.0f * (float)BWPB41 * omega + 4.0f;
	float x2 = o2 + 2.0f * (float)BWPB42 * omega + 4.0f;
	float y1 = o2 - 2.0f * (float)BWPB41 * omega + 4.0f;
	float y2 = o2 - 2.0f * (float)BWPB42 * omega + 4.0f;
	float o228 = 2.0f * o2 - 8.0f;
	float norm = x1 * x2;
	a.push_back(1);
	a.push_back((x1 + x2) * o228 / norm);
	a.push_back((x1 * y2 + x2 * y1 + o228 * o228) / norm);
	a.push_back((y1 + y2) * o228 / norm);
	a.push_back(y1 * y2 / norm);
	b.push_back(o4 / norm);
	b.push_back(4 * o4 / norm);
	b.push_back(6 * o4 / norm);
	b.push_back(b[1]);
	b.push_back(b[0]);
}

AUD_NAMESPACE_END
