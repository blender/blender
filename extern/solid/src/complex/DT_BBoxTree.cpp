/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#include "DT_BBoxTree.h"

inline DT_CBox getBBox(int first, int last, const DT_CBox *boxes, const DT_Index *indices) 
{
	assert(last - first >= 1);

	DT_CBox bbox = boxes[indices[first]];
	int i;
	for (i = first; i < last; ++i) 
	{
		bbox = bbox.hull(boxes[indices[i]]);
	}

	return bbox;
}

DT_BBoxNode::DT_BBoxNode(int first, int last, int& node, DT_BBoxNode *free_nodes, const DT_CBox *boxes, DT_Index *indices, const DT_CBox& bbox)
{
	assert(last - first >= 2);
	
	int axis = bbox.longestAxis();
	MT_Scalar abscissa = bbox.getCenter()[axis];
	int i = first, mid = last;
	while (i < mid) 
	{
		if (boxes[indices[i]].getCenter()[axis] < abscissa)
		{
			++i;
		}
		else
		{
			--mid;
			std::swap(indices[i], indices[mid]);
		}
	}

	if (mid == first || mid == last) 
	{
		mid = (first + last) / 2;
	}
	
	m_lbox = getBBox(first, mid, boxes, indices);
	m_rbox = getBBox(mid, last, boxes, indices);
	m_flags = 0x0;

	if (mid - first == 1)
	{
		m_flags |= LLEAF;
		m_lchild = indices[first];
	}
	else 
	{	
		m_lchild = node++;
		new(&free_nodes[m_lchild]) DT_BBoxNode(first, mid, node, free_nodes, boxes, indices, m_lbox);
	}

	if (last - mid == 1)
	{
		m_flags |= RLEAF;
		m_rchild = indices[mid];
	}
	else 
	{
		m_rchild = node++;
		new(&free_nodes[m_rchild]) DT_BBoxNode(mid, last, node, free_nodes, boxes, indices, m_rbox); 
	}
}
