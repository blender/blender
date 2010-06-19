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

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Simple tagged index class.
 */

#ifndef NAN_INCLUDED_CTR_TaggedIndex_h
#define NAN_INCLUDED_CTR_TaggedIndex_h

/**
 * This class is supposed to be a simple tagged index class. If these
 * were indices into a mesh then we would never need 32 bits for such indices.
 * It is often handy to have a few extra bits around to mark objects etc. We
 * steal a few bits of CTR_TaggedIndex objects for this purpose. From the outside
 * it will behave like a standard unsigned int but just carry the extra tag
 * information around with it.
 */

#include <functional>

enum {

	empty_tag = 0x0,
	empty_index = 0xffffffff
};

template <
	int tag_shift, 
	int index_mask
> class CTR_TaggedIndex {
public:
	CTR_TaggedIndex(
	) : 
		m_val ((empty_tag << tag_shift) | (empty_index & index_mask))
	{
	}

	CTR_TaggedIndex(
		const int val
	) :
		m_val ((val & index_mask) | ((empty_tag << tag_shift) & (~index_mask))) {
	}

	CTR_TaggedIndex(
		const unsigned int val
	) :
		m_val ((val & index_mask) | ((empty_tag << tag_shift) & (~index_mask))) {
	}

	CTR_TaggedIndex(
		const long int val
	) :
		m_val ( ((long int) val & index_mask)
				| ( (empty_tag << tag_shift)
					& (~index_mask)) ) {
	}

	CTR_TaggedIndex(
		const long unsigned int val
	) :
		m_val ( ((long unsigned int)val & index_mask)
				| ( (empty_tag << tag_shift)
					& (~index_mask) ) ) {
	}


#if defined(_WIN64)
	CTR_TaggedIndex(
		const unsigned __int64 val
	) :
		m_val ( ((unsigned __int64)val & index_mask)
				| ( (empty_tag << tag_shift)
					& (~index_mask) ) ) {
	}
#endif

	CTR_TaggedIndex(
		const CTR_TaggedIndex &my_index
	):
		m_val(my_index.m_val)
	{
	}

		bool 
	operator == (
		const CTR_TaggedIndex& rhs
	) const {

		return ((this->m_val & index_mask) == (rhs.m_val & index_mask));
	}		

	operator unsigned int () const {
		return m_val & index_mask;
	}

	operator unsigned long int () const {
		return (unsigned long int)(m_val & index_mask);
	}

	operator int () const {
		return int(m_val & index_mask);
	}

	operator long int () const {
		return (long int)(m_val & index_mask);
	}

#if defined(_WIN64)
	operator unsigned __int64 () const {
			return (unsigned __int64)(m_val & index_mask);
		}
#endif

		bool
	IsEmpty(
	) const {
		return ((m_val & index_mask) == (empty_index & index_mask));
	}


	static
		CTR_TaggedIndex
	Empty(
	) {
		return CTR_TaggedIndex();
	}

		void
	Invalidate(
	) {
		m_val = (empty_tag << tag_shift) | (empty_index & index_mask);
	}


		unsigned int 
	Tag (
	) const {
		return m_val >> tag_shift;
	}
	
		void
	SetTag(
		unsigned int tag
	) {
		m_val = (m_val & index_mask) | ((tag << tag_shift) & (~index_mask));
	}

		void
	EmptyTag(
	) {
		m_val = (m_val & index_mask) | ((empty_tag << tag_shift) & (~index_mask));
	}

		bool
	IsEmptyTag(
	) const {
		return (Tag() == Empty().Tag());
	}
	
	// functionals 

	struct greater : std::binary_function<CTR_TaggedIndex, CTR_TaggedIndex, bool>
	{
			bool 
		operator()(
			const CTR_TaggedIndex& a,
			const CTR_TaggedIndex& b
		) const {
			return (int(a) > int(b));
		}
	};
	
	
private :
	CTR_TaggedIndex(
		const CTR_TaggedIndex *index
	) {}; 

	unsigned int m_val;


};			

#endif

