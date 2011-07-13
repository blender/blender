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

/** \file audaspace/intern/AUD_Reference.h
 *  \ingroup audaspaceintern
 */

#ifndef AUD_REFERENCE
#define AUD_REFERENCE

#include <map>

#ifdef MEM_DEBUG
#include <iostream>
#include <typeinfo>
#endif

class AUD_ReferenceHandler
{
private:
	static std::map<void*, int> m_references;

public:
	static inline void incref(void* reference)
	{
		if(!reference)
			return;

		std::map<void*, int>::iterator result = m_references.find(reference);
		if(result != m_references.end())
		{
			m_references[reference]++;
		}
		else
		{
			m_references[reference] = 1;
		}
	}

	static inline bool decref(void* reference)
	{
		if(!reference)
			return false;

		if(!--m_references[reference])
		{
			m_references.erase(reference);
			return true;
		}
		return false;
	}
};

template <class T>
/**
 * This class provides reference counting functionality.
 */
class AUD_Reference
{
private:
	/// The reference.
	T* m_reference;
	void* m_original;
public:
	/**
	 * Creates a new reference counter.
	 * \param reference The reference.
	 */
	AUD_Reference(T* reference = 0)
	{
		m_original = m_reference = reference;
		AUD_ReferenceHandler::incref(reference);
#ifdef MEM_DEBUG
		if(m_reference != 0)
			std::cerr << "+" << typeid(*m_reference).name() << std::endl;
#endif
	}

	/**
	 * Copies an AUD_Reference object.
	 * \param ref The AUD_Reference object to copy.
	 */
	AUD_Reference(const AUD_Reference& ref)
	{
		m_original = m_reference = ref.m_reference;
		AUD_ReferenceHandler::incref(m_reference);
#ifdef MEM_DEBUG
		if(m_reference != 0)
			std::cerr << "+" << typeid(*m_reference).name() << std::endl;
#endif
	}

	template <class U>
	explicit AUD_Reference(const AUD_Reference<U>& ref)
	{
		m_original = ref.get();
		m_reference = dynamic_cast<T*>(ref.get());
		AUD_ReferenceHandler::incref(m_original);
#ifdef MEM_DEBUG
		if(m_reference != 0)
			std::cerr << "+" << typeid(*m_reference).name() << std::endl;
#endif
	}

	/**
	 * Destroys a AUD_Reference object, if there's no furthere reference on the
	 * reference, it is destroyed as well.
	 */
	~AUD_Reference()
	{
#ifdef MEM_DEBUG
		if(m_reference != 0)
			std::cerr << "-" << typeid(*m_reference).name() << std::endl;
#endif
		if(AUD_ReferenceHandler::decref(m_original))
			delete m_reference;
	}

	/**
	 * Assigns an AUD_Reference to this object.
	 * \param ref The AUD_Reference object to assign.
	 */
	AUD_Reference& operator=(const AUD_Reference& ref)
	{
		if(&ref == this)
			return *this;

#ifdef MEM_DEBUG
		if(m_reference != 0)
			std::cerr << "-" << typeid(*m_reference).name() << std::endl;
#endif
		if(AUD_ReferenceHandler::decref(m_original))
			delete m_reference;

		m_original = ref.m_original;
		m_reference = ref.m_reference;
		AUD_ReferenceHandler::incref(m_original);
#ifdef MEM_DEBUG
		if(m_reference != 0)
			std::cerr << "+" << typeid(*m_reference).name() << std::endl;
#endif

		return *this;
	}

	/**
	 * Returns whether the reference is NULL.
	 */
	inline bool isNull() const
	{
		return m_reference == 0;
	}

	/**
	 * Returns the reference.
	 */
	inline T* get() const
	{
		return m_reference;
	}

	inline void* getOriginal() const
	{
		return m_original;
	}

	/**
	 * Returns the reference.
	 */
	inline T& operator*() const
	{
		return *m_reference;
	}

	/**
	 * Returns the reference.
	 */
	inline T* operator->() const
	{
		return m_reference;
	}
};

template<class T, class U>
inline bool operator==(const AUD_Reference<T>& a, const AUD_Reference<U>& b)
{
	return a.getOriginal() == b.getOriginal();
}

template<class T, class U>
inline bool operator!=(const AUD_Reference<T>& a, const AUD_Reference<U>& b)
{
	return a.getOriginal() != b.getOriginal();
}

#endif // AUD_REFERENCE
