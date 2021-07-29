#ifndef AUD_BUTTERWORTHCALCULATOR_H
#define AUD_BUTTERWORTHCALCULATOR_H

#include "AUD_IDynamicIIRFilterCalculator.h"

class AUD_ButterworthCalculator : public AUD_IDynamicIIRFilterCalculator
{
private:
	/**
	 * The attack value in seconds.
	 */
	const float m_frequency;

public:
	AUD_ButterworthCalculator(float frequency);

	virtual void recalculateCoefficients(AUD_SampleRate rate, std::vector<float> &b, std::vector<float> &a);
};

#endif // AUD_BUTTERWORTHCALCULATOR_H
