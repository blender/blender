/*******************************************************************************
 * Copyright 2009-2021 Jörg Müller
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

#include "util/RingBuffer.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>

#define ALIGNMENT 32
#define ALIGN(a) (a + ALIGNMENT - ((long long)a & (ALIGNMENT-1)))

AUD_NAMESPACE_BEGIN

RingBuffer::RingBuffer(int size) :
	m_buffer(size),
	m_read(0),
	m_write(0)
{
}

const sample_t* RingBuffer::getBuffer() const
{
	return m_buffer.getBuffer();
}

sample_t* RingBuffer::getBuffer()
{
	return m_buffer.getBuffer();
}

int RingBuffer::getSize() const
{
	return m_buffer.getSize();
}

size_t RingBuffer::getReadSize() const
{
	size_t read = m_read;
	size_t write = m_write;

	if(read > write)
		return write + getSize() - read;
	else
		return write - read;
}

size_t RingBuffer::getWriteSize() const
{
	size_t read = m_read;
	size_t write = m_write;

	if(read > write)
		return read - write - 1;
	else
		return read + getSize() - write - 1;
}

size_t RingBuffer::read(data_t* target, size_t size)
{
	size = std::min(size, getReadSize());

	data_t* buffer = reinterpret_cast<data_t*>(m_buffer.getBuffer());

	if(m_read + size > m_buffer.getSize())
	{
		size_t read_first = m_buffer.getSize() - m_read;
		size_t read_second = size - read_first;

		std::memcpy(target, buffer + m_read, read_first);
		std::memcpy(target + read_first, buffer, read_second);

		m_read = read_second;
	}
	else
	{
		std::memcpy(target, buffer + m_read, size);

		m_read += size;
	}

	return size;
}

size_t RingBuffer::write(data_t* source, size_t size)
{
	size = std::min(size, getWriteSize());

	data_t* buffer = reinterpret_cast<data_t*>(m_buffer.getBuffer());

	if(m_write + size > m_buffer.getSize())
	{
		size_t write_first = m_buffer.getSize() - m_write;
		size_t write_second = size - write_first;

		std::memcpy(buffer + m_write, source, write_first);
		std::memcpy(buffer, source + write_first, write_second);

		m_write = write_second;
	}
	else
	{
		std::memcpy(buffer + m_write, source, size);

		m_write += size;
	}

	return size;
}

void RingBuffer::clear()
{
	m_read = m_write;
}

void RingBuffer::reset()
{
	m_read = 0;
	m_write = 0;
}

void RingBuffer::resize(int size)
{
	m_buffer.resize(size);
	reset();
}

void RingBuffer::assureSize(int size)
{
	m_buffer.assureSize(size);
	reset();
}

AUD_NAMESPACE_END
