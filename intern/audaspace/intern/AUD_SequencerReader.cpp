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

/** \file audaspace/intern/AUD_SequencerReader.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_SequencerReader.h"
#include "AUD_MutexLock.h"

typedef std::list<boost::shared_ptr<AUD_SequencerHandle> >::iterator AUD_HandleIterator;
typedef std::list<boost::shared_ptr<AUD_SequencerEntry> >::iterator AUD_EntryIterator;

AUD_SequencerReader::AUD_SequencerReader(boost::shared_ptr<AUD_Sequencer> sequence, bool quality) :
	m_position(0), m_device(sequence->m_specs), m_sequence(sequence), m_status(0), m_entry_status(0)
{
	m_device.setQuality(quality);
}

AUD_SequencerReader::~AUD_SequencerReader()
{
}

bool AUD_SequencerReader::isSeekable() const
{
	return true;
}

void AUD_SequencerReader::seek(int position)
{
	if(position < 0)
		return;

	m_position = position;

	for(AUD_HandleIterator it = m_handles.begin(); it != m_handles.end(); it++)
	{
		(*it)->seek(position / m_sequence->m_specs.rate);
	}
}

int AUD_SequencerReader::getLength() const
{
	return -1;
}

int AUD_SequencerReader::getPosition() const
{
	return m_position;
}

AUD_Specs AUD_SequencerReader::getSpecs() const
{
	return m_sequence->m_specs;
}

void AUD_SequencerReader::read(int& length, bool& eos, sample_t* buffer)
{
	AUD_MutexLock lock(*m_sequence);

	if(m_sequence->m_status != m_status)
	{
		m_device.changeSpecs(m_sequence->m_specs);
		m_device.setSpeedOfSound(m_sequence->m_speed_of_sound);
		m_device.setDistanceModel(m_sequence->m_distance_model);
		m_device.setDopplerFactor(m_sequence->m_doppler_factor);

		m_status = m_sequence->m_status;
	}

	if(m_sequence->m_entry_status != m_entry_status)
	{
		std::list<boost::shared_ptr<AUD_SequencerHandle> > handles;

		AUD_HandleIterator hit = m_handles.begin();
		AUD_EntryIterator  eit = m_sequence->m_entries.begin();

		int result;
		boost::shared_ptr<AUD_SequencerHandle> handle;

		while(hit != m_handles.end() && eit != m_sequence->m_entries.end())
		{
			handle = *hit;
			boost::shared_ptr<AUD_SequencerEntry> entry = *eit;

			result = handle->compare(entry);

			if(result < 0)
			{
				try
				{
					handle = boost::shared_ptr<AUD_SequencerHandle>(new AUD_SequencerHandle(entry, m_device));
					handles.push_back(handle);
				}
				catch(AUD_Exception&)
				{
				}
				eit++;
			}
			else if(result == 0)
			{
				handles.push_back(handle);
				hit++;
				eit++;
			}
			else
			{
				handle->stop();
				hit++;
			}
		}

		while(hit != m_handles.end())
		{
			(*hit)->stop();
			hit++;
		}

		while(eit != m_sequence->m_entries.end())
		{
			try
			{
				handle = boost::shared_ptr<AUD_SequencerHandle>(new AUD_SequencerHandle(*eit, m_device));
				handles.push_back(handle);
			}
			catch(AUD_Exception&)
			{
			}
			eit++;
		}

		m_handles = handles;

		m_entry_status = m_sequence->m_entry_status;
	}

	AUD_Specs specs = m_sequence->m_specs;
	int pos = 0;
	float time = float(m_position) / float(specs.rate);
	float volume, frame;
	int len, cfra;
	AUD_Vector3 v, v2;
	AUD_Quaternion q;


	while(pos < length)
	{
		frame = time * m_sequence->m_fps;
		cfra = int(floor(frame));

		len = int(ceil((cfra + 1) / m_sequence->m_fps * specs.rate)) - m_position;
		len = AUD_MIN(length - pos, len);
		len = AUD_MAX(len, 1);

		for(AUD_HandleIterator it = m_handles.begin(); it != m_handles.end(); it++)
		{
			(*it)->update(time, frame, m_sequence->m_fps);
		}

		m_sequence->m_volume.read(frame, &volume);
		if(m_sequence->m_muted)
			volume = 0.0f;
		m_device.setVolume(volume);

		m_sequence->m_orientation.read(frame, q.get());
		m_device.setListenerOrientation(q);
		m_sequence->m_location.read(frame, v.get());
		m_device.setListenerLocation(v);
		m_sequence->m_location.read(frame + 1, v2.get());
		v2 -= v;
		m_device.setListenerVelocity(v2 * m_sequence->m_fps);

		m_device.read(reinterpret_cast<data_t*>(buffer + specs.channels * pos), len);

		pos += len;
		time += float(len) / float(specs.rate);
	}

	m_position += length;

	eos = false;
}
