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
	void* m_data;
	AUD_volumeFunction m_volume;

public:
	AUD_SequencerFactory(AUD_Specs specs, void* data, AUD_volumeFunction volume);
	~AUD_SequencerFactory();

	AUD_SequencerEntry* add(AUD_IFactory** sound, float begin, float end, float skip, void* data);
	void remove(AUD_SequencerEntry* entry);
	void move(AUD_SequencerEntry* entry, float begin, float end, float skip);
	void mute(AUD_SequencerEntry* entry, bool mute);

	virtual AUD_IReader* createReader();

	void removeReader(AUD_SequencerReader* reader);
};

#endif //AUD_SEQUENCERFACTORY
