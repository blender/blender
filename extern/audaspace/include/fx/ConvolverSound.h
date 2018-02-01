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
* @file ConvolverSound.h
* @ingroup fx
* The ConvolverSound class.
*/

#include "ISound.h"
#include "ImpulseResponse.h"
#include "util/ThreadPool.h"
#include "util/FFTPlan.h"

#include <memory>
#include <vector>

AUD_NAMESPACE_BEGIN

/**
* This class represents a sound that can be modified depending on a given impulse response.
*/
class AUD_API ConvolverSound : public ISound
{
private:
	/**
	* A pointer to the imput sound.
	*/
	std::shared_ptr<ISound> m_sound;

	/**
	* A pointer to the impulse response.
	*/
	std::shared_ptr<ImpulseResponse> m_impulseResponse;

	/**
	* A shared ptr to a thread pool.
	*/
	std::shared_ptr<ThreadPool> m_threadPool;

	/**
	* A shared ponter to an FFT plan.
	*/
	std::shared_ptr<FFTPlan> m_plan;

	// delete copy constructor and operator=
	ConvolverSound(const ConvolverSound&) = delete;
	ConvolverSound& operator=(const ConvolverSound&) = delete;

public:
	/**
	* Creates a new ConvolverSound.
	* \param sound The sound that will be convolved.
	* \param impulseResponse The impulse response sound.
	* \param threadPool A shared pointer to a ThreadPool object with 1 or more threads.
	* \param plan A shared pointer to a FFTPlan object that will be used for convolution.
	* \warning The same FFTPlan object must be used to construct both this and the ImpulseResponse object provided.
	*/
	ConvolverSound(std::shared_ptr<ISound> sound, std::shared_ptr<ImpulseResponse> impulseResponse, std::shared_ptr<ThreadPool> threadPool, std::shared_ptr<FFTPlan> plan);

	/**
	* Creates a new ConvolverSound. A default FFT plan will be created.
	* \param sound The sound that will be convolved.
	* \param impulseResponse The impulse response sound.
	* \param threadPool A shared pointer to a ThreadPool object with 1 or more threads.
	* \warning To use this constructor no FFTPlan object must have been provided to the inpulseResponse.
	*/
	ConvolverSound(std::shared_ptr<ISound> sound, std::shared_ptr<ImpulseResponse> impulseResponse, std::shared_ptr<ThreadPool> threadPool);

	virtual std::shared_ptr<IReader> createReader();

	/**
	* Retrieves the impulse response sound being used.
	* \return A shared pointer to the current impulse response being used.
	*/
	std::shared_ptr<ImpulseResponse> getImpulseResponse();

	/**
	* Changes the inpulse response used for convolution, it'll only affect newly created readers.
	* \param impulseResponse A shared pointer to the new impulse response sound.
	*/
	void setImpulseResponse(std::shared_ptr<ImpulseResponse> impulseResponse);
};

AUD_NAMESPACE_END