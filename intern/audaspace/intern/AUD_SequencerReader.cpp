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

/** \file audaspace/intern/AUD_SequencerReader.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_SequencerReader.h"
#include "AUD_Mixer.h"

#ifdef WITH_SAMPLERATE
#include "AUD_SRCResampleReader.h"
#else
#include "AUD_LinearResampleReader.h"
#endif
#include "AUD_ChannelMapperReader.h"

#include <math.h>

typedef std::list<AUD_Reference<AUD_SequencerStrip> >::iterator AUD_StripIterator;
typedef std::list<AUD_Reference<AUD_SequencerEntry> >::iterator AUD_EntryIterator;

AUD_SequencerReader::AUD_SequencerReader(AUD_Reference<AUD_SequencerFactory> factory,
			std::list<AUD_Reference<AUD_SequencerEntry> > &entries, AUD_Specs specs,
			void* data, AUD_volumeFunction volume) :
	m_position(0), m_factory(factory), m_data(data), m_volume(volume)
{
	AUD_DeviceSpecs dspecs;
	dspecs.specs = specs;
	dspecs.format = AUD_FORMAT_FLOAT32;

	m_mixer = new AUD_Mixer(dspecs);

	AUD_Reference<AUD_SequencerStrip> strip;

	for(AUD_EntryIterator i = entries.begin(); i != entries.end(); i++)
	{
		strip = new AUD_SequencerStrip;
		strip->entry = *i;
		strip->old_sound = NULL;

		m_strips.push_front(strip);
	}
}

AUD_SequencerReader::~AUD_SequencerReader()
{
	m_factory->removeReader(this);
}

void AUD_SequencerReader::add(AUD_Reference<AUD_SequencerEntry> entry)
{
	AUD_Reference<AUD_SequencerStrip> strip = new AUD_SequencerStrip;
	strip->entry = entry;

	m_strips.push_front(strip);
}

void AUD_SequencerReader::remove(AUD_Reference<AUD_SequencerEntry> entry)
{
	AUD_Reference<AUD_SequencerStrip> strip;
	for(AUD_StripIterator i = m_strips.begin(); i != m_strips.end(); i++)
	{
		strip = *i;
		if(strip->entry == entry)
		{
			i++;
			m_strips.remove(strip);
			return;
		}
	}
}

bool AUD_SequencerReader::isSeekable() const
{
	return true;
}

void AUD_SequencerReader::seek(int position)
{
	m_position = position;
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
	return m_mixer->getSpecs().specs;
}

void AUD_SequencerReader::read(int& length, bool& eos, sample_t* buffer)
{
	AUD_DeviceSpecs specs = m_mixer->getSpecs();
	int rate = specs.rate;

	int start, end, current, skip, len;
	AUD_Reference<AUD_SequencerStrip> strip;
	m_buffer.assureSize(length * AUD_SAMPLE_SIZE(specs));

	m_mixer->clear(length);

	if(!m_factory->getMute())
	{
		for(AUD_StripIterator i = m_strips.begin(); i != m_strips.end(); i++)
		{
			strip = *i;
			if(!strip->entry->muted)
			{
				if(strip->old_sound != *strip->entry->sound)
				{
					strip->old_sound = *strip->entry->sound;

					if(strip->old_sound)
					{
						try
						{
							strip->reader = (*strip->old_sound)->createReader();
							// resample
							#ifdef WITH_SAMPLERATE
								strip->reader = new AUD_SRCResampleReader(strip->reader, m_mixer->getSpecs().specs);
							#else
								strip->reader = new AUD_LinearResampleReader(strip->reader, m_mixer->getSpecs().specs);
							#endif

							// rechannel
							strip->reader = new AUD_ChannelMapperReader(strip->reader, m_mixer->getSpecs().channels);
						}
						catch(AUD_Exception)
						{
							strip->reader = NULL;
						}
					}
					else
						strip->reader = NULL;
				}

				if(!strip->reader.isNull())
				{
					end = floor(strip->entry->end * rate);
					if(m_position < end)
					{
						start = floor(strip->entry->begin * rate);
						if(m_position + length > start)
						{
							current = m_position - start;
							if(current < 0)
							{
								skip = -current;
								current = 0;
							}
							else
								skip = 0;
							current += strip->entry->skip * rate;
							len = length > end - m_position ? end - m_position : length;
							len -= skip;
							if(strip->reader->getPosition() != current)
								strip->reader->seek(current);
							strip->reader->read(len, eos, m_buffer.getBuffer());
							m_mixer->mix(m_buffer.getBuffer(), skip, len, m_volume(m_data, strip->entry->data, (float)m_position / (float)rate));
						}
					}
				}
			}
		}
	}

	m_mixer->read((data_t*)buffer, 1.0f);

	m_position += length;

	eos = false;
}
