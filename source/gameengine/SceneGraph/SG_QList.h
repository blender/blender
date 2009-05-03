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
		iterator(SG_QList& head) : m_head(head), m_current(NULL) {}
		~iterator() {}

		void begin()
		{
			m_current = (T*)m_head.QPeek();
			if (m_current == (T*)m_head.Self())
			{
				m_current = NULL;
			}
		}
		bool end()
		{
			return (NULL == m_current);
		}
		T* operator*()
		{
			return m_current;
		}
		_myT& operator++()
		{
			assert(m_current);
			m_current = (T*)m_current->QPeek();
			if (m_current == (T*)m_head.Self())
			{
				m_current = NULL;
			}
			return *this;
		}
	};

	SG_QList() : SG_DList()
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
    void QDelink()             // Remove from the middle
    {
		if (!QEmpty())
		{
			m_bqlink->m_fqlink = m_fqlink;
			m_fqlink->m_bqlink = m_bqlink;
			m_fqlink = m_bqlink = this;
		}
    }
    inline SG_QList *QPeek()			// Look at front without removing
    { 
        return m_fqlink; 
    }  
};

#endif //__SG_QLIST

