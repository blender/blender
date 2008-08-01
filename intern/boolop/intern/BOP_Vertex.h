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
 
#ifndef BOP_VERTEX_H
#define BOP_VERTEX_H

#include "BOP_Tag.h"
#include "BOP_Indexs.h"
#include "MT_Point3.h"
#include "BOP_Misc.h"

class BOP_Vertex 
{
private:
	MT_Point3 m_point;
	BOP_Indexs  m_edges;
	BOP_TAG m_tag;
	
	bool containsEdge(BOP_Index i);

public:
	BOP_Vertex(double x, double y, double z);
	BOP_Vertex(MT_Point3 d);
	void addEdge(BOP_Index i);
	void removeEdge(BOP_Index i);
	inline BOP_Index getEdge(unsigned int i) { return m_edges[i];};
	inline unsigned int getNumEdges() { return m_edges.size();};
	inline BOP_Indexs &getEdges() { return m_edges;};
	inline MT_Point3 getPoint() const { return m_point;};
	inline BOP_TAG getTAG() { return m_tag;};
	inline void setTAG(BOP_TAG t) { m_tag = t;};
#ifdef BOP_DEBUG
	friend ostream &operator<<(ostream &stream, BOP_Vertex *v);
#endif

};

#endif
