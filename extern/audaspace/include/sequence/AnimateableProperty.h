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

#include "util/ILockable.h"

#include <deque>
#include <memory>
#include <mutex>
#include <list>
#include <vector>

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
class AUD_API AnimateableProperty : public ILockable
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

	/// The position of the first frame in the buffer, the frames before it have its value.
	int m_bufferPosition;

	/// The frames of a buffer that fits into a block, in an allocation of at most a block.
	std::vector<float> m_frames;

	/**
	 * The frames of a longer buffer, in blocks of the same size, so that adding or discarding frames
	 * at either end never copies the other frames.
	 */
	std::unique_ptr<std::deque<std::unique_ptr<float[]>>> m_blocks;

	/// The index of the first frame of the buffer in the first block.
	int m_blockOffset;

	/// The number of frames in the buffer.
	int m_length;

	/// The mutex for locking.
	std::recursive_mutex m_mutex;

	/// The list of unknown buffer areas.
	std::list<Unknown> m_unknown;

	// delete copy constructor and operator=
	AnimateableProperty(const AnimateableProperty&) = delete;
	AnimateableProperty& operator=(const AnimateableProperty&) = delete;

	void AUD_LOCAL updateUnknownCache(int start, int end);
	void AUD_LOCAL updateUnknownAfterWrite(int pos, int position, int count);
	void AUD_LOCAL extendBufferStart(int position);

	/// Returns the values of frames in the buffer and the number of frames stored after them.
	float* AUD_LOCAL getFrames(int position, long long& frames);

	/// Returns the values of a frame in the buffer.
	float* AUD_LOCAL getFrame(int position);

	/// Adds frames before and after the buffer, their values are unset.
	void AUD_LOCAL growBuffer(int before, int after);

	/// Adds frames after the buffer so that it ends before a position.
	void AUD_LOCAL growBufferEnd(int position);

	/// Removes frames from the start of the buffer.
	void AUD_LOCAL discardBufferStart(int count);

	/// Writes one value to the frames of the buffer from a start position to before an end position.
	void AUD_LOCAL fillFrames(int start, int end, const float* data);

	/// Writes the values of consecutive frames of the buffer.
	void AUD_LOCAL writeFrames(int position, int count, const float* data);

	/**
	 * Animates the property and extends the buffer for writing frames from a position.
	 * \param position The position of the first written frame.
	 * \return The position after the known frames, the frames in between are unknown.
	 */
	int AUD_LOCAL prepareWrite(int position);

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
	virtual ~AnimateableProperty();

	/**
	 * Returns the count of floats for a single property.
	 * \return The count of floats stored per frame.
	 */
	int getCount() const;

	/**
	 * Locks the property.
	 */
	virtual void lock();

	/**
	 * Unlocks the previously locked property.
	 */
	virtual void unlock();

	/**
	 * Clears animation data after the first value, preserves that value and marks the property
	 * non-animated.
	 */
	void reset();

	/**
	 * Writes the properties value and marks it non-animated.
	 * \param data The new value.
	 */
	void write(const float* data);

	/**
	 * Writes the properties value and marks it animated.
	 * Frames before the buffer have the value of its first frame, see getBufferPosition(). Writing
	 * before them keeps this value for the two frames before the written ones, so a property that
	 * was not animated keeps its value before the written frames without storing it for each frame.
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
	 * Discards the animation data before a position to free its memory, when the frames before it
	 * are not read anymore. Reading before the position then returns the value at the position,
	 * writing before it makes the frames in between unknown again, see write(). Positions at or
	 * before the first frame of the buffer, like INT_MIN, discard nothing.
	 * \param position The position in the animation in frames.
	 */
	void discardBefore(int position);

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
	 * Returns the number of unknown frames after a position that writing frames up to the position
	 * fills with the value of the last written frame, see write(). Unknown frames are frames that
	 * were not written between written ones, they have the value of the frame before them.
	 * \param position The position of the last written frame in the animation in frames, at or
	 *        after getBufferPosition(). Writing the frames before the buffer fills its unknown first
	 *        frames too, at most the two that writing before the buffer keeps.
	 * \return The number of unknown frames after the position if the frame at the position is
	 *         unknown too, otherwise 0.
	 */
	int getUnknownFramesAfter(int position);

	/**
	 * Returns whether the property is animated.
	 * \return Whether the property is animated.
	 */
	bool isAnimated() const;

	/**
	 * Returns the position of the first frame in the buffer, see discardBefore().
	 * \return The position in the animation in frames.
	 */
	int getBufferPosition() const;

	/**
	 * Returns the number of frames in the buffer, which starts at getBufferPosition(). The frames
	 * after it have the value of its last frame.
	 * \return The number of frames, at least one.
	 */
	int getBufferLength() const;

	/**
	 * Reads the values of consecutive frames, like read() at their positions.
	 * \param position The position of the first frame in the animation in frames.
	 * \param count The number of frames to read.
	 * \param[out] out Where to write the values of the frames to.
	 */
	void readFrames(int position, int count, float* out);
};

AUD_NAMESPACE_END
