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
 
#ifndef BOP_FACE_H
#define BOP_FACE_H

#include "BOP_Tag.h"
#include "MT_Plane3.h"
#include "BOP_Indexs.h"
#include "BOP_BBox.h"
#include "BOP_Misc.h"
#include <iostream>
#include <vector>
using namespace std;

class BOP_Face;

typedef vector<BOP_Face *> BOP_Faces;
typedef vector<BOP_Face *>::iterator BOP_IT_Faces;

class BOP_Face
{
private:
	BOP_TAG      m_tag;
	MT_Plane3    m_plane;
	BOP_Index    m_originalFace;

protected:
	BOP_Index    m_indexs[4];
	unsigned int m_size;
	unsigned int m_split;
	BOP_BBox     *m_bbox;

public:
	BOP_Face(MT_Plane3 plane, BOP_Index originalFace);
	virtual ~BOP_Face(){if (m_bbox) delete m_bbox;};
	inline MT_Plane3 getPlane() const {return m_plane;};
	inline void setPlane(const MT_Plane3 plane) {m_plane = plane;};
	inline BOP_TAG getTAG() const {return m_tag;};
	inline void setTAG(const BOP_TAG t) {m_tag = t;};
	inline BOP_Index getOriginalFace() const {return m_originalFace;};
	inline void setOriginalFace(const BOP_Index originalFace) {m_originalFace=originalFace;};
	inline BOP_Index getVertex(unsigned int i) const {return m_indexs[i];};
	inline void setVertex(const BOP_Index idx, const BOP_Index i) {m_indexs[idx]=i;};
	inline unsigned int getSplit() const {return m_split;};
	inline void setSplit(const unsigned int i) {m_split=i;};

	void invert();
	inline void setBBox(const MT_Point3& p1,const MT_Point3& p2,const MT_Point3& p3) {
    m_bbox = new BOP_BBox(p1, p2, p3);};
	inline BOP_BBox *getBBox() {return m_bbox;};
	inline void freeBBox(){if (m_bbox!=NULL) {delete m_bbox; m_bbox=NULL;} };

	inline unsigned int size() const {return m_size;};
	
	virtual bool getEdgeIndex(BOP_Index v1, BOP_Index v2, unsigned int &e) = 0;
	virtual void replaceVertexIndex(BOP_Index oldIndex, BOP_Index newIndex) = 0;
	virtual bool containsVertex(BOP_Index v) = 0;
		
#ifdef BOP_DEBUG
	friend ostream &operator<<(ostream &stream, BOP_Face *f);
#endif
};

class BOP_Face3: public BOP_Face 
{
public:
	BOP_Face3(BOP_Index i, BOP_Index j, BOP_Index k, MT_Plane3 p, BOP_Index originalFace);
	bool getEdgeIndex(BOP_Index v1, BOP_Index v2, unsigned int &e);
	void replaceVertexIndex(BOP_Index oldIndex, BOP_Index newIndex);
	bool containsVertex(BOP_Index v);

	bool getNeighbours(BOP_Index v, BOP_Index &prev, BOP_Index &next);
	bool getPreviousVertex(BOP_Index v, BOP_Index &w);
	bool getNextVertex(BOP_Index v, BOP_Index &w);
};

class BOP_Face4: public BOP_Face 
{
public:
	BOP_Face4(BOP_Index i, BOP_Index j, BOP_Index k, BOP_Index l, MT_Plane3 p, BOP_Index originalFace);
	bool getEdgeIndex(BOP_Index v1, BOP_Index v2, unsigned int &e);
	void replaceVertexIndex(BOP_Index oldIndex, BOP_Index newIndex);
	bool containsVertex(BOP_Index v);

	bool getNeighbours(BOP_Index v, BOP_Index &prev, BOP_Index &next, BOP_Index &opp);
	bool getPreviousVertex(BOP_Index v, BOP_Index &w);
	bool getNextVertex(BOP_Index v, BOP_Index &w);
	bool getOppositeVertex(BOP_Index v, BOP_Index &w);
};

#endif
