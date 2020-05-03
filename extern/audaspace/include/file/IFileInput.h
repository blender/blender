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
 * @file IFileInput.h
 * @ingroup file
 * The IFileInput interface.
 */

#include "Audaspace.h"

#include <memory>
#include <string>

AUD_NAMESPACE_BEGIN

class IReader;
class Buffer;

/**
 * @interface IFileInput
 * The IFileInput interface represents a file input plugin that can create file
 * input readers from filenames or buffers.
 */
class AUD_API IFileInput
{
public:
	/**
	 * Destroys the file input.
	 */
	virtual ~IFileInput() {}

	/**
	 * Creates a reader for a file to be read.
	 * \param filename Path to the file to be read.
	 * \return The reader that reads the file.
	 * \exception Exception Thrown if the file specified cannot be read.
	 */
	virtual std::shared_ptr<IReader> createReader(std::string filename)=0;

	/**
	 * Creates a reader for a file to be read from memory.
	 * \param buffer The in-memory file buffer.
	 * \return The reader that reads the file.
	 * \exception Exception Thrown if the file specified cannot be read.
	 */
	virtual std::shared_ptr<IReader> createReader(std::shared_ptr<Buffer> buffer)=0;
};

AUD_NAMESPACE_END
