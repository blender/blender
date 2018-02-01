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

/**
 * @file DefaultSynchronizer.h
 * @ingroup devices
 * The DefaultSynchronizer class.
 */

#include "ISynchronizer.h"

AUD_NAMESPACE_BEGIN

/**
 * This class is a default ISynchronizer implementation that actually does no
 * synchronization and is intended for devices that don't support it.
 */
class AUD_API DefaultSynchronizer : public ISynchronizer
{
public:
	virtual void seek(std::shared_ptr<IHandle> handle, float time);
	virtual float getPosition(std::shared_ptr<IHandle> handle);
	virtual void play();
	virtual void stop();
	virtual void setSyncCallback(syncFunction function, void* data);
	virtual int isPlaying();
};

AUD_NAMESPACE_END
