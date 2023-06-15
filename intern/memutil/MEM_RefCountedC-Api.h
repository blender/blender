/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_memutil
 *
 * Interface for C access to functionality relating to shared objects in the foundation library.
 */

#ifndef __MEM_REFCOUNTEDC_API_H__
#define __MEM_REFCOUNTEDC_API_H__

/** A pointer to a private object. */
typedef struct MEM_TOpaqueObject *MEM_TObjectPtr;
/** A pointer to a shared object. */
typedef MEM_TObjectPtr MEM_TRefCountedObjectPtr;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the reference count of this object.
 * \param shared: The object to query.
 * \return The current reference count.
 */
extern int MEM_RefCountedGetRef(MEM_TRefCountedObjectPtr shared);

/**
 * Increases the reference count of this object.
 * \param shared: The object to query.
 * \return The new reference count.
 */
extern int MEM_RefCountedIncRef(MEM_TRefCountedObjectPtr shared);

/**
 * Decreases the reference count of this object.
 * If the reference count reaches zero, the object self-destructs.
 * \param shared: The object to query.
 * \return The new reference count.
 */
extern int MEM_RefCountedDecRef(MEM_TRefCountedObjectPtr shared);

#ifdef __cplusplus
}
#endif

#endif  // __MEM_REFCOUNTEDC_API_H__
