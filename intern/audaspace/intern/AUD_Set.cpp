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

/** \file audaspace/intern/AUD_Set.cpp
 *  \ingroup audaspaceintern
 */

#include <set>

#include "AUD_Set.h"

void *AUD_createSet()
{
	return new std::set<void *>();
}

void AUD_destroySet(void *set)
{
	delete reinterpret_cast<std::set<void *>*>(set);
}

char AUD_removeSet(void *set, void *entry)
{
	if (set)
		return reinterpret_cast<std::set<void *>*>(set)->erase(entry);
	return 0;
}

void AUD_addSet(void *set, void *entry)
{
	if (entry)
		reinterpret_cast<std::set<void *>*>(set)->insert(entry);
}

void *AUD_getSet(void *set)
{
	if (set) {
		std::set<void *>* rset = reinterpret_cast<std::set<void *>*>(set);
		if (!rset->empty()) {
			std::set<void *>::iterator it = rset->begin();
			void *result = *it;
			rset->erase(it);
			return result;
		}
	}

	return (void*) 0;
}
