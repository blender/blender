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
 * @file StreamBuffer.h
 * @ingroup util
 * The StreamBuffer class.
 */

#include "ISound.h"
#include "respec/Specification.h"

AUD_NAMESPACE_BEGIN

class Buffer;

/**
 * This sound creates a buffer out of a reader. This way normally streamed
 * sound sources can be loaded into memory for buffered playback.
 */
class AUD_API StreamBuffer : public ISound
{
private:
	/**
	 * The buffer that holds the audio data.
	 */
	std::shared_ptr<Buffer> m_buffer;

	/**
	 * The specification of the samples.
	 */
	Specs m_specs;

	// delete copy constructor and operator=
	StreamBuffer(const StreamBuffer&) = delete;
	StreamBuffer& operator=(const StreamBuffer&) = delete;

public:
	/**
	 * Creates the sound and reads the reader created by the sound supplied
	 * to the buffer.
	 * \param sound The sound that creates the reader for buffering.
	 * \exception Exception Thrown if the reader cannot be created.
	 */
	StreamBuffer(std::shared_ptr<ISound> sound);

	/**
	 * Creates the sound from an preexisting buffer.
	 * \param buffer The buffer to stream from.
	 * \param specs The specification of the data in the buffer.
	 * \exception Exception Thrown if the reader cannot be created.
	 */
	StreamBuffer(std::shared_ptr<Buffer> buffer, Specs specs);

	/**
	 * Returns the buffer to be streamed.
	 * @return The buffer to stream.
	 */
	std::shared_ptr<Buffer> getBuffer();

	/**
	 * Returns the specification of the buffer.
	 * @return The specification of the buffer.
	 */
	Specs getSpecs();

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
