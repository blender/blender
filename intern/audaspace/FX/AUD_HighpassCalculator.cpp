#include "AUD_HighpassCalculator.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AUD_HighpassCalculator::AUD_HighpassCalculator(float frequency, float Q) :
	m_frequency(frequency),
	m_Q(Q)
{
}

void AUD_HighpassCalculator::recalculateCoefficients(AUD_SampleRate rate, std::vector<float> &b, std::vector<float> &a)
{
	float w0 = 2.0 * M_PI * (AUD_SampleRate)m_frequency / rate;
	float alpha = (float)(sin(w0) / (2.0 * (double)m_Q));
	float norm = 1 + alpha;
	float c = cos(w0);
	a.push_back(1);
	a.push_back(-2 * c / norm);
	a.push_back((1 - alpha) / norm);
	b.push_back((1 + c) / (2 * norm));
	b.push_back((-1 - c) / norm);
	b.push_back(b[0]);
}
