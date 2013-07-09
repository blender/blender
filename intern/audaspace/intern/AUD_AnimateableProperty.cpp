/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_AnimateableProperty.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_AnimateableProperty.h"
#include "AUD_MutexLock.h"

#include <cstring>
#include <cmath>

AUD_AnimateableProperty::AUD_AnimateableProperty(int count) :
	AUD_Buffer(count * sizeof(float)), m_count(count), m_isAnimated(false)
{
	memset(getBuffer(), 0, count * sizeof(float));

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_mutex, &attr);

	pthread_mutexattr_destroy(&attr);
}

AUD_AnimateableProperty::~AUD_AnimateableProperty()
{
	pthread_mutex_destroy(&m_mutex);
}

void AUD_AnimateableProperty::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void AUD_AnimateableProperty::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}

void AUD_AnimateableProperty::write(const float* data)
{
	AUD_MutexLock lock(*this);

	m_isAnimated = false;
	memcpy(getBuffer(), data, m_count * sizeof(float));
}

void AUD_AnimateableProperty::write(const float* data, int position, int count)
{
	AUD_MutexLock lock(*this);

	m_isAnimated = true;

	int pos = getSize() / (sizeof(float) * m_count);

	assureSize((count + position) * m_count * sizeof(float), true);

	float* buf = getBuffer();

	memcpy(buf + position * m_count, data, count * m_count * sizeof(float));

	for(int i = pos; i < position; i++)
		memcpy(buf + i * m_count, buf + (pos - 1) * m_count, m_count * sizeof(float));
}

void AUD_AnimateableProperty::read(float position, float* out)
{
	AUD_MutexLock lock(*this);

	if(!m_isAnimated)
	{
		memcpy(out, getBuffer(), m_count * sizeof(float));
		return;
	}

	int last = getSize() / (sizeof(float) * m_count) - 1;
	float t = position - floor(position);

	if(position >= last)
	{
		position = last;
		t = 0;
	}

	if(t == 0)
	{
		memcpy(out, getBuffer() + int(floor(position)) * m_count, m_count * sizeof(float));
	}
	else
	{
		int pos = int(floor(position)) * m_count;
		float t2 = t * t;
		float t3 = t2 * t;
		float m0, m1;
		float* p0;
		float* p1 = getBuffer() + pos;
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

bool AUD_AnimateableProperty::isAnimated() const
{
	return m_isAnimated;
}
