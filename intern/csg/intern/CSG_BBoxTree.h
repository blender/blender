/**
 * I've modified some very nice bounding box tree code from 
 * Gino van der Bergen's Free Solid Library below. It's basically
 * the same code - but I've hacked out the transformation stuff as
 * I didn't understand it. I've also made it far less elegant!
 *
 */

/*
  SOLID - Software Library for Interference Detection
  Copyright (C) 1997-1998  Gino van den Bergen

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  Please send remarks, questions and bug reports to gino@win.tue.nl,
  or write to:
                  Gino van den Bergen
		  Department of Mathematics and Computing Science
		  Eindhoven University of Technology
		  P.O. Box 513, 5600 MB Eindhoven, The Netherlands
*/

#ifndef _BBOXTREE_H_
#define _BBOXTREE_H_

#include "CSG_BBox.h"
#include "MT_Line3.h"
#include "CSG_IndexDefs.h"

#include <vector>

/**
 * Tree structure 
 */

class BBoxNode 
{
public:
  enum TagType { LEAF, INTERNAL };

  BBox m_bbox;
  TagType m_tag;
};


class BBoxLeaf : public BBoxNode 
{
public:
  int m_polyIndex;

  BBoxLeaf() {}

  BBoxLeaf(int polyIndex, const BBox& bbox) 
  : m_polyIndex(polyIndex)
  { 
	m_bbox = bbox;
    m_tag = LEAF;
  }

};


typedef BBoxLeaf* LeafPtr;
typedef BBoxNode* NodePtr;


class BBoxInternal : public BBoxNode 
{
public:
	NodePtr lson;
	NodePtr rson;

	BBoxInternal() {}
	BBoxInternal(
		int n, LeafPtr leafIt
	);

};

typedef BBoxInternal* InternalPtr;


class BBoxTree
{
public :
	BBoxTree() {};

	const NodePtr RootNode() const {
		return m_internals;
	}
	
	~BBoxTree() {
		delete[] m_leaves;
		delete[] m_internals;
	}

	// tree takes ownership of the leaves.
	void BuildTree(LeafPtr leaves, int numLeaves);
		
private :

	void RecursiveTreeBuild(
		int n, LeafPtr leafIt
	);

	int m_branch;

	LeafPtr m_leaves;
	InternalPtr m_internals;
	int m_numLeaves;
};	





#endif






