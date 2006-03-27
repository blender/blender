/*
Copyright (c) 2003-2006 Gino van den Bergen / Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
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



