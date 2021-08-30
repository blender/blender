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
 * @file FileInfo.h
 * @ingroup file
 * The FileInfo data structures.
 */

#include "respec/Specification.h"

AUD_NAMESPACE_BEGIN

/// Specification of a sound source.
struct StreamInfo
{
	/// Start time in seconds.
	double start;

	/// Duration in seconds. May be estimated or 0 if unknown.
	double duration;

	/// Audio data parameters.
	DeviceSpecs specs;
};

AUD_NAMESPACE_END
