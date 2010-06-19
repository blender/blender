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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
 * @file	MEM_RefCountPtr.h
 * Declaration of MEM_RefCounted and MEM_RefCountable classes.
 * @author Laurence
 */

#ifndef NAN_INCLUDED_MEM_RefCountPtr_h
#define NAN_INCLUDED_MEM_RefCountPtr_h

#include <stdlib.h> // for NULL !

/**
 * @section MEM_RefCountable
 * This is a base class for reference countable objects.
 * If you want an object to be shared using a reference 
 * counted system derrivce from this class. All subclasses
 * should insist that they are created on the heap, this
 * can be done by makeing all constructors private and 
 * defining a static New() method that returns a ref counted
 * ptr to a new()ly allocated instance. 
 *
 * @section Example subclass
 *
 *
 * class MySharedObject : public MEM_RefCountable {
 *
 * private :
 *	MySharedObject() : MEM_RefCountable() { //class specific initialization};
 *  MySharedObject(const MySharedObject &other) // not implemented
 * public :
 *		static	
 *			MEM_RefCountPtr<MySharedObject> 
 *		New(
 *		) {
 *			return MEM_RefCountPtr<MySharedObject>( new MySharedObject());
 *		}
 *		
 *		// other member functions
 * };
 *
 * Alternitively you may first wish to define a fully functional
 * class and then define a reference counting wrapper for this class.
 * This is useful when the base type can be used without reference
 * counting.
 *
 * E.g.
 * class UsefullClass {
 * private :
 *  ...
 * public :
 *
 *      UsefullClass()
 *      UsefullMethod(...)
 *      AnotherUsefullMethod(...)
 * };
 *
 * class RcUsefullClass : public UsefullClass, public MEM_RefCountable
 * {
 * private :
 *      // Override base class public constructor --- forces
 *      // use of New(...)
 *      RcUsefullClass(...)
 * public :
 *
 *  // Override each public constructor of UsefullClass with
 *  // an equivalent static New method returning a MEM_RefCountPtr
 *
 *  static
 *      MEM_RefCountPtr<RcUsefullClass>
 *  New(...){
 *       return MEM_RefCountPtr<RcUsefullClass> output(
 *           new UsefullClass(...)
 *       );
 *   }
 *
 *  // warning never call destructor directly allow ref counting
 *  // mechanism to handle object lifetime.
 *  ~RcUsefullClass();
 * };
 *
 *
 */

class MEM_RefCountable {
private :

	/**
	 * The reference count!
	 * We use mutable here because we would like to
	 * share references of const objects!
	 * Maybe should think about having decRef()
	 * another value because we should not be deleting
	 * non-const objects
	 */

	mutable int m_count;

protected :

	/**
	 * Protected constructors
	 * This class is not for direct instanciation. Sub classes
	 * should only be allocated on the heap.
	 */

	MEM_RefCountable (
	) :
		m_count (0)
	{
	};

	MEM_RefCountable (
		const MEM_RefCountable &
	) :
		m_count (0)
	{
	}

public :

		void
	IncRef(
	) const {
		m_count++;
	}

		int
	DecRef(
	) {
		return (--m_count);
	}

	~MEM_RefCountable(
	) {
		//nothing to do
	}
};

/**
 * @section MEM_RefCountPtr
 */

template
	< class T >
class MEM_RefCountPtr {

public :

	/**
	 * Construction from reference - share ownership with
	 * the right hand side.
	 */

	MEM_RefCountPtr(
		const MEM_RefCountPtr &rhs
	) : m_val (NULL) {
		ShareOwnership(rhs.m_val);
	}

	/**
	 * Construction from ptr - this class shares
	 * ownership of object val.
	 */

	MEM_RefCountPtr(
		const T* val
	) :
		m_val (NULL)
	{
		ShareOwnership(val);
	}

	/**
	 * Default constructor
	 */

	MEM_RefCountPtr(
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


	MEM_RefCountPtr & operator=(
		const MEM_RefCountPtr &rhs
	) {
		if (this->m_val != rhs.m_val) {
			ReleaseOwnership();
			ShareOwnership(rhs.m_val);
		}
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
	 * Returrn a reference to the shared object.
	 */

		T&
	Ref(
	) {
		return *m_val;
	}


	/**
	 * Destructor - deletes object if it's ref count is zero.
	 */

	~MEM_RefCountPtr(
	) {
		ReleaseOwnership();
	}

private :
	
	/// The ptr owned by this class.
	T * m_val;

		void
	ShareOwnership(
		const T * val
	) {
		if (val != NULL) {
			val->IncRef();
		}
		m_val = const_cast<T *>(val);
	}
		
		void
	ReleaseOwnership(
	) {
		if (m_val) {
			if (m_val->DecRef() == 0) {
				delete(m_val);
				m_val = NULL;
			}
		}
	}

};

#endif

