/**
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
 
#include "BOP_Edge.h"

/**
 * Constructs a new edge.
 * @param v1 vertex index
 * @param v2 vertex index
 */
BOP_Edge::BOP_Edge(BOP_Index v1, BOP_Index v2) 
{
	m_vertexs[0] = v1;
	m_vertexs[1] = v2;
}

/**
 * Adds a new face index to this edge.
 * @param i face index
 */
void BOP_Edge::addFace(BOP_Index i) 
{
	if (!containsFace(i))
		m_faces.push_back(i);
}

/**
 * Returns if this edge contains the specified face index.
 * @param i face index
 * @return true if this edge contains the specified face index, false otherwise
 */
bool BOP_Edge::containsFace(BOP_Index i)
{
	int pos=0;
	for(BOP_IT_Indexs it = m_faces.begin();it!=m_faces.end();pos++,it++) {
		if ((*it) == i)
		return true;
	}
	
	return false;
}

/**
 * Replaces an edge vertex index.
 * @param oldIndex old vertex index
 * @param newIndex new vertex index
 */
void BOP_Edge::replaceVertexIndex(BOP_Index oldIndex, BOP_Index newIndex) 
{
	if (m_vertexs[0] == oldIndex) m_vertexs[0] = newIndex;
	else if (m_vertexs[1] == oldIndex) m_vertexs[1] = newIndex;
}

#ifdef BOP_NEW_MERGE

/**
 * Returns if this edge contains the specified face index.
 * @param i face index
 * @return true if this edge contains the specified face index, false otherwise
 */
bool BOP_Edge::removeFace(BOP_Index i)
{
	int pos=0;
	for(BOP_IT_Indexs it = m_faces.begin();it!=m_faces.end();pos++,it++) {
		if ((*it) == i) {
			m_faces.erase(it);
			return true;
		}
	}
	
	return false;
}

#endif

#ifdef BOP_DEBUG

#include <iostream>
using namespace std;

/**
 * Implements operator <<.
 */
ostream &operator<<(ostream &stream, BOP_Edge *e)
{
	stream << "Edge[" << e->getVertex1() << "," << e->getVertex2();
#ifdef BOP_NEW_MERGE
	if(e->m_used)
		stream << "] (used)";
	else
		stream << "] (unused)";
#endif
	return stream;
}
#endif


