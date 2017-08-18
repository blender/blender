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

#ifdef JACK_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file JackSynchronizer.h
 * @ingroup plugin
 * The JackSynchronizer class.
 */

#include "devices/ISynchronizer.h"

AUD_NAMESPACE_BEGIN

class JackDevice;

/**
 * This class is a Synchronizer implementation using JACK Transport.
 */
class AUD_PLUGIN_API JackSynchronizer : public ISynchronizer
{
private:
	/// The device that is being synchronized.
	JackDevice* m_device;

public:
	/**
	 * Creates a new JackSynchronizer.
	 * @param device The device that should be synchronized.
	 */
	JackSynchronizer(JackDevice* device);

	virtual void seek(std::shared_ptr<IHandle> handle, float time);
	virtual float getPosition(std::shared_ptr<IHandle> handle);
	virtual void play();
	virtual void stop();
	virtual void setSyncCallback(syncFunction function, void* data);
	virtual int isPlaying();
};

AUD_NAMESPACE_END
