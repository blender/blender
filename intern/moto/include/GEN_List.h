/*
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

/** \file moto/include/GEN_List.h
 *  \ingroup moto
 */


#ifndef GEN_LIST_H
#define GEN_LIST_H

class GEN_Link {
public:
    GEN_Link() : m_next(0), m_prev(0) {}
    GEN_Link(GEN_Link *next, GEN_Link *prev) : m_next(next), m_prev(prev) {}
    
    GEN_Link *getNext() const { return m_next; }  
    GEN_Link *getPrev() const { return m_prev; }  

    bool isHead() const { return m_prev == 0; }
    bool isTail() const { return m_next == 0; }

    void insertBefore(GEN_Link *link) {
        m_next         = link;
        m_prev         = link->m_prev;
        m_next->m_prev = this;
        m_prev->m_next = this;
    } 

    void insertAfter(GEN_Link *link) {
        m_next         = link->m_next;
        m_prev         = link;
        m_next->m_prev = this;
        m_prev->m_next = this;
    } 

    void remove() { 
        m_next->m_prev = m_prev; 
        m_prev->m_next = m_next;
    }

private:  
    GEN_Link  *m_next;
    GEN_Link  *m_prev;
};

class GEN_List {
public:
    GEN_List() : m_head(&m_tail, 0), m_tail(0, &m_head) {}

    GEN_Link *getHead() const { return m_head.getNext(); } 
    GEN_Link *getTail() const { return m_tail.getPrev(); } 

    void addHead(GEN_Link *link) { link->insertAfter(&m_head); }
    void addTail(GEN_Link *link) { link->insertBefore(&m_tail); }
    
private:
    GEN_Link m_head;
    GEN_Link m_tail;
};

#endif

