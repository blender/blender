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
 *
 * @author Laurence
 * @mainpage CTR_UHeap an updatable heap template class (also
 * known as an updatable priority queue)
 *
 * Todo: Make CTR_UHeapable a template class with m_key the
 * template parameter, so that arbitrary value types with
 * operators (=,>) defined can be used.
 *
 */

#ifndef NAN_INCLUDED_CTR_UHeap_h
#define NAN_INCLUDED_CTR_UHeap_h

#include <vector>

#include "MEM_NonCopyable.h"

class CTR_UHeapable {

public :
		int &
	HeapPos(
	){
		return m_ind;
	};
		float &
	HeapKey(
	) {
		return m_key;
	};

	const 
		float &
	HeapKey(
	) const {
		return m_key;
	};

	const 
		int &
	HeapPos(
	) const {
		return m_ind;
	};

private :

	float m_key;
	int m_ind;

protected :

	CTR_UHeapable(
	) : m_key (0),
		m_ind (0)
	{
	};

	~CTR_UHeapable(
	){
	};
};
	
template <class HeapType> 	
class CTR_UHeap : public MEM_NonCopyable
{

public:		

	static
		CTR_UHeap *
	New(
	) {
		return new CTR_UHeap();
	}

		void
	MakeHeap(
		HeapType *base
	) {
		int i;
		int start = Parent(m_vector.size()-1);
		for (i = start; i >=0; --i) {
			DownHeap(base,i);
		}
	}; 
	
		void
	Insert(
		HeapType *base,
		int elem
	) {
		// add element to vector
		m_vector.push_back(elem);
		base[elem].HeapPos() = m_vector.size()-1;

		// push the element up the heap
		UpHeap(base,m_vector.size()-1);
	}

	// access to the vector for initial loading of elements

		std::vector<int> &
	HeapVector(
	) {
		return m_vector;
	};


		void
	Remove(
		HeapType *base,
		int i
	) {
	
		// exchange with last element - pop last
		// element and move up or down the heap as appropriate
		if (m_vector.empty()) {
			assert(false);
		}

		if (i  != int(m_vector.size())-1) {
			
			Swap(base,i,m_vector.size() - 1);
			m_vector.pop_back();

			if (!m_vector.empty()) {
				UpHeap(base,i);
				DownHeap(base,i);
			}
		} else {
			m_vector.pop_back();
		}
	}

		int
	Top(
	) const {
		if (m_vector.empty()) return -1;
		return m_vector[0];
	}
		

		void
	SC_Heap(
		HeapType *base
	) {
		int i;
		for (i = 1; i < int(m_vector.size()) ; i++) {
			
			CTR_UHeapable * elem = base + m_vector[i];			
			CTR_UHeapable * p_elem = base + m_vector[Parent(i)];			

			assert(p_elem->HeapKey() >= elem->HeapKey());
			assert(elem->HeapPos() == i);
		}

	};
			

	~CTR_UHeap(
	) {
	};	


private:

	CTR_UHeap(
	) {
	};


	std::vector<int> m_vector;
		
private:
		void 
	Swap(
		HeapType *base,
		int i, 
		int j
	){
		std::swap(m_vector[i],m_vector[j]);
		
		CTR_UHeapable *heap_i = base + m_vector[i];
		CTR_UHeapable *heap_j = base + m_vector[j];
		
		// Exchange heap positions
		heap_i->HeapPos() = i;
		heap_j->HeapPos() = j;
	}

		int 
	Parent(
		unsigned int i
	) {
		 return (i-1) >> 1; 
	}
		int 
	Left(
		int i
	) {
		return (i<<1)+1; 
	}

		int 
	Right(
		int i
	) {
		return (i<<1)+2;
	}

		float
	HeapVal(
		HeapType *base,
		int i
	) {
		return base[m_vector[i]].HeapKey();
	}

		void
	DownHeap(
		HeapType *base,		
		int i
	) {
		int heap_size = m_vector.size();

		int l = Left(i);
		int r = Right(i);

		int largest;
		if (l < heap_size && HeapVal(base,l) > HeapVal(base,i)) {
			largest = l;
		} else {
			largest = i;
		}

		if (r < heap_size && HeapVal(base,r) > HeapVal(base,largest)) {
			largest = r;
		}	

		if (largest != i) {
			// exchange i and largest
			Swap(base,i,largest);
			DownHeap(base,largest);
		}
	}

		void
	UpHeap(
		HeapType *base,
		int i
	) {

		// swap parents untill it's found a place in the heap < it's parent or
		// top of heap

		while (i > 0) {
			int p = Parent(i);
			if (HeapVal(base,i) < HeapVal(base,p)) {
				break;
			}
			Swap(base,p,i);
			i = p;
		}
	}		
};

#endif

