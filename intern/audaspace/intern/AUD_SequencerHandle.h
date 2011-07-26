/*
 * $Id$
 *
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


#ifndef AUD_SEQUENCERHANDLE
#define AUD_SEQUENCERHANDLE

#include "AUD_SequencerEntry.h"
#include "AUD_IHandle.h"
#include "AUD_I3DHandle.h"

class AUD_ReadDevice;

class AUD_SequencerHandle
{
private:
	AUD_Reference<AUD_SequencerEntry> m_entry;
	AUD_Reference<AUD_IHandle> m_handle;
	AUD_Reference<AUD_I3DHandle> m_3dhandle;
	int m_status;
	int m_pos_status;
	int m_sound_status;
	AUD_ReadDevice& m_device;

public:
	AUD_SequencerHandle(AUD_Reference<AUD_SequencerEntry> entry, AUD_ReadDevice& device);
	~AUD_SequencerHandle();
	int compare(AUD_Reference<AUD_SequencerEntry> entry) const;
	void stop();
	void update(float position);
	void seek(float position);
};

#endif //AUD_SEQUENCERHANDLE
