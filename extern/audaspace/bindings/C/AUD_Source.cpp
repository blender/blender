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
#include "AUD_Source.h"

extern AUD_API AUD_Source* AUD_Source_create(float azimuth, float elevation, float distance)
{
	try
	{
		return new AUD_Source(new Source(azimuth, elevation, distance));
	}
	catch(Exception&)
	{
		return nullptr;
	}
}

extern AUD_API void AUD_Source_free(AUD_Source* source)
{
	assert(source);
	delete source;
}

extern AUD_API float AUD_Source_getAzimuth(AUD_Source* source)
{
	assert(source);

	return (*source)->getAzimuth();
}

extern AUD_API float AUD_Source_getElevation(AUD_Source* source)
{
	assert(source);

	return (*source)->getElevation();
}

extern AUD_API float AUD_Source_getDistance(AUD_Source* source)
{
	assert(source);

	return (*source)->getDistance();
}

extern AUD_API void AUD_Source_setAzimuth(AUD_Source* source, float azimuth)
{
	assert(source);

	(*source)->setAzimuth(azimuth);
}

extern AUD_API void AUD_Source_setElevation(AUD_Source* source, float elevation)
{
	assert(source);

	(*source)->setElevation(elevation);
}

extern AUD_API void AUD_Source_setDistance(AUD_Source* source, float distance)
{
	assert(source);

	(*source)->setDistance(distance);
}