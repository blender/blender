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

#include "CSG_BBoxTree.h"
#include <algorithm>


using namespace std;


BBoxInternal::
BBoxInternal(
	int n, LeafPtr leafIt
)
{
	m_tag = INTERNAL;
	m_bbox.SetEmpty();

	int i;
	for (i=0;i<n;i++) {
		m_bbox.Include(leafIt[i].m_bbox);
	}
}
// Construct a BBoxInternal from a list of BBoxLeaf nodes.
// Recursive function that does the longest axis median point 
// fit.

void BBoxTree::
BuildTree(
	LeafPtr leaves, int numLeaves
) {
	m_branch = 0;
	m_leaves = leaves;
	m_numLeaves = numLeaves;
	m_internals = new BBoxInternal[numLeaves];

	RecursiveTreeBuild(m_numLeaves,m_leaves);
}

	void 
BBoxTree::
RecursiveTreeBuild(
	int n, LeafPtr leafIt
){				
	m_internals[m_branch] = BBoxInternal(n,leafIt);
	BBoxInternal& aBBox  = m_internals[m_branch];

	m_branch++;

	int axis = aBBox.m_bbox.LongestAxis();
	int i = 0, mid = n;

	// split the leaves into two groups those that are < bbox.getCenter()[axis]
	// and those that are >= 
	// smart bit about this code is it does the grouping in place.
	while (i < mid) 
	{
		if (leafIt[i].m_bbox.Center()[axis] < aBBox.m_bbox.Center()[axis])
		{
			++i;
		} else {
			--mid;
			swap(leafIt[i], leafIt[mid]);
		}
	}
	
	// all of the nodes were on one side of the box centre
	// I'm not sure if this case ever gets reached?	
	if (mid == 0 || mid == n) 
	{
		mid = n / 2;
	}
  
	if (mid >= 2) 
	{
		aBBox.rson = m_internals + m_branch;
		RecursiveTreeBuild(mid,leafIt);
	} else {
		aBBox.rson = leafIt;
	}
	if (n - mid >= 2) {
		aBBox.lson = m_internals + m_branch;
		RecursiveTreeBuild(n - mid, leafIt + mid);
	} else {
		aBBox.lson = leafIt + mid;
	}
}







	







