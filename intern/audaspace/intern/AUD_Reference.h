/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#ifndef AUD_REFERENCE
#define AUD_REFERENCE

template <class T>
/**
 * This class provides reference counting functionality.
 */
class AUD_Reference
{
private:
	/// The reference.
	T* m_reference;
	/// The reference counter.
	int* m_refcount;
public:
	/**
	 * Creates a new reference counter.
	 * \param reference The reference.
	 */
	AUD_Reference(T* reference = 0)
	{
		m_reference = reference;
		m_refcount = new int;
		*m_refcount = 1;
	}

	/**
	 * Copies an AUD_Reference object.
	 * \param ref The AUD_Reference object to copy.
	 */
	AUD_Reference(const AUD_Reference& ref)
	{
		m_reference = ref.m_reference;
		m_refcount = ref.m_refcount;
		(*m_refcount)++;
	}

	/**
	 * Destroys a AUD_Reference object, if there's no furthere reference on the
	 * reference, it is destroyed as well.
	 */
	~AUD_Reference()
	{
		(*m_refcount)--;
		if(*m_refcount == 0)
		{
			if(m_reference)
			{
				delete m_reference;
			}
			delete m_refcount;
		}
	}

	/**
	 * Assigns an AUD_Reference to this object.
	 * \param ref The AUD_Reference object to assign.
	 */
	AUD_Reference& operator=(const AUD_Reference& ref)
	{
		if(&ref == this)
			return *this;

		(*m_refcount)--;
		if(*m_refcount == 0)
		{
			if(m_reference)
			{
				delete m_reference;
			}
			delete m_refcount;
		}

		m_reference = ref.m_reference;
		m_refcount = ref.m_refcount;
		(*m_refcount)++;

		return *this;
	}

	/**
	 * Returns the reference.
	 */
	T* get() const
	{
		return m_reference;
	}
};

#endif // AUD_REFERENCE
