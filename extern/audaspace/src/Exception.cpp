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

#include "Exception.h"

#include <sstream>

AUD_NAMESPACE_BEGIN

Exception::Exception(const Exception& exception) :
	Exception(exception.m_message, exception.m_file, exception.m_line)
{
}

Exception::Exception(const std::string &message, const std::string &file, int line) :
	m_message(message),
	m_file(file),
	m_line(line)
{
}

Exception::~Exception() AUD_NOEXCEPT
{
}

const char* Exception::what() const AUD_NOEXCEPT
{
	return m_message.c_str();
}

std::string Exception::getDebugMessage() const
{
	std::stringstream out;

	out << m_message << " File " << m_file << ":" << m_line;

	return out.str();
}

const std::string& Exception::getMessage() const
{
	return m_message;
}

const std::string& Exception::getFile() const
{
	return m_file;
}

int Exception::getLine() const
{
	return m_line;
}

FileException::FileException(const std::string &message, const std::string &file, int line) :
	Exception(message, file, line)
{
}

FileException::FileException(const FileException& exception) :
	Exception(exception)
{
}

FileException::~FileException() AUD_NOEXCEPT
{
}

DeviceException::DeviceException(const std::string &message, const std::string &file, int line) :
	Exception(message, file, line)
{
}

DeviceException::DeviceException(const DeviceException& exception) :
	Exception(exception)
{
}

DeviceException::~DeviceException() AUD_NOEXCEPT
{
}

StateException::StateException(const std::string &message, const std::string &file, int line) :
	Exception(message, file, line)
{
}

StateException::StateException(const StateException& exception) :
	Exception(exception)
{
}

StateException::~StateException() AUD_NOEXCEPT
{
}

AUD_NAMESPACE_END
