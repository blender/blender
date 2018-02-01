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
 * @file ILockable.h
 * @ingroup util
 * The ILockable interface.
 */

#include "Audaspace.h"

AUD_NAMESPACE_BEGIN

/**
 * @interface ILockable
 * This class provides an interface for lockable objects.
 */
class AUD_API ILockable
{
public:
	/**
	 * Locks the object.
	 */
	virtual void lock()=0;
	/**
	 * Unlocks the previously locked object.
	 */
	virtual void unlock()=0;
};

AUD_NAMESPACE_END
