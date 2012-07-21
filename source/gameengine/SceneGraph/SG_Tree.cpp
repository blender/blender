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
 * Bounding Box
 */

/** \file gameengine/SceneGraph/SG_Tree.cpp
 *  \ingroup bgesg
 */


#include <math.h>
 
#include "SG_BBox.h"
#include "SG_Tree.h"
#include "SG_Node.h"

SG_Tree::SG_Tree()
{
}

SG_Tree::SG_Tree(SG_Tree* left, SG_Tree* right) :
		m_left(left),
		m_right(right),
		m_client_object(NULL)
{
	if (m_left)
	{
		m_bbox = m_left->m_bbox;
		m_left->m_parent = this;
	}
	if (m_right)
	{
		m_bbox += m_right->m_bbox;
		m_right->m_parent = this;
	}
	m_center = (m_bbox.m_min + m_bbox.m_max)/2.0;
	m_radius = (m_bbox.m_max - m_bbox.m_min).length();
}
	
SG_Tree::SG_Tree(SG_Node* client) :
		m_left(NULL),
		m_right(NULL),
		m_client_object(client)
{
	m_bbox = SG_BBox(client->BBox(), client->GetWorldTransform());
	m_center = (m_bbox.m_min + m_bbox.m_max)/2.0;
	m_radius = (m_bbox.m_max - m_bbox.m_min).length();
}

SG_Tree::~SG_Tree() 
{
}
	
MT_Scalar SG_Tree::volume() const
{
	return m_bbox.volume();
}
	
void SG_Tree::dump() const
{
	if (m_left)
		m_left->dump();
	if (m_client_object)
		std::cout << m_client_object << std::endl;
	else
		std::cout << this << " ";
	if (m_right)
		m_right->dump();
}

SG_Tree* SG_Tree::Left() const
{
	return m_left;
}

SG_Tree* SG_Tree::Right() const
{
	return m_right;
}

SG_Node* SG_Tree::Client() const
{
	return m_client_object;
}

SG_Tree* SG_Tree::Find(SG_Node *node)
{
	if (m_client_object == node)
		return this;
	
	SG_Tree *left = m_left, *right = m_right;
	
	if (left && right)
	{
		if (right->m_bbox.intersects(node->BBox()))
			std::swap(left, right);
	}
		
	if (left)
	{
		SG_Tree* ret = left->Find(node);
		if (ret) return ret;
	}
	
	if (right)
	{
		SG_Tree* ret = right->Find(node);
		if (ret) return ret;
	}
	
	return NULL;
}

void SG_Tree::get(MT_Point3 *box) const
{
	MT_Transform identity;
	identity.setIdentity();
	m_bbox.get(box, identity);
}

bool SG_Tree::inside(const MT_Point3 &point) const
{
	return m_bbox.inside(point);
}

const SG_BBox& SG_Tree::BBox() const
{
	return m_bbox;
}

void SG_Tree::SetLeft(SG_Tree *left)
{
	m_left = left;
	m_bbox += left->m_bbox;
	m_center = (m_bbox.m_min + m_bbox.m_max)/2.0;
	m_radius = (m_bbox.m_max - m_bbox.m_min).length();
}

void SG_Tree::SetRight(SG_Tree *right)
{
	m_right = right;
	m_bbox += right->m_bbox;
	m_center = (m_bbox.m_min + m_bbox.m_max)/2.0;
	m_radius = (m_bbox.m_max - m_bbox.m_min).length();
}

/**
 * A Half array is a square 2d array where cell(x, y) is undefined
 * if x < y.
 */
template<typename T>
class HalfArray
{
	std::vector<std::vector<T> > m_array;
public:
	HalfArray() {}
	~HalfArray() {}
	
	void resize(unsigned int size)
	{
		m_array.resize(size);
		for ( unsigned int i = 0; i < size; i++)
		{
			m_array[i].resize(size - i);
		}
	}
	
	T& operator() (unsigned int x, unsigned int y)
	{
		assert(x >= y);
		return m_array[y][x - y];
	}
	
	void erase_column (unsigned int x)
	{
		for (unsigned int y = 0; y <= x; y++)
			m_array[y].erase(m_array[y].begin() + x - y);
	}

	void delete_column (unsigned int x)
	{
		for (unsigned int y = 0; y < x; y++)
		{
			delete m_array[y][x - y];
			m_array[y].erase(m_array[y].begin() + x - y);
		}
	}
	
	void erase_row (unsigned int y)
	{
		m_array.erase(m_array.begin() + y);
	}
};

SG_TreeFactory::SG_TreeFactory()
{
}

SG_TreeFactory::~SG_TreeFactory()
{
}
	
void SG_TreeFactory::Add(SG_Node* client)
{
	if (client)
		m_objects.insert(new SG_Tree(client));
}

void SG_TreeFactory::Add(SG_Tree* tree)
{
	m_objects.insert(tree);
}

SG_Tree* SG_TreeFactory::MakeTreeDown(SG_BBox &bbox)
{
	if (m_objects.size() == 0)
		return NULL;
	if (m_objects.size() == 1)
		return *m_objects.begin();
		
	TreeSet::iterator it = m_objects.begin();
	SG_Tree *root = *it;
	if (m_objects.size() == 2)
	{
		root->SetRight(*(++it));
		return root;
	}
		
	if (m_objects.size() == 3)
	{
		root->SetLeft(*(++it));
		root->SetRight(*(++it));
		return root;
	}

	if (bbox.volume() < 1.0)
		return MakeTreeUp();
		
	SG_TreeFactory lefttree;
	SG_TreeFactory righttree;
	
	SG_BBox left, right;
	int hasleft = 0, hasright = 0;
	bbox.split(left, right);
	
	if (left.test(root->BBox()) == SG_BBox::INSIDE)
	{
		lefttree.Add(root);
		root = NULL;
	}
	
	if (root && right.test(root->BBox()) == SG_BBox::INSIDE)
	{
		righttree.Add(root);
		root = NULL;
	}
	
	for (++it; it != m_objects.end(); ++it)
	{
		switch (left.test((*it)->BBox()))
		{
			case SG_BBox::INSIDE:
				// Object is inside left tree;
				lefttree.Add(*it);
				hasleft++;
				break;
			case SG_BBox::OUTSIDE:
				righttree.Add(*it);
				hasright++;
				break;
			case SG_BBox::INTERSECT:
				if (left.inside((*it)->Client()->GetWorldPosition()))
				{
					lefttree.Add(*it);
					hasleft++;
				}
				else {
					righttree.Add(*it);
					hasright++;
				}
				break;
		}
	}
	std::cout << "Left: " << hasleft << " Right: " << hasright << " Count: " << m_objects.size() << std::endl;
	
	SG_Tree *leftnode = NULL;
	if (hasleft)
		leftnode = lefttree.MakeTreeDown(left);
	
	SG_Tree *rightnode = NULL;
	if (hasright)
		rightnode = righttree.MakeTreeDown(right);
		
	if (!root)
		root = new SG_Tree(leftnode, rightnode);
	else
	{
		if (leftnode)
			root->SetLeft(leftnode);
		if (rightnode)
			root->SetRight(rightnode);
	}

	return root;
}

SG_Tree* SG_TreeFactory::MakeTree()
{
	if (m_objects.size() < 8)
		return MakeTreeUp();

	TreeSet::iterator it = m_objects.begin();
	SG_BBox bbox((*it)->BBox());
	for (++it; it != m_objects.end(); ++it)
		bbox += (*it)->BBox();
	
	return MakeTreeDown(bbox);
}

SG_Tree* SG_TreeFactory::MakeTreeUp()
{
	unsigned int num_objects = m_objects.size();
	
	if (num_objects < 1)
		return NULL;
	if (num_objects < 2)
		return *m_objects.begin();

	HalfArray<SG_Tree*> sizes;
	sizes.resize(num_objects);
	
	unsigned int x, y;
	TreeSet::iterator xit, yit;
	for ( y = 0, yit = m_objects.begin(); y < num_objects; y++, ++yit)
	{
		sizes(y, y) = *yit;
		xit = yit;
		for ( x = y+1, ++xit; x < num_objects; x++, ++xit)
		{
			sizes(x, y) = new SG_Tree(*xit, *yit);
			
		}
	}
	while (num_objects > 2)
	{
		/* Find the pair of bboxes that produce the smallest combined bbox. */
		unsigned int minx = UINT_MAX, miny = UINT_MAX;
		MT_Scalar min_volume = FLT_MAX;
		SG_Tree *min = NULL;
		//char temp[16];
		for ( y = 0; y < num_objects; y++)
		{
			for ( x = y+1; x < num_objects; x++)
			{
				if (sizes(x, y)->volume() < min_volume)
				{
					min = sizes(x, y);
					minx = x;
					miny = y;
					min_volume = sizes(x, y)->volume();
				}
			}
		}
		
		/* Remove other bboxes that contain the two bboxes */
		sizes.delete_column(miny);
		
		for ( x = miny + 1; x < num_objects; x++)
		{
			if (x == minx)
				continue;
			delete sizes(x, miny);
		}
		sizes.erase_row(miny);
		
		num_objects--;
		minx--;
		sizes(minx, minx) = min;
		for ( x = minx + 1; x < num_objects; x++)
		{
			delete sizes(x, minx);
			sizes(x, minx) = new SG_Tree(min, sizes(x, x));
		}
		for ( y = 0; y < minx; y++)
		{
			delete sizes(minx, y);
			sizes(minx, y) = new SG_Tree(sizes(y, y), min);
		}
	}
	return sizes(1, 0);
}

