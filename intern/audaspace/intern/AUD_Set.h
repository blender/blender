/* SPDX-FileCopyrightText: 2009-2011 Jörg Hermann Müller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup audaspace
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
 * \param set: The set to delete.
 */
extern void AUD_destroySet(void *set);

/**
 * Removes an entry from a set.
 * \param set: The set work on.
 * \param entry: The entry to remove.
 * \return Whether the entry was in the set or not.
 */
extern char AUD_removeSet(void *set, void *entry);

/**
 * Adds a new entry to a set.
 * \param set: The set work on.
 * \param entry: The entry to add.
 */
extern void AUD_addSet(void *set, void *entry);

/**
 * Removes one entry from a set and returns it.
 * \param set: The set work on.
 * \return The entry or NULL if the set is empty.
 */
extern void *AUD_getSet(void *set);

#ifdef __cplusplus
}
#endif

#endif  //__AUD_SET_H__
