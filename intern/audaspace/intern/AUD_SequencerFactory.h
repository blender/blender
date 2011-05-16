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

/** \file audaspace/intern/AUD_SequencerFactory.h
 *  \ingroup audaspaceintern
 */


#ifndef AUD_SEQUENCERFACTORY
#define AUD_SEQUENCERFACTORY

#include "AUD_IFactory.h"

#include <list>

typedef float (*AUD_volumeFunction)(void*, void*, float);

struct AUD_SequencerEntry
{
	AUD_IFactory** sound;
	float begin;
	float end;
	float skip;
	bool muted;
	void* data;
};

class AUD_SequencerReader;

/**
 * This factory creates a resampling reader that does simple linear resampling.
 */
class AUD_SequencerFactory : public AUD_IFactory
{
private:
	/**
	 * The target specification.
	 */
	AUD_Specs m_specs;

	std::list<AUD_SequencerEntry*> m_entries;
	std::list<AUD_SequencerReader*> m_readers;
	bool m_muted;
	void* m_data;
	AUD_volumeFunction m_volume;

	AUD_IReader* newReader();

	// hide copy constructor and operator=
	AUD_SequencerFactory(const AUD_SequencerFactory&);
	AUD_SequencerFactory& operator=(const AUD_SequencerFactory&);

public:
	AUD_SequencerFactory(AUD_Specs specs, bool muted, void* data, AUD_volumeFunction volume);
	~AUD_SequencerFactory();

	void mute(bool muted);
	bool getMute() const;
	AUD_SequencerEntry* add(AUD_IFactory** sound, float begin, float end, float skip, void* data);
	void remove(AUD_SequencerEntry* entry);
	void move(AUD_SequencerEntry* entry, float begin, float end, float skip);
	void mute(AUD_SequencerEntry* entry, bool mute);

	virtual AUD_IReader* createReader() const;

	void removeReader(AUD_SequencerReader* reader);
};

#endif //AUD_SEQUENCERFACTORY
