
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
	std::unique_lock<std::mutex> lock(m_delayed_close_mutex);

	m_immediate_close_condition.wait_until(lock, m_playback_stopped_time + m_device_close_delay);

	m_delayed_close_running = false;

	if(m_playing)
		return;

	close();
	m_device_opened = false;
}

void OpenCloseDevice::closeNow()
{
	if(m_delayed_close_thread.joinable())
	{
		m_immediate_close_condition.notify_all();
		m_delayed_close_thread.join();
	}
}

void OpenCloseDevice::playing(bool playing)
{
	std::lock_guard<std::mutex> lock(m_delayed_close_mutex);

	if(m_playing != playing)
	{
		m_playing = playing;
		if(playing)
		{
			if(!m_device_opened)
			{
				open();
				m_device_opened = true;
			}

			start();
		}
		else
		{
			stop();

			m_playback_stopped_time = std::chrono::steady_clock::now();

			if(m_device_opened && !m_delayed_close_running)
			{
				if(m_delayed_close_thread.joinable())
					m_delayed_close_thread.join();

				m_delayed_close_running = true;
				m_delayed_close_thread = std::thread(&OpenCloseDevice::closeAfterDelay, this);
			}
		}
	}
}
AUD_NAMESPACE_END
