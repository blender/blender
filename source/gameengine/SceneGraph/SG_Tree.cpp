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
	m_bbox = m_left->m_bbox + m_right->m_bbox;
	m_left->m_parent = this;
	m_right->m_parent = this;
}
	
SG_Tree::SG_Tree(SG_Node* client) :
		m_left(NULL),
		m_right(NULL),
		m_client_object(client)
{
	const MT_Vector3 &scale = client->GetWorldScaling();
	m_bbox = SG_BBox(client->BBox(), 
		MT_Transform(client->GetWorldPosition(), 
			client->GetWorldOrientation().scaled(scale[0], scale[1], scale[2])));
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
	if (m_client_object)
	{
		m_client_object->getAABBox(box);
	}
	else
	{
		MT_Transform identity;
		identity.setIdentity();
		m_bbox.getaa(box, identity);
	}
}

bool SG_Tree::inside(const MT_Point3 &point) const
{
	if (m_client_object)
		return m_client_object->inside(point);

	return m_bbox.inside(point);
}

const SG_BBox& SG_Tree::BBox() const
{
	return m_bbox;
}

SG_TreeFactory::SG_TreeFactory()
{
}

SG_TreeFactory::~SG_TreeFactory()
{
}
	
void SG_TreeFactory::Add(SG_Node* client)
{
	if (client)
		m_objects.push_back(new SG_Tree(client));
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
		for( unsigned int i = 0; i < size; i++)
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

SG_Tree* SG_TreeFactory::MakeTree()
{
	unsigned int num_objects = m_objects.size();
	
	if (num_objects < 1)
		return NULL;
	if (num_objects < 2)
		return m_objects[0];

	HalfArray<SG_Tree*> sizes;
	sizes.resize(num_objects);
	
	unsigned int x, y;
	for( y = 0; y < num_objects; y++)
	{
		sizes(y, y) = m_objects[y];
		for( x = y+1; x < num_objects; x++)
		{
			sizes(x, y) = new SG_Tree(m_objects[x], m_objects[y]);
			
		}
	}
	while (num_objects > 2)
	{
		/* Find the pair of bboxes that produce the smallest combined bbox. */
		unsigned int minx, miny;
		MT_Scalar min_volume = FLT_MAX;
		SG_Tree *min;
		//char temp[16];
		for( y = 0; y < num_objects; y++)
		{
			/*std::cout << sizes(y, y) << " ";
			for( unsigned int x = 0; x < y; x++)
				std::cout << "                   "; */
			
			for( x = y+1; x < num_objects; x++)
			{
				//sprintf(temp, "%7.1f", sizes(x, y)->volume());
				//std::cout << sizes(x, y) << "(" << temp << ") ";
				if (sizes(x, y)->volume() < min_volume)
				{
					min = sizes(x, y);
					minx = x;
					miny = y;
					min_volume = sizes(x, y)->volume();
				}
			}
			//std::cout << std::endl;
		}
		
		//std::cout << "minx, miny, minv = " << minx << ", " << miny << ", " << min_volume << std::endl;
		
		/* Remove other bboxes that contain the two bboxes */
		sizes.delete_column(miny);
		
		for( x = miny + 1; x < num_objects; x++)
		{
			if (x == minx)
				continue;
			delete sizes(x, miny);
		}
		sizes.erase_row(miny);
		
		num_objects--;
		minx--;
		sizes(minx, minx) = min;
		for( x = minx + 1; x < num_objects; x++)
		{
			delete sizes(x, minx);
			sizes(x, minx) = new SG_Tree(min, sizes(x, x));
		}
		for( y = 0; y < minx; y++)
		{
			delete sizes(minx, y);
			sizes(minx, y) = new SG_Tree(sizes(y, y), min);
		}
	}
	return sizes(1, 0);
}

