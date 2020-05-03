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

#include "sequence/SequenceReader.h"
#include "sequence/SequenceData.h"
#include "Exception.h"
#include "SequenceHandle.h"

#include <algorithm>
#include <mutex>
#include <cmath>

AUD_NAMESPACE_BEGIN

SequenceReader::SequenceReader(std::shared_ptr<SequenceData> sequence, bool quality) :
	m_position(0), m_device(sequence->m_specs), m_sequence(sequence), m_status(0), m_entry_status(0)
{
	m_device.setQuality(quality);
}

SequenceReader::~SequenceReader()
{
}

bool SequenceReader::isSeekable() const
{
	return true;
}

void SequenceReader::seek(int position)
{
	if(position < 0)
		return;

	m_position = position;

	for(auto& handle : m_handles)
	{
		handle->seek(position / (double)m_sequence->m_specs.rate);
	}
}

int SequenceReader::getLength() const
{
	return -1;
}

int SequenceReader::getPosition() const
{
	return m_position;
}

Specs SequenceReader::getSpecs() const
{
	return m_sequence->m_specs;
}

void SequenceReader::read(int& length, bool& eos, sample_t* buffer)
{
	std::lock_guard<ILockable> lock(*m_sequence);

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
		std::list<std::shared_ptr<SequenceHandle> > handles;

		auto hit = m_handles.begin();
		auto eit = m_sequence->m_entries.begin();

		int result;
		std::shared_ptr<SequenceHandle> handle;

		while(hit != m_handles.end() && eit != m_sequence->m_entries.end())
		{
			handle = *hit;
			std::shared_ptr<SequenceEntry> entry = *eit;

			result = handle->compare(entry);

			if(result < 0)
			{
				try
				{
					handle = std::shared_ptr<SequenceHandle>(new SequenceHandle(entry, m_device));
					handles.push_back(handle);
				}
				catch(Exception&)
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
				handle = std::shared_ptr<SequenceHandle>(new SequenceHandle(*eit, m_device));
				handles.push_back(handle);
			}
			catch(Exception&)
			{
			}
			eit++;
		}

		m_handles = handles;

		m_entry_status = m_sequence->m_entry_status;
	}

	Specs specs = m_sequence->m_specs;
	int pos = 0;
	double time = double(m_position) / double(specs.rate);
	float volume, frame;
	int len, cfra;
	Vector3 v, v2;
	Quaternion q;

	while(pos < length)
	{
		frame = time * m_sequence->m_fps;
		cfra = int(std::floor(frame));

		len = int(std::ceil((cfra + 1) / m_sequence->m_fps * specs.rate)) - m_position;
		len = std::min(length - pos, len);
		len = std::max(len, 1);

		for(auto& handle : m_handles)
		{
			handle->update(time, frame, m_sequence->m_fps);
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
		time += double(len) / double(specs.rate);
	}

	m_position += length;

	eos = false;
}

AUD_NAMESPACE_END
