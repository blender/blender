/*******************************************************************************
* Copyright 2015-2016 Juan Francisco Crespo Galán
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

#include "Exception.h"

#include <cassert>

using namespace aud;

#define AUD_CAPI_IMPLEMENTATION
#include "AUD_DynamicMusic.h"

AUD_API AUD_DynamicMusic* AUD_DynamicMusic_create(AUD_Device* device)
{
	assert(device);

	try
	{
		return new AUD_DynamicMusic(new DynamicMusic(*device));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

AUD_API void AUD_DynamicMusic_free(AUD_DynamicMusic* player)
{
	assert(player);
	delete player;
}

AUD_API int AUD_DynamicMusic_addScene(AUD_DynamicMusic* player, AUD_Sound* scene)
{
	assert(player);
	assert(scene);

	return (*player)->addScene(*scene);
}

AUD_API int AUD_DynamicMusic_setSecene(AUD_DynamicMusic* player, int scene)
{
	assert(player);

	return (*player)->changeScene(scene);
}

AUD_API int AUD_DynamicMusic_getScene(AUD_DynamicMusic* player)
{
	assert(player);

	return (*player)->getScene();
}

AUD_API int AUD_DynamicMusic_addTransition(AUD_DynamicMusic* player, int ini, int end, AUD_Sound* transition)
{
	assert(player);
	assert(transition);

	return (*player)->addTransition(ini, end, *transition);
}

AUD_API void AUD_DynamicMusic_setFadeTime(AUD_DynamicMusic* player, float seconds)
{
	assert(player);

	(*player)->setFadeTime(seconds);
}

AUD_API float AUD_DynamicMusic_getFadeTime(AUD_DynamicMusic* player)
{
	assert(player);

	return (*player)->getFadeTime();
}

AUD_API int AUD_DynamicMusic_resume(AUD_DynamicMusic* player)
{
	assert(player);

	return (*player)->resume();
}

AUD_API int AUD_DynamicMusic_pause(AUD_DynamicMusic* player)
{
	assert(player);

	return (*player)->pause();
}

AUD_API int AUD_DynamicMusic_seek(AUD_DynamicMusic* player, float position)
{
	assert(player);

	return (*player)->seek(position);
}

AUD_API float AUD_DynamicMusic_getPosition(AUD_DynamicMusic* player)
{
	assert(player);

	return (*player)->getPosition();
}

AUD_API float AUD_DynamicMusic_getVolume(AUD_DynamicMusic* player)
{
	assert(player);

	return (*player)->getVolume();
}

AUD_API int AUD_DynamicMusic_setVolume(AUD_DynamicMusic* player, float volume)
{
	assert(player);

	return (*player)->setVolume(volume);
}

AUD_API AUD_Status AUD_DynamicMusic_getStatus(AUD_DynamicMusic* player)
{
	assert(player);

	return static_cast<AUD_Status>((*player)->getStatus());
}

AUD_API int AUD_DynamicMusic_stop(AUD_DynamicMusic* player)
{
	assert(player);

	return (*player)->stop();
}