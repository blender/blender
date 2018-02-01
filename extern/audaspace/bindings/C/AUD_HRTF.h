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

#pragma once

#include "AUD_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* Creates a new HRTF object.
* \return The new HRTF object.
*/
extern AUD_API AUD_HRTF* AUD_HRTF_create();

/**
* Deletes a HRTF object.
* \param hrtfs The HRTF object to be deleted.
*/
extern AUD_API void AUD_HRTF_free(AUD_HRTF* hrtfs);

/**
* Adds a new impulse response to an HRTF object.
* \param hrtfs The HRTF object.
* \param sound A Sound object representing an HRTF.
* \param azimuth The azimuth angle of the HRTF.
* \param elevation The elevation angle of the HRTF.
*/
extern AUD_API void AUD_HRTF_addImpulseResponseFromSound(AUD_HRTF* hrtfs, AUD_Sound* sound, float azimuth, float elevation);

#ifdef __cplusplus
}
#endif