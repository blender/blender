#include "AUD_ButterworthCalculator.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BWPB41 0.76536686473
#define BWPB42 1.84775906502

AUD_ButterworthCalculator::AUD_ButterworthCalculator(float frequency) :
	m_frequency(frequency)
{
}

void AUD_ButterworthCalculator::recalculateCoefficients(AUD_SampleRate rate, std::vector<float> &b, std::vector<float> &a)
{
	float omega = 2 * tan(m_frequency * M_PI / rate);
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
