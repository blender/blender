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

#include "util/Buffer.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>

#define ALIGNMENT 32
#define ALIGN(a) (a + ALIGNMENT - ((long long)a & (ALIGNMENT-1)))

AUD_NAMESPACE_BEGIN

Buffer::Buffer(long long size)
{
	m_size = size;
	m_buffer = (data_t*) std::malloc(size + ALIGNMENT);
}

Buffer::~Buffer()
{
	std::free(m_buffer);
}

const sample_t* Buffer::getBuffer() const
{
	return (sample_t*) ALIGN(m_buffer);
}

sample_t* Buffer::getBuffer()
{
	return (sample_t*) ALIGN(m_buffer);
}

long long Buffer::getSize() const
{
	return m_size;
}

void Buffer::resize(long long size, bool keep)
{
	if(keep)
	{
		data_t* buffer = (data_t*) std::malloc(size + ALIGNMENT);

		std::memcpy(ALIGN(buffer), ALIGN(m_buffer), std::min(size, m_size));

		std::free(m_buffer);
		m_buffer = buffer;
	}
	else
		m_buffer = (data_t*) std::realloc(m_buffer, size + ALIGNMENT);

	m_size = size;
}

void Buffer::assureSize(long long size, bool keep)
{
	if(m_size < size)
		resize(size, keep);
}

AUD_NAMESPACE_END
