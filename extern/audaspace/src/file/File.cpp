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

#include "file/File.h"
#include "file/FileManager.h"
#include "util/Buffer.h"
#include "Exception.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

File::File(const std::string &filename, int stream) :
	m_filename(filename), m_stream(stream)
{
}

File::File(const data_t* buffer, int size, int stream) :
	m_buffer(new Buffer(size)), m_stream(stream)
{
	std::memcpy(m_buffer->getBuffer(), buffer, size);
}

std::vector<StreamInfo> File::queryStreams()
{
	if(m_buffer.get())
		return FileManager::queryStreams(m_buffer);
	else
		return FileManager::queryStreams(m_filename);
}

std::shared_ptr<IReader> File::createReader()
{
	if(m_buffer.get())
		return FileManager::createReader(m_buffer, m_stream);
	else
		return FileManager::createReader(m_filename, m_stream);
}

AUD_NAMESPACE_END
