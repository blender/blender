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
 * @file	MEM_SmartPtr.h
 * Declaration of MEM_RefCounted and MEM_RefCountable classes.
 * @author Laurence
 */

#ifndef NAN_INCLUDED_MEM_SmartPtr_h
#define NAN_INCLUDED_MEM_SmartPtr_h


#include <stdlib.h> // for NULL !


/**
 * @section MEM_SmartPtr 
 * This class defines a smart pointer similar to that defined in 
 * the Standard Template Library but without the painful get()
 * semantics to access the internal c style pointer.
 *
 * It is often useful to explicitely decalre ownership of memory
 * allocated on the heap within class or function scope. This
 * class helps you to encapsulate this ownership within a value
 * type. When an instance of this class goes out of scope it
 * makes sure that any memory associated with it's internal pointer
 * is deleted. It can help to inform users of an aggregate class
 * that it owns instances of it's members and these instances 
 * should not be shared. This is not reliably enforcable in C++
 * but this class attempts to make the 1-1 relationship clear.
 * 
 * @section Example usage
 *
 * class foo {
 *		...constructors accessors etc.
 *		int x[1000];
 * }
 * 
 * class bar {
 *  public :
 *		static
 *			bar *
 *		New(
 *		) {
 *			MEM_SmartPtr<foo> afoo = new foo();
 *			MEM_SmartPtr<bar> that = new bar();
 *
 *			if (foo == NULL || that == NULL) return NULL;
 *
 *			that->m_foo = afoo.Release();
 *			return that.Release();
 *		}
 *
 *		~bar() {
 *			// smart ptr takes care of deletion
 *		}
 *	private :
 *		MEM_SmartPtr<foo> m_foo;
 *	}
 *			
 * You may also safely construct vectors of MEM_SmartPtrs and 
 * have the vector own stuff you put into it. 
 *
 * e.g.
 * { 
 * std::vector<MEM_SmartPtr<foo> > foo_vector;
 * foo_vector.push_back( new foo());
 * foo_vector.push_back( new foo());
 *
 * foo_vector[0]->bla();
 * } // foo_vector out of scope => heap memory freed for both foos
 *
 * @warning this class should only be used for objects created
 * on the heap via the new function. It will not behave correctly
 * if you pass ptrs to objects created with new[] nor with 
 * objects declared on the stack. Doing this is likely to crash
 * the program or lead to memory leaks.
 */

template 
	< class T >
class MEM_SmartPtr {

public :

	/**
	 * Construction from reference - this class
	 * always assumes ownership from the rhs.
	 */

	MEM_SmartPtr(
		const MEM_SmartPtr &rhs
	){
		m_val = rhs.Release();
	}

	/**
	 * Construction from ptr - this class always
	 * assumes that it now owns the memory associated with the
	 * ptr.
	 */

	MEM_SmartPtr(
		T* val
	) :
		m_val (val)
	{
	}
	
	/**
	 * Defalut constructor
	 */

	MEM_SmartPtr(
	) :
		m_val (NULL)
	{
	}

	/**
	 * Type conversion from this class to the type
	 * of a pointer to the template parameter. 
	 * This means you can pass an instance of this class
	 * to a function expecting a ptr of type T.
	 */

	operator T * () const {
		return m_val;
	}

	/**
	 * Return a reference to the internal ptr class.
	 * Use with care when you now that the internal ptr
	 * is not NULL!
	 */

		T &
	Ref(
	) const {
		return *m_val;
	}	

	/** 
	 * Assignment operator - ownership is transfered from rhs to lhs. 
	 * There is an intenional side-effect of function of transferring
	 * ownership from the const parameter rhs. This is to insure 
	 * the 1-1 relationship.
	 * The object associated with this instance is deleted if it 
	 * is not the same as that contained in the rhs.
	 */

	MEM_SmartPtr & operator=(
		const MEM_SmartPtr &rhs
	) {
		if (this->m_val != rhs.m_val) {
			delete this->m_val;
		}

		this->m_val = rhs.Release();
		return *this;
	}
	
	/** 
	 * Overload the operator -> so that it's possible to access
	 * all the normal methods of the internal ptr. 
	 */
	
	T * operator->() const {
		return m_val;
	}

	/**
	 * Caller takes ownership of the object - the object will not 
	 * be deleted when the ptr goes out of scope.
	 */

		T *
	Release(
	) const {
		T* temp = m_val;
		(const_cast<MEM_SmartPtr *>(this))->m_val = NULL;	
		return temp;
	}

	/**
	 * Force destruction of the internal object.
	 */
	
		void
	Delete(
	) {
		delete (m_val);
		m_val = NULL;
	}

	/** 
	 * Destructor - deletes object if it exists
	 */

	~MEM_SmartPtr(
	) {
		delete (m_val);
	}

private :
	
	/// The ptr owned by this class.
	T * m_val;
};

#endif

