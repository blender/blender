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

#ifndef AUD_SEQUENCERREADER
#define AUD_SEQUENCERREADER

#include "AUD_IReader.h"
#include "AUD_SequencerFactory.h"
#include "AUD_Mixer.h"

class AUD_Buffer;

struct AUD_SequencerStrip
{
	AUD_IFactory* old_sound;
	AUD_IReader* reader;
	AUD_SequencerEntry* entry;
};

/**
 * This resampling reader uses libsamplerate for resampling.
 */
class AUD_SequencerReader : public AUD_IReader
{
private:
	/**
	 * The current position.
	 */
	int m_position;

	/**
	 * The sound output buffer.
	 */
	AUD_Buffer *m_buffer;

	/**
	 * The target specification.
	 */
	AUD_Mixer m_mixer;

	/**
	 * Saves the SequencerFactory the reader belongs to.
	 */
	AUD_SequencerFactory* m_factory;

	std::list<AUD_SequencerStrip*> m_strips;

	void* m_data;
	AUD_volumeFunction m_volume;

public:
	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param specs The target specification.
	 * \exception AUD_Exception Thrown if the reader is NULL.
	 */
	AUD_SequencerReader(AUD_SequencerFactory* factory, std::list<AUD_SequencerEntry*> &entries, AUD_Specs specs, void* data, AUD_volumeFunction volume);

	/**
	 * Destroys the reader.
	 */
	~AUD_SequencerReader();

	void destroy();

	void add(AUD_SequencerEntry* entry);
	void remove(AUD_SequencerEntry* entry);

	virtual bool isSeekable();
	virtual void seek(int position);
	virtual int getLength();
	virtual int getPosition();
	virtual AUD_Specs getSpecs();
	virtual AUD_ReaderType getType();
	virtual bool notify(AUD_Message &message);
	virtual void read(int & length, sample_t* & buffer);
};

#endif //AUD_SEQUENCERREADER
