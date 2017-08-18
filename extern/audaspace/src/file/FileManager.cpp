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

#include "file/FileManager.h"
#include "file/IFileInput.h"
#include "file/IFileOutput.h"
#include "Exception.h"

AUD_NAMESPACE_BEGIN

std::list<std::shared_ptr<IFileInput>>& FileManager::inputs()
{
	static std::list<std::shared_ptr<IFileInput>> inputs;
	return inputs;
}

std::list<std::shared_ptr<IFileOutput>>& FileManager::outputs()
{
	static std::list<std::shared_ptr<IFileOutput>> outputs;
	return outputs;
}

void FileManager::registerInput(std::shared_ptr<IFileInput> input)
{
	inputs().push_back(input);
}

void FileManager::registerOutput(std::shared_ptr<aud::IFileOutput> output)
{
	outputs().push_back(output);
}

std::shared_ptr<IReader> FileManager::createReader(std::string filename)
{
	for(std::shared_ptr<IFileInput> input : inputs())
	{
		try
		{
			return input->createReader(filename);
		}
		catch(Exception&) {}
	}

	AUD_THROW(FileException, "The file couldn't be read with any installed file reader.");
}

std::shared_ptr<IReader> FileManager::createReader(std::shared_ptr<Buffer> buffer)
{
	for(std::shared_ptr<IFileInput> input : inputs())
	{
		try
		{
			return input->createReader(buffer);
		}
		catch(Exception&) {}
	}

	AUD_THROW(FileException, "The file couldn't be read with any installed file reader.");
}

std::shared_ptr<IWriter> FileManager::createWriter(std::string filename, DeviceSpecs specs, Container format, Codec codec, unsigned int bitrate)
{
	for(std::shared_ptr<IFileOutput> output : outputs())
	{
		try
		{
			return output->createWriter(filename, specs, format, codec, bitrate);
		}
		catch(Exception&) {}
	}

	AUD_THROW(FileException, "The file couldn't be written with any installed writer.");
}

AUD_NAMESPACE_END
