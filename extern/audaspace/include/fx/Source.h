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

#pragma once

/**
* @file Source.h
* @ingroup fx
* The Source class.
*/

#include "Audaspace.h"

#include <atomic>

AUD_NAMESPACE_BEGIN

/**
* This class stores the azimuth and elevation angles of a sound and allows to change them dynamically.
* The azimuth angle goes clockwise. For a sound source situated at the right of the listener the azimuth angle is 90.
*/
class AUD_API Source
{
private:
	/**
	* Azimuth value.
	*/
	std::atomic<float> m_azimuth;

	/**
	* Elevation value.
	*/
	std::atomic<float> m_elevation;

	/**
	* Distance value. Between 0 and 1.
	*/
	std::atomic<float> m_distance;

	// delete copy constructor and operator=
	Source(const Source&) = delete;
	Source& operator=(const Source&) = delete;

public:
	/**
	* Creates a Source instance with an initial value.
	* \param azimuth The value of the azimuth.
	* \param elevation The value of the elevation.
	* \param distance The distance from the listener. Max distance is 1, min distance is 0.
	*/
	Source(float azimuth, float elevation, float distance = 0.0);

	/**
	* Retrieves the current azimuth value.
	* \return The current azimuth.
	*/
	float getAzimuth();

	/**
	* Retrieves the current elevation value.
	* \return The current elevation.
	*/
	float getElevation();

	/**
	* Retrieves the current distance value.
	* \return The current distance.
	*/
	float getDistance();

	/**
	* Retrieves the current volume value based on the distance.
	* \return The current volume based on the Distance.
	*/
	float getVolume();

	/**
	* Changes the azimuth value.
	* \param azimuth The new value for the azimuth.
	*/
	void setAzimuth(float azimuth);

	/**
	* Changes the elevation value.
	* \param elevation The new value for the elevation.
	*/
	void setElevation(float elevation);

	/**
	* Changes the distance value.
	* \param distance The new value for the distance. Max distance is 1, min distance is 0.
	*/
	void setDistance(float distance);
};

AUD_NAMESPACE_END