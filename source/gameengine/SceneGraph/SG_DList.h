/**
 * $Id$
 *
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
#ifndef __SG_DLIST
#define __SG_DLIST

#include <stdlib.h>

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

/**
 * Double circular linked list
 */
class SG_DList
{
protected :
	SG_DList* m_flink;
	SG_DList* m_blink;

public:
	template<typename T> class iterator
	{
	private:
		SG_DList&	m_head;
		T*			m_current;
	public:
		typedef iterator<T> _myT;
		iterator(SG_DList& head) : m_head(head), m_current(NULL) {}
		~iterator() {}

		void begin()
		{
			m_current = (T*)m_head.Peek();
		}
		void back()
		{
			m_current = (T*)m_head.Back();
		}
		bool end()
		{
			return (m_current == (T*)m_head.Self());
		}
		bool add_back(T* item)
		{
			return m_current->AddBack(item);
		}
		T* operator*()
		{
			return m_current;
		}
		_myT& operator++()
		{
			// no check of NULL! make sure you don't try to increment beyond end
			m_current = (T*)m_current->Peek();
			return *this;
		}
		_myT& operator--()
		{
			// no check of NULL! make sure you don't try to increment beyond end
			m_current = (T*)m_current->Back();
			return *this;
		}
	};

    SG_DList() 
    { 
        m_flink = m_blink = this; 
    }
	SG_DList(const SG_DList& other)
	{
        m_flink = m_blink = this; 
	}
    virtual ~SG_DList() 
    {
		Delink();
    }

    inline bool Empty()               // Check for empty queue
    {     
        return ( m_flink == this ); 
    }
    bool AddBack( SG_DList *item )  // Add to the back
    {
		if (!item->Empty())
			return false;
        item->m_blink = m_blink;
        item->m_flink = this;
        m_blink->m_flink = item;
        m_blink = item;
		return true;
    }
    bool AddFront( SG_DList *item )  // Add to the back
    {
		if (!item->Empty())
			return false;
        item->m_flink = m_flink;
        item->m_blink = this;
        m_flink->m_blink = item;
        m_flink = item;
		return true;
    }
    SG_DList *Remove()           // Remove from the front
    {
        if (Empty()) 
        {
            return NULL;
        }
        SG_DList* item = m_flink;
        m_flink = item->m_flink;
        m_flink->m_blink = this;
        item->m_flink = item->m_blink = item;
        return item;
    }
    bool Delink()             // Remove from the middle
    {
		if (Empty())
			return false;
		m_blink->m_flink = m_flink;
		m_flink->m_blink = m_blink;
		m_flink = m_blink = this;
		return true;
    }
    inline SG_DList *Peek()			// Look at front without removing
    { 
        return m_flink; 
    }  
    inline SG_DList *Back()			// Look at front without removing
    { 
        return m_blink; 
    }  
    inline SG_DList *Self() 
    { 
        return this; 
    }
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:SG_DList"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__SG_DLIST

