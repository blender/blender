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
 * @file AnimateableProperty.h
 * @ingroup sequence
 * Defines the AnimateableProperty class as well as existing property types.
 */

#include "util/Buffer.h"
#include "util/ILockable.h"

#include <mutex>
#include <list>

AUD_NAMESPACE_BEGIN

/// Possible animatable properties for Sequencer Factories and Entries.
enum AnimateablePropertyType
{
	AP_VOLUME,
	AP_PANNING,
	AP_PITCH,
	AP_LOCATION,
	AP_ORIENTATION,
	AP_TIME_STRETCH,
	AP_PITCH_SCALE
};

/**
 * This class saves animation data for float properties.
 */
class AUD_API AnimateableProperty : private Buffer
{
private:
	struct Unknown {
		int start;
		int end;

		Unknown(int start, int end) :
			start(start), end(end) {}
	};

	/// The count of floats for a single property.
	const int m_count;

	/// Whether the property is animated or not.
	bool m_isAnimated;

	/// The mutex for locking.
	std::recursive_mutex m_mutex;

	/// The list of unknown buffer areas.
	std::list<Unknown> m_unknown;

	// delete copy constructor and operator=
	AnimateableProperty(const AnimateableProperty&) = delete;
	AnimateableProperty& operator=(const AnimateableProperty&) = delete;

	void AUD_LOCAL updateUnknownCache(int start, int end);

public:
	/**
	 * Creates a new animateable property.
	 * \param count The count of floats for a single property.
	 */
	AnimateableProperty(int count = 1);

	/**
	 * Creates a new animateable property.
	 * \param count The count of floats for a single property.
	 * \param value The value that the property should get initialized with.
	 *   All count floats will be initialized to the same value.
	 */
	AnimateableProperty(int count, float value);

	/**
	 * Destroys the animateable property.
	 */
	~AnimateableProperty();

	/**
	 * Returns the count of floats for a single property.
	 * \return The count of floats stored per frame.
	 */
	int getCount() const;

	/**
	 * Writes the properties value and marks it non-animated.
	 * \param data The new value.
	 */
	void write(const float* data);

	/**
	 * Writes the properties value and marks it animated.
	 * \param data The new value.
	 * \param position The position in the animation in frames.
	 * \param count The count of frames to write.
	 */
	void write(const float* data, int position, int count);

	/**
	 * Fills the properties frame range with constant value and marks it animated.
	 * \param data The new value.
	 * \param position_start The start position in the animation in frames.
	 * \param position_end The end position in the animation in frames.
	 */
	void writeConstantRange(const float* data, int position_start, int position_end);

	/**
	 * Reads the properties value.
	 * \param position The position in the animation in frames.
	 * \param[out] out Where to write the value to.
	 */
	void read(float position, float* out);

	/**
	 * Reads the property's value at the specified position, assuming there is exactly one value.
	 * \param position The position in the animation in frames.
	 * \return The value at the position.
	 */
	float readSingle(float position);

	/**
	 * Returns whether the property is animated.
	 * \return Whether the property is animated.
	 */
	bool isAnimated() const;

	/**
	 * Returns this object cast as a Buffer.
	 */
	const Buffer& getBuffer();
};

AUD_NAMESPACE_END
