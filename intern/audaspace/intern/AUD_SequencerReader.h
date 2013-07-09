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

/** \file audaspace/intern/AUD_SequencerReader.h
 *  \ingroup audaspaceintern
 */


#ifndef __AUD_SEQUENCERREADER_H__
#define __AUD_SEQUENCERREADER_H__

#include "AUD_IReader.h"
#include "AUD_ReadDevice.h"
#include "AUD_Sequencer.h"
#include "AUD_SequencerHandle.h"

/**
 * This reader plays back sequenced entries.
 */
class AUD_SequencerReader : public AUD_IReader
{
private:
	/**
	 * The current position.
	 */
	int m_position;

	/**
	 * The read device used to mix the sounds correctly.
	 */
	AUD_ReadDevice m_device;

	/**
	 * Saves the sequence the reader belongs to.
	 */
	boost::shared_ptr<AUD_Sequencer> m_sequence;

	/**
	 * The list of playback handles for the entries.
	 */
	std::list<boost::shared_ptr<AUD_SequencerHandle> > m_handles;

	/**
	 * Last status read from the sequence.
	 */
	int m_status;

	/**
	 * Last entry status read from the sequence.
	 */
	int m_entry_status;

	// hide copy constructor and operator=
	AUD_SequencerReader(const AUD_SequencerReader&);
	AUD_SequencerReader& operator=(const AUD_SequencerReader&);

public:
	/**
	 * Creates a resampling reader.
	 * \param reader The reader to mix.
	 * \param specs The target specification.
	 */
	AUD_SequencerReader(boost::shared_ptr<AUD_Sequencer> sequence, bool quality = false);

	/**
	 * Destroys the reader.
	 */
	~AUD_SequencerReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual AUD_Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

#endif //__AUD_SEQUENCERREADER_H__
