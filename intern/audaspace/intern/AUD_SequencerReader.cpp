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
#include "AUD_DefaultMixer.h"

#include <math.h>

typedef std::list<AUD_SequencerStrip*>::iterator AUD_StripIterator;
typedef std::list<AUD_SequencerEntry*>::iterator AUD_EntryIterator;

AUD_SequencerReader::AUD_SequencerReader(AUD_SequencerFactory* factory,
			std::list<AUD_SequencerEntry*> &entries, AUD_Specs specs,
			void* data, AUD_volumeFunction volume)
{
	AUD_DeviceSpecs dspecs;
	dspecs.specs = specs;
	dspecs.format = AUD_FORMAT_FLOAT32;

	m_mixer = new AUD_DefaultMixer(dspecs);
	m_factory = factory;
	m_data = data;
	m_volume = volume;

	AUD_SequencerStrip* strip;

	for(AUD_EntryIterator i = entries.begin(); i != entries.end(); i++)
	{
		strip = new AUD_SequencerStrip;
		strip->entry = *i;
		strip->old_sound = NULL;

		if(strip->old_sound)
			strip->reader = m_mixer->prepare(strip->old_sound->createReader());
		else
			strip->reader = NULL;

		m_strips.push_front(strip);
	}

	m_position = 0;
}

AUD_SequencerReader::~AUD_SequencerReader()
{
	if(m_factory != NULL)
		m_factory->removeReader(this);

	AUD_SequencerStrip* strip;

	while(!m_strips.empty())
	{
		strip = m_strips.front();
		m_strips.pop_front();
		if(strip->reader)
		{
			delete strip->reader;
		}
		delete strip;
	}

	delete m_mixer;
}

void AUD_SequencerReader::destroy()
{
	m_factory = NULL;
	AUD_SequencerStrip* strip;

	while(!m_strips.empty())
	{
		strip = m_strips.front();
		m_strips.pop_front();
		delete strip;
	}
}

void AUD_SequencerReader::add(AUD_SequencerEntry* entry)
{
	AUD_SequencerStrip* strip = new AUD_SequencerStrip;
	strip->entry = entry;

	if(*strip->entry->sound)
	{
		strip->old_sound = *strip->entry->sound;
		strip->reader = m_mixer->prepare(strip->old_sound->createReader());
	}
	else
	{
		strip->reader = NULL;
		strip->old_sound = NULL;
	}
	m_strips.push_front(strip);
}

void AUD_SequencerReader::remove(AUD_SequencerEntry* entry)
{
	AUD_SequencerStrip* strip;
	for(AUD_StripIterator i = m_strips.begin(); i != m_strips.end(); i++)
	{
		strip = *i;
		if(strip->entry == entry)
		{
			i++;
			if(strip->reader)
			{
				delete strip->reader;
			}
			m_strips.remove(strip);
			delete strip;
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

void AUD_SequencerReader::read(int & length, sample_t* & buffer)
{
	AUD_DeviceSpecs specs = m_mixer->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);
	int rate = specs.rate;

	int size = length * samplesize;

	int start, end, current, skip, len;
	AUD_SequencerStrip* strip;
	sample_t* buf;

	if(m_buffer.getSize() < size)
		m_buffer.resize(size);
	buffer = m_buffer.getBuffer();

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
					if(strip->reader)
						delete strip->reader;

					if(strip->old_sound)
					{
						try
						{
							strip->reader = m_mixer->prepare(strip->old_sound->createReader());
						}
						catch(AUD_Exception)
						{
							strip->reader = NULL;
						}
					}
					else
						strip->reader = NULL;
				}

				if(strip->reader)
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
							strip->reader->read(len, buf);
							m_mixer->add(buf, skip, len, m_volume(m_data, strip->entry->data, (float)m_position / (float)rate));
						}
					}
				}
			}
		}
	}

	m_mixer->superpose((data_t*)buffer, length, 1.0f);

	m_position += length;
}
