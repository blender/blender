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
 * @file IFileOutput.h
 * @ingroup file
 * The IFileOutput interface.
 */

#include "file/IWriter.h"
#include "respec/Specification.h"

#include <memory>
#include <string>

AUD_NAMESPACE_BEGIN

/**
 * @interface IFileOutput
 * The IFileOutput interface represents a file output plugin that can write files.
 */
class AUD_API IFileOutput
{
public:
	/**
	 * Creates a new file writer.
	 * \param filename The path to the file to be written.
	 * \param specs The file's audio specification.
	 * \param format The file's container format.
	 * \param codec The codec used for encoding the audio data.
	 * \param bitrate The bitrate for encoding.
	 * \exception Exception Thrown if the file specified cannot be written.
	 */
	virtual std::shared_ptr<IWriter> createWriter(std::string filename, DeviceSpecs specs, Container format, Codec codec, unsigned int bitrate)=0;
};

AUD_NAMESPACE_END
