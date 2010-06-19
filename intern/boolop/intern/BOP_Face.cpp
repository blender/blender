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
 
#include "BOP_Face.h"

/******************************************************************************/
/*** BOP_Face                                                               ***/
/******************************************************************************/

/**
 * Constructs a new face.
 * @param plane face plane
 * @param originalFace index of the original face
 */
BOP_Face::BOP_Face(MT_Plane3 plane, BOP_Index originalFace)
{
	m_plane        = plane;
	m_tag          = UNCLASSIFIED;
	m_originalFace = originalFace;
	m_split        = 0;
	m_bbox         = NULL;
}

/**
 * Inverts this face.
 */
void BOP_Face::invert()
{
	getPlane().Invert();
	BOP_Index aux = m_indexs[0];
	m_indexs[0] = m_indexs[2];
	m_indexs[2] = aux;
}

/******************************************************************************/
/*** BOP_Face                                                              ***/
/******************************************************************************/

/**
 * Constructs a new triangle face.
 * @param v1 vertex index
 * @param v2 vertex index
 * @param v3 vertex index
 * @param plane face plane
 * @param originalFace index of the original face
 */
BOP_Face3::BOP_Face3(BOP_Index v1, BOP_Index v2, BOP_Index v3, MT_Plane3 plane, BOP_Index originalFace): BOP_Face(plane,originalFace) 
{
	m_indexs[0] = v1;
	m_indexs[1] = v2;
	m_indexs[2] = v3;	
	m_size = 3;
}

/**
 * Returns the relative edge index (1,2,3) for the specified vertex indexs.
 * @param v1 vertex index
 * @param v2 vertex index
 * @param e relative edge index (1,2,3)
 * @return true if (v1,v2) is an edge of this face, false otherwise
 */
bool BOP_Face3::getEdgeIndex(BOP_Index v1, BOP_Index v2, unsigned int &e)
{
	if (m_indexs[0] == v1) {
		if (m_indexs[1] == v2) {
			e = 1;
		}
		else if (m_indexs[2] == v2) {
			e = 3;
		}
		else
		  return false;
	}
	else if (m_indexs[1] == v1) {
		if (m_indexs[0] == v2) {
			e = 1;
		}
		else if (m_indexs[2] == v2) {
			e = 2;
		}
		else
		  return false;
	}
	else if (m_indexs[2] == v1) {
		if (m_indexs[0] == v2) {
			e = 3;
		}
		else if (m_indexs[1] == v2) {
			e = 2;
		}
		else
		  return false;
	}else {
	  return false;
	}
	
	return  true;
} 

/**
 * Returns if this face contains the specified vertex index.
 * @param v vertex index
 * @return true if this face contains the specified vertex index, false otherwise
 */
bool BOP_Face3::containsVertex(BOP_Index v) 
{
	return (m_indexs[0] == v || m_indexs[1] == v || m_indexs[2] == v);
}

/**
 * Returns the neighbours of the specified vertex index.
 * @param v vertex index
 * @param prev previous vertex index
 * @param next next vertex index
 * @return true if this face contains the vertex index v, false otherwise
 */
bool BOP_Face3::getNeighbours(BOP_Index v, BOP_Index &prev, BOP_Index &next) 
{
	if (m_indexs[0] == v) {
	  prev = m_indexs[2];
	  next = m_indexs[1];
	}
	else if (m_indexs[1] == v) {
	  prev = m_indexs[0];
	  next = m_indexs[2];
	}
        else if (m_indexs[2] == v) {
	  prev = m_indexs[1];
	  next = m_indexs[0];
	}
	else return false;
	
	return true;
}

/**
 * Returns the previous neighbour of the specified vertex index.
 * @param v vertex index
 * @param w previous vertex index
 * @return true if this face contains the specified vertex index, false otherwise
 */
bool BOP_Face3::getPreviousVertex(BOP_Index v, BOP_Index &w) 
{
	if (m_indexs[0] == v) w = m_indexs[2];
	else if (m_indexs[1] == v) w = m_indexs[0];
	else if (m_indexs[2] == v) w = m_indexs[1];
	else return false;
	
	return true;
}

/**
 * Returns the next neighbour of the specified vertex index.
 * @param v vertex index
 * @param w vertex index
 * @return true if this face contains the specified vertex index, false otherwise
 */
bool BOP_Face3::getNextVertex(BOP_Index v, BOP_Index &w) 
{
	if (m_indexs[0] == v) w = m_indexs[1];
	else if (m_indexs[1] == v) w = m_indexs[2];
	else if (m_indexs[2] == v) w = m_indexs[0];
	else return false;
	
	return true;
} 

/**
 * Replaces a face vertex index.
 * @param oldIndex old vertex index
 * @param newIndex new vertex index
 */
void BOP_Face3::replaceVertexIndex(BOP_Index oldIndex, BOP_Index newIndex)
{
	/* if the old index really exists, and new index also exists already,
	 * don't create an edge with both vertices == newIndex */

	if( (m_indexs[0] == oldIndex || m_indexs[1] == oldIndex || m_indexs[2] == oldIndex) &&
			(m_indexs[0] == newIndex || m_indexs[1] == newIndex || m_indexs[2] == newIndex) ) {
		setTAG(BROKEN);
	}

	if (m_indexs[0] == oldIndex) m_indexs[0] = newIndex;
	else if (m_indexs[1] == oldIndex) m_indexs[1] = newIndex;
	else if (m_indexs[2] == oldIndex) m_indexs[2] = newIndex;
}

/******************************************************************************/
/*** BOP_Face4                                                              ***/
/******************************************************************************/

/**
 * Constructs a new quad face.
 * @param v1 vertex index
 * @param v2 vertex index
 * @param v3 vertex index
 * @param v4 vertex index
 * @param plane face plane
 * @param originalFace index of the original face
 */
BOP_Face4::BOP_Face4(BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index v4, MT_Plane3 plane, 
					 BOP_Index originalFace): 
					 BOP_Face(plane,originalFace) 
{
	m_indexs[0] = v1;
	m_indexs[1] = v2;
	m_indexs[2] = v3;
	m_indexs[3] = v4;
	
	m_size = 4;
}

/**
 * Returns if this face contains the specified vertex index.
 * @param v vertex index
 * @return true if this face contains the specified vertex index, false otherwise
 */
bool BOP_Face4::containsVertex(BOP_Index v) 
{
	return (m_indexs[0] == v || m_indexs[1] == v || m_indexs[2] == v || m_indexs[3]==v);
}

/**
 * Returns the neighbours of the specified vertex index.
 * @param v vertex index
 * @param prev previous vertex index
 * @param next next vertex index
 * @param opp opposite vertex index
 * @return true if this face contains the vertex index v, false otherwise
 */
bool BOP_Face4::getNeighbours(BOP_Index v, BOP_Index &prev, BOP_Index &next, BOP_Index &opp) 
{
	if (m_indexs[0] == v) {
	  prev = m_indexs[3];
	  next = m_indexs[1];
	  opp = m_indexs[2];
	}
	else if (m_indexs[1] == v) {
	  prev = m_indexs[0];
	  next = m_indexs[2];
	  opp = m_indexs[3];
	}
	else if (m_indexs[2] == v) {
	  prev = m_indexs[1];
	  next = m_indexs[3];
	  opp = m_indexs[0];
	}
	else if (m_indexs[3] == v) {
	  prev = m_indexs[2];
	  next = m_indexs[0];
	  opp = m_indexs[1];
	}
	else return false;

	return true;
}

/**
 * Returns the previous neighbour of the specified vertex index.
 * @param v vertex index
 * @param w previous vertex index
 * @return true if this face contains the specified vertex index, false otherwise
 */
bool BOP_Face4::getPreviousVertex(BOP_Index v, BOP_Index &w) 
{
	if (m_indexs[0] == v) w = m_indexs[3];
	else if (m_indexs[1] == v) w = m_indexs[0];
	else if (m_indexs[2] == v) w = m_indexs[1];
	else if (m_indexs[3] == v) w = m_indexs[2];
	else return false;

	return true;
}

/**
 * Returns the next neighbour of the specified vertex index.
 * @param v vertex index
 * @param w next vertex index
 * @return true if this face contains the specified vertex index, false otherwise
 */
bool BOP_Face4::getNextVertex(BOP_Index v, BOP_Index &w) 
{
	if (m_indexs[0] == v) w = m_indexs[1];
	else if (m_indexs[1] == v) w = m_indexs[2];
	else if (m_indexs[2] == v) w = m_indexs[3];
	else if (m_indexs[3] == v) w = m_indexs[0];
	else return false;

	return true;
} 

/**
 * Returns the opposite  neighbour of the specified vertex index.
 * @param v vertex index
 * @param w opposite vertex index
 * @return true if this face contains the specified vertex index, false otherwise
 */
bool BOP_Face4::getOppositeVertex(BOP_Index v, BOP_Index &w)
{
	if (m_indexs[0] == v) 
		w = m_indexs[2];
	else if (m_indexs[1] == v) 
		w = m_indexs[3];
	else if (m_indexs[2] == v) 
		w = m_indexs[0];
	else if (m_indexs[3] == v) 
		w = m_indexs[1];
	else
	  return false;

	return true;
}

/**
 * Replaces a face vertex index.
 * @param oldIndex old vertex index
 * @param newIndex new vertex index
 */
void BOP_Face4::replaceVertexIndex(BOP_Index oldIndex, BOP_Index newIndex)
{
	if (m_indexs[0] == oldIndex) m_indexs[0] = newIndex;
	else if (m_indexs[1] == oldIndex) m_indexs[1] = newIndex;
	else if (m_indexs[2] == oldIndex) m_indexs[2] = newIndex;
	else if (m_indexs[3] == oldIndex) m_indexs[3] = newIndex;
}

/**
 * Returns the relative edge index (1,2,3,4) for the specified vertex indexs.
 * @param v1 vertex index
 * @param v2 vertex index
 * @param e relative edge index (1,2,3,4)
 * @return true if (v1,v2) is an edge of this face, false otherwise
 */
bool BOP_Face4::getEdgeIndex(BOP_Index v1, BOP_Index v2, unsigned int &e)
{
	if (m_indexs[0] == v1) {
		if (m_indexs[1] == v2) {
			e = 1;
		}
		else if (m_indexs[3] == v2) {
			e = 4;
		}
		else
		  return false;
	}
	else if (m_indexs[1] == v1) {
		if (m_indexs[0] == v2) {
			e = 1;
		}
		else if (m_indexs[2] == v2) {
			e = 2;
		}
		else
		  return false;
	}
	else if (m_indexs[2] == v1) {
		if (m_indexs[1] == v2) {
			e = 2;
		}
		else if (m_indexs[3] == v2) {
			e = 3;
		}
		else
		  return false;
	}
	else if (m_indexs[3] == v1) {
		if (m_indexs[2] == v2) {
			e = 3;
		}
		else if (m_indexs[0] == v2) {
			e = 4;
		}
		else
		  return false;
	}
	else return false;
	
	return  true;
}  

#ifdef BOP_DEBUG
/**
 * Implements operator <<.
 */
ostream &operator<<(ostream &stream, BOP_Face *f)
{
	char aux[20];
	BOP_stringTAG(f->m_tag,aux);
	if (f->size()==3) {
		stream << "Face[" << f->getVertex(0) << "," << f->getVertex(1) << ",";
		stream << f->getVertex(2) << "] ("  <<  aux  <<  ") <-- " << f->m_originalFace;
	}
	else {
		stream << "Face[" << f->getVertex(0) << "," << f->getVertex(1) << ",";
		stream << f->getVertex(2) << "," << f->getVertex(3) << "] ("  <<  aux;
		stream <<  ") <-- " << f->m_originalFace;
	}

	return stream;
}
#endif
