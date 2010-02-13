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
#ifndef __SG_QLIST
#define __SG_QLIST

#include "SG_DList.h"

/**
 * Double-Double circular linked list
 * For storing an object is two lists simultaneously
 */
class SG_QList : public SG_DList
{
protected :
	SG_QList* m_fqlink;
	SG_QList* m_bqlink;

public:
	template<typename T> class iterator
	{
	private:
		SG_QList&	m_head;
		T*			m_current;
	public:
		typedef iterator<T> _myT;
		iterator(SG_QList& head, SG_QList* current=NULL) : m_head(head) { m_current = (T*)current; }
		~iterator() {}

		void begin()
		{
			m_current = (T*)m_head.QPeek();
		}
		void back()
		{
			m_current = (T*)m_head.QBack();
		}
		bool end()
		{
			return (m_current == (T*)m_head.Self());
		}
		bool add_back(T* item)
		{
			return m_current->QAddBack(item);
		}
		T* operator*()
		{
			return m_current;
		}
		_myT& operator++()
		{
			m_current = (T*)m_current->QPeek();
			return *this;
		}
		_myT& operator--()
		{
			// no check on NULL! make sure you don't try to increment beyond end
			m_current = (T*)m_current->QBack();
			return *this;
		}
	};

	SG_QList() : SG_DList()
    { 
        m_fqlink = m_bqlink = this; 
    }
	SG_QList(const SG_QList& other) : SG_DList()
	{
        m_fqlink = m_bqlink = this; 
	}
    virtual ~SG_QList() 
    {
		QDelink();
    }

    inline bool QEmpty()               // Check for empty queue
    {     
        return ( m_fqlink == this ); 
    }
    bool QAddBack( SG_QList *item )  // Add to the back
    {
		if (!item->QEmpty())
			return false;
        item->m_bqlink = m_bqlink;
        item->m_fqlink = this;
        m_bqlink->m_fqlink = item;
        m_bqlink = item;
		return true;
    }
    bool QAddFront( SG_QList *item )  // Add to the back
    {
		if (!item->Empty())
			return false;
        item->m_fqlink = m_fqlink;
        item->m_bqlink = this;
        m_fqlink->m_bqlink = item;
        m_fqlink = item;
		return true;
    }
    SG_QList *QRemove()           // Remove from the front
    {
        if (QEmpty()) 
        {
            return NULL;
        }
        SG_QList* item = m_fqlink;
        m_fqlink = item->m_fqlink;
        m_fqlink->m_bqlink = this;
        item->m_fqlink = item->m_bqlink = item;
        return item;
    }
    bool QDelink()             // Remove from the middle
    {
		if (QEmpty())
			return false;
		m_bqlink->m_fqlink = m_fqlink;
		m_fqlink->m_bqlink = m_bqlink;
		m_fqlink = m_bqlink = this;
		return true;
    }
    inline SG_QList *QPeek()			// Look at front without removing
    { 
        return m_fqlink; 
    }  
    inline SG_QList *QBack()			// Look at front without removing
    { 
        return m_bqlink; 
    }  
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:SG_QList"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__SG_QLIST

