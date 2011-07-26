/*
 * $Id$
 *
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

#include <cstring>

AUD_AnimateableProperty::AUD_AnimateableProperty(int count) :
	AUD_Buffer(count * sizeof(float)), m_count(count), m_isAnimated(false), m_changed(false)
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
	lock();

	m_isAnimated = false;
	m_changed = true;
	memcpy(getBuffer(), data, m_count * sizeof(float));

	unlock();
}

void AUD_AnimateableProperty::write(const float* data, int position, int count)
{
	lock();

	m_isAnimated = true;
	m_changed = true;
	assureSize((count + position) * m_count * sizeof(float), true);
	memcpy(getBuffer() + position * m_count, data, count * m_count * sizeof(float));

	unlock();
}

const float* AUD_AnimateableProperty::read(int position) const
{
	return getBuffer() + position * m_count;
}

bool AUD_AnimateableProperty::isAnimated() const
{
	return m_isAnimated;
}

bool AUD_AnimateableProperty::hasChanged()
{
	bool result = m_changed;
	m_changed = false;
	return result;
}
