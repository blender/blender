/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_SequencerHandle.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SEQUENCERHANDLE_H__
#define __AUD_SEQUENCERHANDLE_H__

#include "AUD_SequencerEntry.h"
#include "AUD_IHandle.h"
#include "AUD_I3DHandle.h"

class AUD_ReadDevice;

/**
 * Represents a playing sequenced entry.
 */
class AUD_SequencerHandle
{
private:
	/// The entry this handle belongs to.
	AUD_Reference<AUD_SequencerEntry> m_entry;

	/// The handle in the read device.
	AUD_Reference<AUD_IHandle> m_handle;

	/// The 3D handle in the read device.
	AUD_Reference<AUD_I3DHandle> m_3dhandle;

	/// The last read status from the entry.
	int m_status;

	/// The last position status from the entry.
	int m_pos_status;

	/// The last sound status from the entry.
	int m_sound_status;

	/// The read device this handle is played on.
	AUD_ReadDevice& m_device;

public:
	/**
	 * Creates a new sequenced handle.
	 * \param entry The entry this handle plays.
	 * \param device The read device to play on.
	 */
	AUD_SequencerHandle(AUD_Reference<AUD_SequencerEntry> entry, AUD_ReadDevice& device);

	/**
	 * Destroys the handle.
	 */
	~AUD_SequencerHandle();

	/**
	 * Compares whether this handle is playing the same entry as supplied.
	 * \param entry The entry to compare to.
	 * \return Whether the entries ID is smaller, equal or bigger.
	 */
	int compare(AUD_Reference<AUD_SequencerEntry> entry) const;

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
	 */
	void seek(float position);
};

#endif //__AUD_SEQUENCERHANDLE_H__
