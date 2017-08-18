/*******************************************************************************
* Copyright 2009-2015 Juan Francisco Crespo Galán
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
#include "AUD_HRTF.h"

extern AUD_API AUD_HRTF* AUD_HRTF_create()
{
	try
	{
		return new AUD_HRTF(new HRTF());
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

extern AUD_API void AUD_HRTF_free(AUD_HRTF* hrtfs)
{
	assert(hrtfs);
	delete hrtfs;
}

extern AUD_API void AUD_HRTF_addImpulseResponseFromSound(AUD_HRTF* hrtfs, AUD_Sound* sound, float azimuth, float elevation)
{
	assert(hrtfs);
	assert(sound);

	(*hrtfs)->addImpulseResponse(std::make_shared<StreamBuffer>(*sound), azimuth, elevation);
}