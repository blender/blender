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

/** \file audaspace/intern/AUD_SequencerFactory.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_SequencerFactory.h"
#include "AUD_SequencerReader.h"

typedef std::list<AUD_SequencerReader*>::iterator AUD_ReaderIterator;

AUD_SequencerFactory::AUD_SequencerFactory(AUD_Specs specs, bool muted,
										   void* data,
										   AUD_volumeFunction volume) :
	m_specs(specs),
	m_muted(muted),
	m_data(data),
	m_volume(volume)
{
}

AUD_SequencerFactory::~AUD_SequencerFactory()
{
	AUD_SequencerReader* reader;
	AUD_SequencerEntry* entry;

	while(!m_readers.empty())
	{
		reader = m_readers.front();
		m_readers.pop_front();
		reader->destroy();
	}

	while(!m_entries.empty())
	{
		entry = m_entries.front();
		m_entries.pop_front();
		delete entry;
	}
}

void AUD_SequencerFactory::mute(bool muted)
{
	m_muted = muted;
}

bool AUD_SequencerFactory::getMute() const
{
	return m_muted;
}

AUD_IReader* AUD_SequencerFactory::newReader()
{
	AUD_SequencerReader* reader = new AUD_SequencerReader(this, m_entries,
														  m_specs, m_data,
														  m_volume);
	m_readers.push_front(reader);

	return reader;
}

AUD_SequencerEntry* AUD_SequencerFactory::add(AUD_IFactory** sound, float begin, float end, float skip, void* data)
{
	AUD_SequencerEntry* entry = new AUD_SequencerEntry;
	entry->sound = sound;
	entry->begin = begin;
	entry->skip = skip;
	entry->end = end;
	entry->muted = false;
	entry->data = data;

	m_entries.push_front(entry);

	for(AUD_ReaderIterator i = m_readers.begin(); i != m_readers.end(); i++)
		(*i)->add(entry);

	return entry;
}

void AUD_SequencerFactory::remove(AUD_SequencerEntry* entry)
{
	for(AUD_ReaderIterator i = m_readers.begin(); i != m_readers.end(); i++)
		(*i)->remove(entry);

	m_entries.remove(entry);

	delete entry;
}

void AUD_SequencerFactory::move(AUD_SequencerEntry* entry, float begin, float end, float skip)
{
	entry->begin = begin;
	entry->skip = skip;
	entry->end = end;
}

void AUD_SequencerFactory::mute(AUD_SequencerEntry* entry, bool mute)
{
	entry->muted = mute;
}

AUD_IReader* AUD_SequencerFactory::createReader() const
{
	return const_cast<AUD_SequencerFactory*>(this)->newReader();
}

void AUD_SequencerFactory::removeReader(AUD_SequencerReader* reader)
{
	m_readers.remove(reader);
}
