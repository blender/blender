
/*******************************************************************************
 * Copyright 2009-2024 Jörg Müller
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

#include "devices/OpenCloseDevice.h"

AUD_NAMESPACE_BEGIN

void OpenCloseDevice::closeAfterDelay()
{
	for(;;)
	{
		std::this_thread::sleep_for(m_device_close_delay / 10);
		if(m_playing || m_playback_stopped_time.time_since_epoch().count() == 0)
			m_playback_stopped_time = std::chrono::steady_clock::now();
		if(std::chrono::steady_clock::now() < m_playback_stopped_time + m_device_close_delay)
			continue;

		break;
	}
	close();
	m_delayed_close_finished = true;
	m_device_opened = false;
}

void OpenCloseDevice::playing(bool playing)
{
	if(m_playing != playing)
	{
		m_playing = playing;
		if(playing)
		{
			if(!m_device_opened)
				open();
			m_device_opened = true;
			start();
		}
		else
		{
			stop();
			m_playback_stopped_time = std::chrono::steady_clock::now();
			if(m_delayed_close_thread.joinable() && m_delayed_close_finished)
			{
				m_delayed_close_thread.join();
				m_delayed_close_finished = false;
			}

			if(m_device_opened && !m_delayed_close_thread.joinable())
				m_delayed_close_thread = std::thread(&OpenCloseDevice::closeAfterDelay, this);
		}
	}
}
AUD_NAMESPACE_END
