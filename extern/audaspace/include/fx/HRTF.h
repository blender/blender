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
* @file HRTF.h
* @ingroup fx
* The HRTF class.
*/

#include "util/StreamBuffer.h"
#include "util/FFTPlan.h"
#include "ImpulseResponse.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <utility>

AUD_NAMESPACE_BEGIN

/**
* This class represents a complete set of HRTFs.
*/
class AUD_API HRTF
{
private:
	/**
	* An unordered map of unordered maps containing the ImpulseResponse objects of the HRTFs.
	*/
	std::unordered_map<float, std::unordered_map<float, std::shared_ptr<ImpulseResponse>>> m_hrtfs;

	/**
	* The FFTPlan used to create the ImpulseResponses.
	*/
	std::shared_ptr<FFTPlan> m_plan;

	/**
	* The specifications of the HRTFs.
	*/
	Specs m_specs;

	/**
	* True if the HRTF object is empty.
	*/
	bool m_empty;

	// delete copy constructor and operator=
	HRTF(const HRTF&) = delete;
	HRTF& operator=(const HRTF&) = delete;

public:
	/**
	* Creates a new empty HRTF object that will instance it own FFTPlan with default size.
	*/
	HRTF();

	/**
	* Creates a new empty HRTF object.
	* \param plan A shared pointer to a FFT plan used to transform the impulse responses added.
	*/
	HRTF(std::shared_ptr<FFTPlan> plan);

	/**
	* Adds a new HRTF to the class.
	* \param impulseResponse A shared pointer to an StreamBuffer with the HRTF.
	* \param azimuth The azimuth angle of the HRTF. Interval [0,360).
	* \param elevation The elevation angle of the HRTF.
	* \return True if the impulse response was added successfully, false otherwise (the specs weren't correct).
	*/
	bool addImpulseResponse(std::shared_ptr<StreamBuffer> impulseResponse, float azimuth, float elevation);

	/**
	* Retrieves a pair of HRTFs for a certain azimuth and elevation. If no exact match is found, the closest ones will be chosen (the elevation has priority over the azimuth).
	* \param[in,out] azimuth The desired azimuth angle. If no exact match is found, the value of azimuth will represent the actual azimuth elevation of the chosen HRTF. Interval [0,360)
	* \param[in,out] elevation The desired elevation angle. If no exact match is found, the value of elevation will represent the actual elevation angle of the chosen HRTF.
	* \return A pair of shared pointers to ImpulseResponse objects containing the HRTFs for the left (first element) and right (second element) ears.
	*/
	std::pair<std::shared_ptr<ImpulseResponse>, std::shared_ptr<ImpulseResponse>> getImpulseResponse(float &azimuth, float &elevation);

	/**
	* Retrieves the specs shared by all the HRTFs.
	* \return The shared specs of all the HRTFs.
	*/
	Specs getSpecs();

	/**
	* Retrieves the state of the HRTF object.
	* \return True if it is empty, false otherwise.
	*/
	bool isEmpty();
};

AUD_NAMESPACE_END
