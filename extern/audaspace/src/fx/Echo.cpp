#include "fx/Echo.h"

#include "fx/EchoReader.h"

AUD_NAMESPACE_BEGIN

Echo::Echo(std::shared_ptr<ISound> sound, float delay, float feedback, float mix, bool resetBuffer) :
    Effect(sound), m_delay(delay), m_feedback(feedback), m_mix(mix), m_resetBuffer(resetBuffer)
{
}

std::shared_ptr<IReader> Echo::createReader()
{
	return std::make_shared<EchoReader>(getReader(), m_delay, m_feedback, m_mix, m_resetBuffer);
}

AUD_NAMESPACE_END
