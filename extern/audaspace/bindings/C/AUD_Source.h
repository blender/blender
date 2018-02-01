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
* Creates a new Source object.
* \param azimuth The azimuth angle.
* \param elevation The elevation angle.
* \param elevation The distance value. [0,1]
* \return The new Source object.
*/
extern AUD_API AUD_Source* AUD_Source_create(float azimuth, float elevation, float distance);

/**
* Deletes a Source object.
* \param source The Source object to be deleted.
*/
extern AUD_API void AUD_Source_free(AUD_Source* source);

/**
* Retrieves the azimuth angle of a Source object.
* \param source The Source object.
* \return The azimuth angle.
*/
extern AUD_API float AUD_Source_getAzimuth(AUD_Source* source);

/**
* Retrieves the elevation angle oa a Source object.
* \param source The Source object.
* \return The elevation angle.
*/
extern AUD_API float AUD_Source_getElevation(AUD_Source* source);

/**
* Retrieves the distance of a Source object. [0,1]
* \param source The Source object.
* \return The distance.
*/
extern AUD_API float AUD_Source_getDistance(AUD_Source* distance);

/**
* Changes the azimuth angle of a Source object.
* \param source The Source object.
* \param azimuth The azimuth angle.
*/
extern AUD_API void AUD_Source_setAzimuth(AUD_Source* source, float azimuth);

/**
* Changes the elevation angle of a Source object.
* \param source The Source object.
* \param elevation The elevation angle.
*/
extern AUD_API void AUD_Source_setElevation(AUD_Source* source, float elevation);

/**
* Changes the distance of a Source object. [0,1]
* \param source The Source object.
* \param distance The distance.
*/
extern AUD_API void AUD_Source_setDistance(AUD_Source* source, float distance);

#ifdef __cplusplus
}
#endif
