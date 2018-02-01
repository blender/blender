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
 * @file IWriter.h
 * @ingroup file
 * Defines the IWriter interface as well as Container and Codec types.
 */

#include "respec/Specification.h"

AUD_NAMESPACE_BEGIN

/// Container formats for writers.
enum Container
{
	CONTAINER_INVALID = 0,
	CONTAINER_AC3,
	CONTAINER_FLAC,
	CONTAINER_MATROSKA,
	CONTAINER_MP2,
	CONTAINER_MP3,
	CONTAINER_OGG,
	CONTAINER_WAV
};

/// Audio codecs for writers.
enum Codec
{
	CODEC_INVALID = 0,
	CODEC_AAC,
	CODEC_AC3,
	CODEC_FLAC,
	CODEC_MP2,
	CODEC_MP3,
	CODEC_PCM,
	CODEC_VORBIS,
	CODEC_OPUS
};

/**
 * @interface IWriter
 * This class represents a sound sink where audio data can be written to.
 */
class AUD_API IWriter
{
public:
	/**
	 * Destroys the writer.
	 */
	virtual ~IWriter() {}

	/**
	 * Returns how many samples have been written so far.
	 * \return The writing position as sample count. May be negative if unknown.
	 */
	virtual int getPosition() const=0;

	/**
	 * Returns the specification of the audio data being written into the sink.
	 * \return The DeviceSpecs structure.
	 * \note Regardless of the format the input still has to be float!
	 */
	virtual DeviceSpecs getSpecs() const=0;

	/**
	 * Request to write the next length samples out into the sink.
	 * \param length The count of samples to write.
	 * \param buffer The pointer to the buffer containing the data.
	 */
	virtual void write(unsigned int length, sample_t* buffer)=0;
};

AUD_NAMESPACE_END
