/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Interface for C access to functionality relating to shared objects in the foundation library.
 * @author	Maarten Gribnau
 * @date	June 17, 2001
 */

#ifndef _H_MEM_REF_COUNTED_C_API
#define _H_MEM_REF_COUNTED_C_API

/** A pointer to a private object. */
typedef struct MEM_TOpaqueObject* MEM_TObjectPtr;
/** A pointer to a shared object. */
typedef MEM_TObjectPtr MEM_TRefCountedObjectPtr;


#ifdef __cplusplus
extern "C" {
#endif


/**
 * A shared object in an object with reference counting.
 * When a shared object is ceated, it has reference count == 1.
 * If the the reference count of a shared object reaches zero, the object self-destructs.
 * The default constrcutor and destructor of this object have been made protected on purpose.
 * This disables the creation and disposal of shared objects on the stack.
 */

/** 
 * Returns the reference count of this object.
 * @param shared The object to query.
 * @return The current reference count.
 */
extern int	MEM_RefCountedGetRef(MEM_TRefCountedObjectPtr shared);

/** 
 * Increases the reference count of this object.
 * @param shared The object to query.
 * @return The new reference count.
 */
extern int	MEM_RefCountedIncRef(MEM_TRefCountedObjectPtr shared);

/** 
 * Decreases the reference count of this object.
 * If the the reference count reaches zero, the object self-destructs.
 * @param shared The object to query.
 * @return The new reference count.
 */
extern int	MEM_RefCountedDecRef(MEM_TRefCountedObjectPtr shared);


#ifdef __cplusplus
}
#endif

#endif // _H_MEM_REF_COUNTED_C_API

