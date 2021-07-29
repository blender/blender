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

/** \file AUD_Set.h
 *  \ingroup audaspace
 */
 
#ifndef __AUD_SET_H__
#define __AUD_SET_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a new set.
 * \return The new set.
 */
extern void *AUD_createSet(void);

/**
 * Deletes a set.
 * \param set The set to delete.
 */
extern void AUD_destroySet(void *set);

/**
 * Removes an entry from a set.
 * \param set The set work on.
 * \param entry The entry to remove.
 * \return Whether the entry was in the set or not.
 */
extern char AUD_removeSet(void *set, void *entry);

/**
 * Adds a new entry to a set.
 * \param set The set work on.
 * \param entry The entry to add.
 */
extern void AUD_addSet(void *set, void *entry);

/**
 * Removes one entry from a set and returns it.
 * \param set The set work on.
 * \return The entry or NULL if the set is empty.
 */
extern void *AUD_getSet(void *set);

#ifdef __cplusplus
}
#endif

#endif //__AUD_SET_H__
