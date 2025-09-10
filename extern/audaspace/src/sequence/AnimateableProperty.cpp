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

#include <cassert>
#include <cstring>
#include <cmath>
#include <mutex>

AUD_NAMESPACE_BEGIN

AnimateableProperty::AnimateableProperty(int count) :
	Buffer(count * sizeof(float)), m_count(count), m_isAnimated(false)
{
	std::memset(Buffer::getBuffer(), 0, count * sizeof(float));
}

AnimateableProperty::AnimateableProperty(int count, float value) :
	Buffer(count * sizeof(float)), m_count(count), m_isAnimated(false)
{
	sample_t* buf = Buffer::getBuffer();

	for(int i = 0; i < count; i++)
		buf[i] = value;
}

void AnimateableProperty::updateUnknownCache(int start, int end)
{
	float* buf = Buffer::getBuffer();

	// we could do a better interpolation than zero order, but that doesn't work with Blender's animation system
	// as frames are only written when changing, so to support jumps, we need zero order interpolation here.
	for(int i = start; i <= end; i++)
		std::memcpy(buf + i * m_count, buf + (start - 1) * m_count, m_count * sizeof(float));
}

AnimateableProperty::~AnimateableProperty()
{
}

int AnimateableProperty::getCount() const
{
	return m_count;
}

void AnimateableProperty::write(const float* data)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_isAnimated = false;
	m_unknown.clear();
	std::memcpy(Buffer::getBuffer(), data, m_count * sizeof(float));
}

void AnimateableProperty::writeConstantRange(const float* data, int position_start, int position_end)
{
	assureSize(position_end * m_count * sizeof(float), true);
	float* buffer = Buffer::getBuffer();

	for(int i = position_start; i < position_end; i++)
	{
		std::memcpy(buffer + i * m_count, data, m_count * sizeof(float));
	}

	m_isAnimated = true;
}

void AnimateableProperty::write(const float* data, int position, int count)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	int pos = getSize() / (sizeof(float) * m_count);

	if(!m_isAnimated)
		pos = 0;

	m_isAnimated = true;

	assureSize((count + position) * m_count * sizeof(float), true);

	float* buf = Buffer::getBuffer();

	std::memcpy(buf + position * m_count, data, count * m_count * sizeof(float));

	// have to fill up space between?
	if(pos < position)
	{
		m_unknown.push_back(Unknown(pos, position - 1));

		// if the buffer was not animated before, we copy the previous static value
		if(pos == 0)
			pos = 1;

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

void AnimateableProperty::read(float position, float* out)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if(!m_isAnimated)
	{
		std::memcpy(out, Buffer::getBuffer(), m_count * sizeof(float));
		return;
	}

	int last = getSize() / (sizeof(float) * m_count) - 1;
	float t = position - std::floor(position);

	if(position >= last)
	{
		position = last;
		t = 0;
	}

	if(position < 0)
	{
		position = 0;
		t = 0;
	}

	if(t == 0)
	{
		std::memcpy(out, Buffer::getBuffer() + int(std::floor(position)) * m_count, m_count * sizeof(float));
	}
	else
	{
		int pos = int(std::floor(position)) * m_count;
		float t2 = t * t;
		float t3 = t2 * t;
		float m0, m1;
		float* p0;
		float* p1 = Buffer::getBuffer() + pos;
		float* p2;
		float* p3;
		last *= m_count;

		if(pos == 0)
			p0 = p1;
		else
			p0 = p1 - m_count;

		p2 = p1 + m_count;
		if(pos + m_count == last)
			p3 = p2;
		else
			p3 = p2 + m_count;

		for(int i = 0; i < m_count; i++)
		{
			m0 = (p2[i] - p0[i]) / 2.0f;
			m1 = (p3[i] - p1[i]) / 2.0f;

			out[i] = (2 * t3 - 3 * t2 + 1) * p0[i] + (-2 * t3 + 3 * t2) * p1[i] +
					 (t3 - 2 * t2 + t) * m0 + (t3 - t2) * m1;
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

bool AnimateableProperty::isAnimated() const
{
	return m_isAnimated;
}

const Buffer& AnimateableProperty::getBuffer()
{
	return *this;
}

AUD_NAMESPACE_END
