#ifndef AUD_LOWPASSCALCULATOR_H
#define AUD_LOWPASSCALCULATOR_H

#include "AUD_IDynamicIIRFilterCalculator.h"

class AUD_LowpassCalculator : public AUD_IDynamicIIRFilterCalculator
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
	AUD_LowpassCalculator(float frequency, float Q);

	virtual void recalculateCoefficients(AUD_SampleRate rate, std::vector<float> &b, std::vector<float> &a);
};

#endif // AUD_LOWPASSCALCULATOR_H
