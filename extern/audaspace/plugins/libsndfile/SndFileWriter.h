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

#ifdef LIBSNDFILE_PLUGIN
#define AUD_BUILD_PLUGIN
#endif

/**
 * @file SndFileWriter.h
 * @ingroup plugin
 * The SndFileWriter class.
 */

#include "file/IWriter.h"

#include <string>
#include <sndfile.h>

AUD_NAMESPACE_BEGIN

/**
 * This class writes a sound file via libsndfile.
 */
class AUD_PLUGIN_API SndFileWriter : public IWriter
{
private:
	/**
	 * The current position in samples.
	 */
	int m_position;

	/**
	 * The specification of the audio data.
	 */
	DeviceSpecs m_specs;

	/**
	 * The sndfile.
	 */
	SNDFILE* m_sndfile;

	// delete copy constructor and operator=
	SndFileWriter(const SndFileWriter&) = delete;
	SndFileWriter& operator=(const SndFileWriter&) = delete;

public:
	/**
	 * Creates a new writer.
	 * \param filename The path to the file to be read.
	 * \param specs The file's audio specification.
	 * \param format The file's container format.
	 * \param codec The codec used for encoding the audio data.
	 * \param bitrate The bitrate for encoding.
	 * \exception Exception Thrown if the file specified cannot be written
	 *                          with libsndfile.
	 */
	SndFileWriter(std::string filename, DeviceSpecs specs, Container format, Codec codec, unsigned int bitrate);

	/**
	 * Destroys the writer and closes the file.
	 */
	virtual ~SndFileWriter();

	virtual int getPosition() const;
	virtual DeviceSpecs getSpecs() const;
	virtual void write(unsigned int length, sample_t* buffer);
};

AUD_NAMESPACE_END
