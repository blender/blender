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

#pragma once

/**
 * @file SequenceReader.h
 * @ingroup sequence
 * The SequenceReader class.
 */

#include "IReader.h"
#include "devices/ReadDevice.h"

AUD_NAMESPACE_BEGIN

class SequenceHandle;
class SequenceData;

/**
 * This reader plays back sequenced entries.
 */
class AUD_API SequenceReader : public IReader
{
private:
	/**
	 * The current position.
	 */
	int m_position;

	/**
	 * The read device used to mix the sounds correctly.
	 */
	ReadDevice m_device;

	/**
	 * Saves the sequence the reader belongs to.
	 */
	std::shared_ptr<SequenceData> m_sequence;

	/**
	 * The list of playback handles for the entries.
	 */
	std::list<std::shared_ptr<SequenceHandle> > m_handles;

	/**
	 * Last status read from the sequence.
	 */
	int m_status;

	/**
	 * Last entry status read from the sequence.
	 */
	int m_entry_status;

	// delete copy constructor and operator=
	SequenceReader(const SequenceReader&) = delete;
	SequenceReader& operator=(const SequenceReader&) = delete;

public:
	/**
	 * Creates a resampling reader.
	 * \param sequence The sequence data.
	 * \param quality Resampling quality vs performance option.
	 */
	SequenceReader(std::shared_ptr<SequenceData> sequence, ResampleQuality quality = ResampleQuality::FASTEST);

	/**
	 * Destroys the reader.
	 */
	~SequenceReader();

	virtual bool isSeekable() const;
	virtual void seek(int position);
	virtual int getLength() const;
	virtual int getPosition() const;
	virtual Specs getSpecs() const;
	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
