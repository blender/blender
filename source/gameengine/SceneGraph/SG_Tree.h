/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Bounding Box
 */
 
#ifndef __SG_TREE_H__
#define __SG_TREE_H__
 
#include "MT_Point3.h"
#include "SG_BBox.h"

#include <vector> 

class SG_Node;


/**
 * SG_Tree.
 * Holds a binary tree of SG_Nodes.
 */
class SG_Tree 
{
	SG_Tree* m_left;
	SG_Tree* m_right;
	SG_Tree* m_parent;
	SG_BBox  m_bbox;
	SG_Node* m_client_object;
public:
	SG_Tree();
	SG_Tree(SG_Tree* left, SG_Tree* right);
	
	SG_Tree(SG_Node* client);
	~SG_Tree();
	
	/**
	 * Computes the volume of the bounding box.
	 */
	MT_Scalar volume() const;
	
	/**
	 * Prints the tree (for debugging.)
	 */
	void dump() const;
	
	/**
	 * Returns the left node.
	 */
	SG_Tree *Left() const;
	SG_Tree *Right() const;
	SG_Node *Client() const;
	
	SG_Tree* Find(SG_Node *node);
	/**
	 * Gets the eight corners of this treenode's bounding box,
	 * in world coordinates.
	 * @param box: an array of 8 MT_Point3
	 * @example MT_Point3 box[8];
	 *          treenode->get(box);
	 */
	void get(MT_Point3 *box) const;
	/**
	 * Get the tree node's bounding box.
	 */
	const SG_BBox& BBox() const;
	
	/**
	 * Test if the given bounding box is inside this bounding box.
	 */
	bool inside(const MT_Point3 &point) const;

};


/**
 *  SG_TreeFactory generates an SG_Tree from a list of SG_Nodes.
 *  It joins pairs of SG_Nodes to minimise the size of the resultant
 *  bounding box.
 *  cf building an optimised Huffman tree.
 *  @warning O(n^3)!!!
 */
class SG_TreeFactory 
{
	std::vector<SG_Tree*> m_objects;
public:
	SG_TreeFactory();
	~SG_TreeFactory();
	
	/**
	 *  Add a node to be added to the tree.
	 */
	void Add(SG_Node* client);

	/**
	 *  Build the tree from the set of nodes added by
	 *  the Add method.
	 */
	SG_Tree* MakeTree();
};

#endif /* __SG_BBOX_H__ */
