#pragma once

/**
 * @file Echo.h
 * @ingroup fx
 * The Echo class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

class AUD_API Echo : public Effect
{
private:
	float m_delay;      /* Delay time in seconds */
	float m_feedback;   /* Feedback amount */
	float m_mix;        /* Wet/dry mix */
	bool m_resetBuffer; /* Whether to reset the delay buffer */

public:
	Echo(std::shared_ptr<ISound> sound, float delay, float feedback, float mix, bool resetBuffer = true);

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
