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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "CTR_List.h"


CTR_Link::
CTR_Link(
) : 
	m_next(0), 
	m_prev(0) 
{
}

CTR_Link::
CTR_Link(
	CTR_Link *next,
	CTR_Link *prev
) : 
	m_next(next), 
	m_prev(prev) 
{
}

	CTR_Link *
CTR_Link::
getNext(
) const {
	return m_next; 
}

	CTR_Link *
CTR_Link::
getPrev(
) const { 
	return m_prev; 
}  

	bool 
CTR_Link::
isHead(
) const { 
	return m_prev == 0; 
}

	bool 
CTR_Link::
isTail(
) const { 
	return m_next == 0; 
}

	void 
CTR_Link::
insertBefore(
	CTR_Link *link
) {
    m_next         = link;
    m_prev         = link->m_prev;
    m_next->m_prev = this;
    m_prev->m_next = this;
} 

	void 
CTR_Link::
insertAfter(
	CTR_Link *link
) {
    m_next         = link->m_next;
    m_prev         = link;
    m_next->m_prev = this;
    m_prev->m_next = this;
} 

	void 
CTR_Link::
remove(
) { 
    m_next->m_prev = m_prev; 
    m_prev->m_next = m_next;
}


CTR_List::
CTR_List(
) : 
	m_head(&m_tail, 0), 
	m_tail(0, &m_head) 
{
}

	CTR_Link *
CTR_List::
getHead(
) const { 
	return m_head.getNext(); 
}

	CTR_Link *
CTR_List::
getTail(
) const { 
	return m_tail.getPrev();
} 

	void 
CTR_List::
addHead(
	CTR_Link *link
) { 
	link->insertAfter(&m_head); 
}

	void 
CTR_List::
addTail(
	CTR_Link *link
) { 
	link->insertBefore(&m_tail); 
}

