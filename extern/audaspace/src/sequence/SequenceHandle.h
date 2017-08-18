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

#include "Audaspace.h"

#include <memory>

AUD_NAMESPACE_BEGIN

class ReadDevice;
class IHandle;
class I3DHandle;
class SequenceEntry;

/**
 * Represents a playing sequenced entry.
 */
class SequenceHandle
{
private:
	/// The entry this handle belongs to.
	std::shared_ptr<SequenceEntry> m_entry;

	/// The handle in the read device.
	std::shared_ptr<IHandle> m_handle;

	/// The 3D handle in the read device.
	std::shared_ptr<I3DHandle> m_3dhandle;

	/// Whether the sound is playable.
	bool m_valid;

	/// The last read status from the entry.
	int m_status;

	/// The last position status from the entry.
	int m_pos_status;

	/// The last sound status from the entry.
	int m_sound_status;

	/// The read device this handle is played on.
	ReadDevice& m_device;

	// delete copy constructor and operator=
	SequenceHandle(const SequenceHandle&) = delete;
	SequenceHandle& operator=(const SequenceHandle&) = delete;

	/**
	 * Starts playing back the handle.
	 */
	void start();

	/**
	 * Updates the handle state depending on position.
	 * \param position Current playback position in seconds.
	 * \return Whether the handle is valid.
	 */
	bool updatePosition(float position);

public:
	/**
	 * Creates a new sequenced handle.
	 * \param entry The entry this handle plays.
	 * \param device The read device to play on.
	 */
	SequenceHandle(std::shared_ptr<SequenceEntry> entry, ReadDevice& device);

	/**
	 * Destroys the handle.
	 */
	~SequenceHandle();

	/**
	 * Compares whether this handle is playing the same entry as supplied.
	 * \param entry The entry to compare to.
	 * \return Whether the entries ID is smaller, equal or bigger.
	 */
	int compare(std::shared_ptr<SequenceEntry> entry) const;

	/**
	 * Stops playing back the handle.
	 */
	void stop();

	/**
	 * Updates the handle for playback.
	 * \param position The current time during playback.
	 * \param frame The current frame during playback.
	 * \param fps The animation frames per second.
	 */
	void update(float position, float frame, float fps);

	/**
	 * Seeks the handle to a specific time position.
	 * \param position The time to seek to.
	 * \return Whether the handle is valid.
	 */
	bool seek(float position);
};

AUD_NAMESPACE_END
