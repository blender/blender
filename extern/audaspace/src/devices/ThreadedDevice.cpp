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

#include "devices/ThreadedDevice.h"

#include <mutex>

AUD_NAMESPACE_BEGIN

void ThreadedDevice::start()
{
	std::lock_guard<ILockable> lock(*this);

	// thread is still running, we can abort stopping it
	if(m_stop)
		m_stop = false;

	// thread is not running, let's start it
	else if(!m_playing)
	{
		if(m_thread.joinable())
			m_thread.join();

		m_playing = true;

		m_thread = std::thread(&ThreadedDevice::runMixingThread, this);
	}
}

void ThreadedDevice::playing(bool playing)
{
	if((!m_playing || m_stop) && playing)
		start();
	else
		m_stop = true;
}

ThreadedDevice::ThreadedDevice() :
	m_playing(false),
	m_stop(false)
{
}

void aud::ThreadedDevice::stopMixingThread()
{
	stopAll();

	if(m_thread.joinable())
		m_thread.join();
}

AUD_NAMESPACE_END
