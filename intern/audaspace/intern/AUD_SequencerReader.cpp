/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#include "AUD_SequencerReader.h"
#include "AUD_Buffer.h"

#include <math.h>

typedef std::list<AUD_SequencerStrip*>::iterator AUD_StripIterator;
typedef std::list<AUD_SequencerEntry*>::iterator AUD_EntryIterator;

AUD_SequencerReader::AUD_SequencerReader(AUD_SequencerFactory* factory, std::list<AUD_SequencerEntry*> &entries, AUD_Specs specs, void* data, AUD_volumeFunction volume)
{
	AUD_DeviceSpecs dspecs;
	dspecs.specs = specs;
	dspecs.format = AUD_FORMAT_FLOAT32;

	m_mixer.setSpecs(dspecs);
	m_factory = factory;
	m_data = data;
	m_volume = volume;

	AUD_SequencerStrip* strip;

	for(AUD_EntryIterator i = entries.begin(); i != entries.end(); i++)
	{
		strip = new AUD_SequencerStrip;  AUD_NEW("seqstrip")
		strip->entry = *i;
		strip->old_sound = NULL;

		if(strip->old_sound)
			strip->reader = m_mixer.prepare(strip->old_sound->createReader());
		else
			strip->reader = NULL;

		m_strips.push_front(strip);
	}

	m_position = 0;
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
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
			delete strip->reader; AUD_DELETE("reader")
		}
		delete strip; AUD_DELETE("seqstrip")
	}

	delete m_buffer; AUD_DELETE("buffer")
}

void AUD_SequencerReader::destroy()
{
	m_factory = NULL;
	AUD_SequencerStrip* strip;

	while(!m_strips.empty())
	{
		strip = m_strips.front();
		m_strips.pop_front();
		delete strip; AUD_DELETE("seqstrip")
	}
}

void AUD_SequencerReader::add(AUD_SequencerEntry* entry)
{
	AUD_SequencerStrip* strip = new AUD_SequencerStrip; AUD_NEW("seqstrip")
	strip->entry = entry;

	if(*strip->entry->sound)
	{
		strip->old_sound = *strip->entry->sound;
		strip->reader = m_mixer.prepare(strip->old_sound->createReader());
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
				delete strip->reader; AUD_DELETE("reader")
			}
			m_strips.remove(strip);
			delete strip; AUD_DELETE("seqstrip")
			return;
		}
	}
}

bool AUD_SequencerReader::isSeekable()
{
	return true;
}

void AUD_SequencerReader::seek(int position)
{
	m_position = position;
}

int AUD_SequencerReader::getLength()
{
	return -1;
}

int AUD_SequencerReader::getPosition()
{
	return m_position;
}

AUD_Specs AUD_SequencerReader::getSpecs()
{
	return m_mixer.getSpecs().specs;
}

AUD_ReaderType AUD_SequencerReader::getType()
{
	return AUD_TYPE_STREAM;
}

bool AUD_SequencerReader::notify(AUD_Message &message)
{
	bool result = false;
	AUD_SequencerStrip* strip;

	for(AUD_StripIterator i = m_strips.begin(); i != m_strips.end(); i++)
	{
		strip = *i;
		if(strip->reader)
			result |= (*i)->reader->notify(message);
	}

	return result;
}

void AUD_SequencerReader::read(int & length, sample_t* & buffer)
{
	AUD_DeviceSpecs specs = m_mixer.getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);
	int rate = specs.rate;

	int size = length * samplesize;

	int start, end, current, skip, len;
	AUD_SequencerStrip* strip;
	sample_t* buf;

	if(m_buffer->getSize() < size)
		m_buffer->resize(size);
	buffer = m_buffer->getBuffer();

	for(AUD_StripIterator i = m_strips.begin(); i != m_strips.end(); i++)
	{
		strip = *i;
		if(!strip->entry->muted)
		{
			if(strip->old_sound != *strip->entry->sound)
			{
				strip->old_sound = *strip->entry->sound;
				if(strip->reader)
				{
					delete strip->reader; AUD_DELETE("reader")
				}

				if(strip->old_sound)
					strip->reader = m_mixer.prepare(strip->old_sound->createReader());
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
						m_mixer.add(buf, skip, len, m_volume(m_data, strip->entry->data, (float)m_position / (float)rate));
					}
				}
			}
		}
	}

	m_mixer.superpose((data_t*)buffer, length, 1.0f);

	m_position += length;
}
