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
 * @file JOSResample.h
 * @ingroup respec
 * The JOSResample class.
 */

#include "respec/SpecsChanger.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound creates a resampling reader that does Julius O. Smith's resampling algorithm.
 */
class AUD_API JOSResample : public SpecsChanger
{
private:
	// delete copy constructor and operator=
	JOSResample(const JOSResample&) = delete;
	JOSResample& operator=(const JOSResample&) = delete;

public:
	/**
	 * Creates a new sound.
	 * \param sound The input sound.
	 * \param specs The target specifications.
	 */
	JOSResample(std::shared_ptr<ISound> sound, DeviceSpecs specs);

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
