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

/** \file audaspace/intern/AUD_Reference.cpp
 *  \ingroup audaspaceintern
 */

#include "AUD_Reference.h"

std::map<void*, unsigned int> AUD_ReferenceHandler::m_references;
pthread_mutex_t AUD_ReferenceHandler::m_mutex;
bool AUD_ReferenceHandler::m_mutex_initialised = false;

pthread_mutex_t *AUD_ReferenceHandler::getMutex()
{
	if(!m_mutex_initialised)
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

		pthread_mutex_init(&m_mutex, &attr);

		pthread_mutexattr_destroy(&attr);

		m_mutex_initialised = true;
	}

	return &m_mutex;
}

