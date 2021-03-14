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

#include "CoreAudioSynchronizer.h"

#include "CoreAudioDevice.h"
#include "Exception.h"

AUD_NAMESPACE_BEGIN

CoreAudioSynchronizer::CoreAudioSynchronizer(AudioUnit& audio_unit) :
	m_clock_ref(nullptr),
	m_playing(false)
{
	OSStatus status = CAClockNew(0, &m_clock_ref);

	if(status != noErr)
		AUD_THROW(DeviceException, "Could not create a CoreAudio clock.");

	CAClockTimebase timebase = kCAClockTimebase_AudioOutputUnit;

	status = CAClockSetProperty(m_clock_ref, kCAClockProperty_InternalTimebase, sizeof(timebase), &timebase);

	if(status != noErr)
	{
		CAClockDispose(m_clock_ref);
		AUD_THROW(DeviceException, "Could not create a CoreAudio clock.");
	}

	status = CAClockSetProperty(m_clock_ref, kCAClockProperty_TimebaseSource, sizeof(audio_unit), &audio_unit);

	if(status != noErr)
	{
		CAClockDispose(m_clock_ref);
		AUD_THROW(DeviceException, "Could not create a CoreAudio clock.");
	}

	CAClockSyncMode sync_mode = kCAClockSyncMode_Internal;

	status = CAClockSetProperty(m_clock_ref, kCAClockProperty_SyncMode, sizeof(sync_mode), &sync_mode);

	if(status != noErr)
	{
		CAClockDispose(m_clock_ref);
		AUD_THROW(DeviceException, "Could not create a CoreAudio clock.");
	}
}

CoreAudioSynchronizer::~CoreAudioSynchronizer()
{
	CAClockDispose(m_clock_ref);
}

void CoreAudioSynchronizer::seek(std::shared_ptr<IHandle> handle, double time)
{
	if(m_playing)
		CAClockStop(m_clock_ref);

	CAClockTime clock_time;
	clock_time.format = kCAClockTimeFormat_Seconds;
	clock_time.time.seconds = time;
	CAClockSetCurrentTime(m_clock_ref, &clock_time);

	handle->seek(time);

	if(m_playing)
		CAClockStart(m_clock_ref);
}

double CoreAudioSynchronizer::getPosition(std::shared_ptr<IHandle> handle)
{
	CAClockTime clock_time;

	OSStatus status;

	if(m_playing)
		status = CAClockGetCurrentTime(m_clock_ref, kCAClockTimeFormat_Seconds, &clock_time);
	else
		status = CAClockGetStartTime(m_clock_ref, kCAClockTimeFormat_Seconds, &clock_time);

	if(status != noErr)
		return 0;

	return clock_time.time.seconds;
}

void CoreAudioSynchronizer::play()
{
	if(m_playing)
		return;

	m_playing = true;
	CAClockStart(m_clock_ref);
}

void CoreAudioSynchronizer::stop()
{
	if(!m_playing)
		return;

	m_playing = false;
	CAClockStop(m_clock_ref);
}

void CoreAudioSynchronizer::setSyncCallback(ISynchronizer::syncFunction function, void* data)
{
}

int CoreAudioSynchronizer::isPlaying()
{
	return m_playing;
}

AUD_NAMESPACE_END
