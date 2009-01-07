/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
 */
/**
 * @file	MEM_RefCounted.h
 * Declaration of MEM_RefCounted class.
 */

#ifndef _H_MEM_REF_COUNTED
#define _H_MEM_REF_COUNTED

/**
 * An object with reference counting.
 * Base class for objects with reference counting.
 * When a shared object is ceated, it has reference count == 1.
 * If the the reference count of a shared object reaches zero, the object self-destructs.
 * The default destructor of this object has been made protected on purpose.
 * This disables the creation of shared objects on the stack.
 *
 * @author	Maarten Gribnau
 * @date	March 31, 2001
 */

class MEM_RefCounted {
public:
	/**
	 * Constructs a a shared object.
	 */
	MEM_RefCounted() : m_refCount(1)
	{
	}

	/** 
	 * Returns the reference count of this object.
	 * @return the reference count.
	 */
	inline virtual int getRef() const;

	/** 
	 * Increases the reference count of this object.
	 * @return the new reference count.
	 */
	inline virtual int incRef();

	/** 
	 * Decreases the reference count of this object.
	 * If the the reference count reaches zero, the object self-destructs.
	 * @return the new reference count.
	 */
	inline virtual int decRef();

protected:
	/**
	 * Destructs a shared object.
	 * The destructor is protected to force the use of incRef and decRef.
	 */
	virtual ~MEM_RefCounted()
	{
	}

protected:
	/// The reference count.
	int m_refCount;
};


inline int MEM_RefCounted::getRef() const
{
	return m_refCount;
}

inline int MEM_RefCounted::incRef()
{
	return ++m_refCount;
}

inline int MEM_RefCounted::decRef()
{
	m_refCount--;
	if (m_refCount == 0) {
		delete this;
		return 0;
	}
	return m_refCount;
}

#endif // _H_MEM_REF_COUNTED

