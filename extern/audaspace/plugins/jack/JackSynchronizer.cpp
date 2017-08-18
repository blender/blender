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

#include "JackSynchronizer.h"

#include "JackDevice.h"

AUD_NAMESPACE_BEGIN

JackSynchronizer::JackSynchronizer(JackDevice* device) :
	m_device(device)
{
}

void JackSynchronizer::seek(std::shared_ptr<IHandle> handle, float time)
{
	m_device->seekPlayback(time);
}

float JackSynchronizer::getPosition(std::shared_ptr<IHandle> handle)
{
	return m_device->getPlaybackPosition();
}

void JackSynchronizer::play()
{
	m_device->startPlayback();
}

void JackSynchronizer::stop()
{
	m_device->stopPlayback();
}

void JackSynchronizer::setSyncCallback(ISynchronizer::syncFunction function, void* data)
{
	m_device->setSyncCallback(function, data);
}

int JackSynchronizer::isPlaying()
{
	return m_device->doesPlayback();
}

AUD_NAMESPACE_END
