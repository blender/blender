#ifndef AUD_HIGHPASSCALCULATOR_H
#define AUD_HIGHPASSCALCULATOR_H

#include "AUD_IDynamicIIRFilterCalculator.h"

class AUD_HighpassCalculator : public AUD_IDynamicIIRFilterCalculator
{
private:
	/**
	 * The cutoff frequency.
	 */
	const float m_frequency;

	/**
	 * The Q factor.
	 */
	const float m_Q;

public:
	AUD_HighpassCalculator(float frequency, float Q);

	virtual void recalculateCoefficients(AUD_SampleRate rate, std::vector<float> &b, std::vector<float> &a);
};

#endif // AUD_HIGHPASSCALCULATOR_H
