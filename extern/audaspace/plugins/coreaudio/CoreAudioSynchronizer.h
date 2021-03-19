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

#pragma once

#ifdef COREAUDIO_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file CoreAudioSynchronizer.h
 * @ingroup plugin
 * The CoreAudioSynchronizer class.
 */

#include "devices/ISynchronizer.h"

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/CoreAudioClock.h>

AUD_NAMESPACE_BEGIN

/**
 * This class is a Synchronizer implementation using a CoreAudio clock.
 */
class AUD_PLUGIN_API CoreAudioSynchronizer : public ISynchronizer
{
private:
    /// The CoreAudio clock referene.
    CAClockRef m_clock_ref;

	/// Whether the clock is currently playing.
	bool m_playing;

public:
	/**
	 * Creates a new CoreAudioSynchronizer.
	 * @param device The device that should be synchronized.
	 */
    CoreAudioSynchronizer(AudioUnit& audio_unit);
    virtual ~CoreAudioSynchronizer();

	virtual void seek(std::shared_ptr<IHandle> handle, double time);
	virtual double getPosition(std::shared_ptr<IHandle> handle);
	virtual void play();
	virtual void stop();
	virtual void setSyncCallback(syncFunction function, void* data);
	virtual int isPlaying();
};

AUD_NAMESPACE_END
