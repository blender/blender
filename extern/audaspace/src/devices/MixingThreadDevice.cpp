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

#include "devices/MixingThreadDevice.h"

AUD_NAMESPACE_BEGIN

void MixingThreadDevice::updateRingBuffer()
{
	unsigned int samplesize = AUD_DEVICE_SAMPLE_SIZE(m_specs);

	std::unique_lock<std::mutex> lock(m_mixingLock);

	while(m_valid)
	{
		{
			std::lock_guard<ILockable> device_lock(*this);

			preMixingWork(m_playback);

			if(m_playback)
			{
				size_t size = m_ringBuffer.getWriteSize();

				size_t sample_count = size / samplesize;

				while(sample_count > 0)
				{
					size = sample_count * samplesize;

					mix(reinterpret_cast<data_t*>(m_mixingBuffer.getBuffer()), sample_count);

					m_ringBuffer.write(reinterpret_cast<data_t*>(m_mixingBuffer.getBuffer()), size);

					sample_count = m_ringBuffer.getWriteSize() / samplesize;
				}
			}
		}

		m_mixingCondition.wait(lock);
	}
}

void MixingThreadDevice::startMixingThread(size_t buffersize)
{
	m_mixingBuffer.resize(buffersize);
	m_ringBuffer.resize(buffersize);

	m_valid = true;

	m_mixingThread = std::thread(&MixingThreadDevice::updateRingBuffer, this);
}

void MixingThreadDevice::notifyMixingThread()
{
	m_mixingCondition.notify_all();
}

void MixingThreadDevice::playing(bool playing)
{
	std::lock_guard<ILockable> lock(*this);

	m_playback = playing;

	if(playing)
		notifyMixingThread();
}

void MixingThreadDevice::preMixingWork(bool playing)
{
}

MixingThreadDevice::MixingThreadDevice()
{
}

void aud::MixingThreadDevice::stopMixingThread()
{
	{
		std::unique_lock<std::mutex> lock(m_mixingLock);
		m_valid = false;
	}

	m_mixingCondition.notify_all();

	m_mixingThread.join();
}

AUD_NAMESPACE_END
