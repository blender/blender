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
 * @file FileWriter.h
 * @ingroup file
 * The FileWriter class.
 */

#include "respec/Specification.h"
#include "file/IWriter.h"

#include <string>
#include <vector>
#include <memory>

AUD_NAMESPACE_BEGIN

class IReader;

/**
 * The FileWriter class is able to create IWriter classes as well as write readers to them.
 */
class AUD_API FileWriter
{
private:
	// hide default constructor, copy constructor and operator=
	FileWriter() = delete;
	FileWriter(const FileWriter&) = delete;
	FileWriter& operator=(const FileWriter&) = delete;

public:
	/**
	 * Creates a new IWriter.
	 * \param filename The file to write to.
	 * \param specs The file's audio specification.
	 * \param format The file's container format.
	 * \param codec The codec used for encoding the audio data.
	 * \param bitrate The bitrate for encoding.
	 * \return The writer to write data to.
	 */
	static std::shared_ptr<IWriter> createWriter(const std::string &filename, DeviceSpecs specs, Container format, Codec codec, unsigned int bitrate);

	/**
	 * Writes a reader to a writer.
	 * \param reader The reader to read from.
	 * \param writer The writer to write to.
	 * \param length How many samples should be transferred.
	 * \param buffersize How many samples should be transferred at once.
	 */
	static void writeReader(std::shared_ptr<IReader> reader, std::shared_ptr<IWriter> writer, unsigned int length, unsigned int buffersize, void(*callback)(float, void*) = nullptr, void* data = nullptr);

	/**
	 * Writes a reader to several writers.
	 * \param reader The reader to read from.
	 * \param writers The writers to write to.
	 * \param length How many samples should be transferred.
	 * \param buffersize How many samples should be transferred at once.
	 */
	static void writeReader(std::shared_ptr<IReader> reader, std::vector<std::shared_ptr<IWriter> >& writers, unsigned int length, unsigned int buffersize, void(*callback)(float, void*) = nullptr, void* data = nullptr);
};

AUD_NAMESPACE_END
