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

#include "sequence/AnimateableProperty.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <cmath>
#include <mutex>

AUD_NAMESPACE_BEGIN

// the number of frames of a block of the buffer
static const int BLOCK_FRAMES = 1024;

// allocates the values of frames, which are unset
static std::unique_ptr<float[]> allocateFrames(long long frames, int count)
{
	return std::unique_ptr<float[]>(new float[frames * count]);
}

AnimateableProperty::AnimateableProperty(int count) :
	m_count(count), m_isAnimated(false), m_bufferPosition(0), m_frames(count, 0.0f), m_blockOffset(0), m_length(1)
{
}

AnimateableProperty::AnimateableProperty(int count, float value) :
	m_count(count), m_isAnimated(false), m_bufferPosition(0), m_frames(count, value), m_blockOffset(0), m_length(1)
{
}

float* AnimateableProperty::getFrames(int position, long long& frames)
{
	long long index = (long long)position - m_bufferPosition;

	if(!m_blocks)
	{
		frames = m_length - index;
		return m_frames.data() + index * m_count;
	}

	index += m_blockOffset;
	frames = BLOCK_FRAMES - index % BLOCK_FRAMES;
	return (*m_blocks)[index / BLOCK_FRAMES].get() + (index % BLOCK_FRAMES) * m_count;
}

float* AnimateableProperty::getFrame(int position)
{
	long long frames;
	return getFrames(position, frames);
}

void AnimateableProperty::growBuffer(int before, int after)
{
	const long long length = (long long)m_length + before + after;

	if(!m_blocks)
	{
		if(length <= BLOCK_FRAMES)
		{
			// a buffer that fits into a block grows within the capacity of a block
			const size_t size = size_t(length) * m_count;

			if(size > m_frames.capacity())
				m_frames.reserve(std::min(std::max(size, 2 * m_frames.capacity()), size_t(BLOCK_FRAMES) * m_count));

			m_frames.insert(m_frames.begin(), size_t(before) * m_count, 0.0f);
			m_frames.resize(size);
			m_length = int(length);
			return;
		}

		// a longer buffer is stored in blocks, the stored frames are copied into the first
		m_blocks = std::make_unique<std::deque<std::unique_ptr<float[]>>>();
		m_blocks->push_back(allocateFrames(BLOCK_FRAMES, m_count));
		std::memcpy(m_blocks->front().get(), m_frames.data(), m_frames.size() * sizeof(float));
		m_frames = std::vector<float>();
		m_blockOffset = 0;
	}

	// blocks are added before and after the stored frames, which stay where they are
	long long offset = (long long)m_blockOffset - before;

	for(; offset < 0; offset += BLOCK_FRAMES)
		m_blocks->push_front(allocateFrames(BLOCK_FRAMES, m_count));

	m_blockOffset = int(offset);
	m_length = int(length);

	while((long long)m_blocks->size() * BLOCK_FRAMES < m_blockOffset + length)
		m_blocks->push_back(allocateFrames(BLOCK_FRAMES, m_count));
}

void AnimateableProperty::growBufferEnd(int position)
{
	const long long length = (long long)position - m_bufferPosition;

	if(length > m_length)
		growBuffer(0, int(length - m_length));
}

void AnimateableProperty::discardBufferStart(int count)
{
	const int length = m_length - count;

	if(!m_blocks)
		m_frames.erase(m_frames.begin(), m_frames.begin() + size_t(count) * m_count);
	else
	{
		// the blocks before the first frame are freed
		long long offset = (long long)m_blockOffset + count;

		for(; offset >= BLOCK_FRAMES; offset -= BLOCK_FRAMES)
			m_blocks->pop_front();

		m_blockOffset = int(offset);
	}

	m_length = length;
}

void AnimateableProperty::fillFrames(int start, int end, const float* data)
{
	for(long long position = start; position < end;)
	{
		long long frames;
		float* buf = getFrames(int(position), frames);
		frames = std::min(frames, end - position);

		if(m_count == 1)
			std::fill(buf, buf + frames, *data);
		else
		{
			for(long long i = 0; i < frames; i++)
				std::memcpy(buf + i * m_count, data, m_count * sizeof(float));
		}

		position += frames;
	}
}

void AnimateableProperty::writeFrames(int position, int count, const float* data)
{
	for(long long i = 0; i < count;)
	{
		long long frames;
		float* buf = getFrames(int(position + i), frames);
		frames = std::min(frames, count - i);

		std::memcpy(buf, data + i * m_count, frames * m_count * sizeof(float));
		i += frames;
	}
}

void AnimateableProperty::updateUnknownCache(int start, int end)
{
	// we could do a better interpolation than zero order, but that doesn't work with Blender's animation system
	// as frames are only written when changing, so to support jumps, we need zero order interpolation here.
	fillFrames(start, end + 1, getFrame(start - 1));
}

void AnimateableProperty::extendBufferStart(int position)
{
	if(position >= m_bufferPosition)
		return;

	const int count = m_bufferPosition - position;

	growBuffer(count, 0);

	// the discarded frames are unknown again
	if(!m_unknown.empty() && m_unknown.front().start == m_bufferPosition)
		m_unknown.front().start = position;
	else
		m_unknown.push_front(Unknown(position, m_bufferPosition - 1));

	m_bufferPosition = position;
}

int AnimateableProperty::prepareWrite(int position)
{
	// reading before the buffer returns the value of its first frame, keep it for the two frames
	// before the written ones, which reading them interpolates with, but only from frame 0, like
	// when the buffer always started at frame 0
	const int start = position >= 2 ? position - 2 : std::min(position, 0);

	if(!m_isAnimated)
	{
		// the static value is the first frame of the buffer
		m_isAnimated = true;
		m_bufferPosition = start;
		return start;
	}

	const int end = m_bufferPosition + m_length;

	if(start < m_bufferPosition)
	{
		const int first = m_bufferPosition - start;

		extendBufferStart(start);

		// the added frames keep the value that they had before the buffer, frames from the written
		// ones on are written or filled again
		fillFrames(start, start + first, getFrame(start + first));
	}

	return end;
}

AnimateableProperty::~AnimateableProperty()
{
}

int AnimateableProperty::getCount() const
{
	return m_count;
}

void AnimateableProperty::lock()
{
	m_mutex.lock();
}

void AnimateableProperty::unlock()
{
	m_mutex.unlock();
}

void AnimateableProperty::reset()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	const float* first = getFrame(m_bufferPosition);
	m_frames = std::vector<float>(first, first + m_count);
	m_blocks.reset();
	m_blockOffset = 0;
	m_length = 1;
	m_isAnimated = false;
	m_unknown.clear();
	m_bufferPosition = 0;
}

void AnimateableProperty::write(const float* data)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_isAnimated = false;
	m_unknown.clear();
	m_bufferPosition = 0;
	std::memcpy(getFrame(0), data, m_count * sizeof(float));
}

void AnimateableProperty::writeConstantRange(const float* data, int position_start, int position_end)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	const bool animated = m_isAnimated;
	int pos = prepareWrite(position_start);

	growBufferEnd(position_end);
	
	// if we were not animated yet, use the new constant value as the
	// first/default value to fill in the unknown parts
	if (!animated)
		std::memcpy(getFrame(m_bufferPosition), data, m_count * sizeof(float));

	fillFrames(position_start, position_end, data);

	m_isAnimated = true;
	
	updateUnknownAfterWrite(pos, position_start, position_end - position_start);
}

void AnimateableProperty::write(const float* data, int position, int count)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	int pos = prepareWrite(position);

	growBufferEnd(position + count);

	writeFrames(position, count, data);
	
	updateUnknownAfterWrite(pos, position, count);
}

void AnimateableProperty::updateUnknownAfterWrite(int pos, int position, int count)
{
	// have to fill up space between?
	if(pos < position)
	{
		m_unknown.push_back(Unknown(pos, position - 1));

		// if the buffer was not animated before, we copy the previous static value
		if(pos == m_bufferPosition)
			pos++;

		updateUnknownCache(pos, position - 1);
	}
	// otherwise it's not at the end, let's check if some unknown part got filled
	else
	{
		bool erased = false;

		for(auto it = m_unknown.begin(); it != m_unknown.end(); erased ? it : it++)
		{
			erased = false;

			// unknown area before position
			if(it->end < position)
				continue;

			// we're after the new area, let's stop
			if(it->start >= position + count)
				break;

			// we have an intersection, now 4 cases:
			// the start is included
			if(position <= it->start)
			{
				// the end is included
				if(position + count > it->end)
				{
					// simply delete
					it = m_unknown.erase(it);
					erased = true;
				}
				// the end is excluded, a second part remains
				else
				{
					// update second part
					it->start = position + count;
					updateUnknownCache(it->start, it->end);
					break;
				}
			}
			// start is excluded, a first part remains
			else
			{
				// the end is included
				if(position + count > it->end)
				{
					// update first part
					it->end = position - 1;
				}
				// the end is excluded, a second part remains
				else
				{
					// add another item and update both parts
					m_unknown.insert(it, Unknown(it->start, position - 1));
					it->start = position + count;
					updateUnknownCache(it->start, it->end);
				}
			}
		}
	}
}

void AnimateableProperty::discardBefore(int position)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	// nothing is before the buffer, also for positions like INT_MIN that mean no position
	if(!m_isAnimated || position <= m_bufferPosition)
		return;

	// keep the last frame, its value continues after the buffer
	const int count = int(std::min((long long)position - m_bufferPosition, (long long)m_length - 1));

	if(count <= 0)
		return;

	discardBufferStart(count);
	m_bufferPosition += count;

	while(!m_unknown.empty() && m_unknown.front().end < m_bufferPosition)
		m_unknown.pop_front();

	if(!m_unknown.empty() && m_unknown.front().start < m_bufferPosition)
		m_unknown.front().start = m_bufferPosition;
}

void AnimateableProperty::read(float position, float* out)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if(!m_isAnimated)
	{
		std::memcpy(out, getFrame(m_bufferPosition), m_count * sizeof(float));
		return;
	}

	int last = m_bufferPosition + m_length - 1;
	float t = position - std::floor(position);

	if(position >= last)
	{
		position = last;
		t = 0;
	}

	if(position < m_bufferPosition)
	{
		position = m_bufferPosition;
		t = 0;
	}

	if(t == 0)
	{
		std::memcpy(out, getFrame(int(std::floor(position))), m_count * sizeof(float));
	}
	else
	{
		int pos = int(std::floor(position));
		float t2 = t * t;
		float t3 = t2 * t;
		float m0, m1;
		float* p0;
		float* p1 = getFrame(pos);
		float* p2;
		float* p3;

		if(pos == m_bufferPosition)
			p0 = p1;
		else
			p0 = getFrame(pos - 1);

		p2 = getFrame(pos + 1);
		if(pos + 1 == last)
			p3 = p2;
		else
			p3 = getFrame(pos + 2);

		for(int i = 0; i < m_count; i++)
		{
			m0 = (p2[i] - p0[i]) / 2.0f;
			m1 = (p3[i] - p1[i]) / 2.0f;

			out[i] = (2 * t3 - 3 * t2 + 1) * p1[i] + (-2 * t3 + 3 * t2) * p2[i] + (t3 - 2 * t2 + t) * m0 + (t3 - t2) * m1;
		}
	}
}

float AnimateableProperty::readSingle(float position)
{
	assert(m_count == 1);
	float value;
	read(position, &value);
	return value;
}

int AnimateableProperty::getUnknownFramesAfter(int position)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	for(const Unknown& unknown : m_unknown)
	{
		if(unknown.start <= position && position < unknown.end)
			return unknown.end - position;
	}

	return 0;
}

bool AnimateableProperty::isAnimated() const
{
	return m_isAnimated;
}

int AnimateableProperty::getBufferPosition() const
{
	return m_bufferPosition;
}

int AnimateableProperty::getBufferLength() const
{
	return m_length;
}

void AnimateableProperty::readFrames(int position, int count, float* out)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	const long long last = m_isAnimated ? (long long)m_bufferPosition + m_length - 1 : m_bufferPosition;

	for(long long i = 0; i < count;)
	{
		const long long frame = position + i;

		// frames outside the buffer have the value of its first or last frame
		if(frame < m_bufferPosition || frame > last)
		{
			std::memcpy(out + i * m_count, getFrame(frame < m_bufferPosition ? m_bufferPosition : int(last)), m_count * sizeof(float));
			i++;
			continue;
		}

		long long frames;
		const float* buf = getFrames(int(frame), frames);
		frames = std::min(frames, std::min(count - i, last + 1 - frame));

		std::memcpy(out + i * m_count, buf, frames * m_count * sizeof(float));
		i += frames;
	}
}

AUD_NAMESPACE_END
