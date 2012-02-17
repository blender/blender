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

/** \file boolop/intern/BOP_Edge.h
 *  \ingroup boolopintern
 */

 
#ifndef __BOP_EDGE_H__
#define __BOP_EDGE_H__

#include "BOP_Indexs.h"
#include "BOP_Misc.h"

class BOP_Edge
{
private:
	BOP_Index  m_vertexs[2];
	BOP_Indexs m_faces;
#ifdef BOP_NEW_MERGE
	bool m_used;
#endif

	bool containsFace(BOP_Index i);

public:
	BOP_Edge(BOP_Index v1, BOP_Index v2);
	inline BOP_Index getVertex1() { return m_vertexs[0];};
	inline BOP_Index getVertex2() { return m_vertexs[1];};
	void replaceVertexIndex(BOP_Index oldIndex, BOP_Index newIndex);
	inline BOP_Index getFace(unsigned int i){return m_faces[i];};
	inline unsigned int getNumFaces(){return m_faces.size();};
	inline BOP_Indexs &getFaces(){return m_faces;};
	void addFace(BOP_Index face);
#ifdef BOP_NEW_MERGE
	bool removeFace(BOP_Index i);
	bool getUsed() { return m_used;};
	void setUsed(bool setting) { m_used=setting;};
#endif
#ifdef BOP_DEBUG
	friend ostream &operator<<(ostream &stream, BOP_Edge *e);
#endif

};

#endif
