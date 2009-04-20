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
 
#ifndef BOP_SEGMENT_H
#define BOP_SEGMENT_H

#include "BOP_Indexs.h"
#include <iostream>
using namespace std;

class BOP_Segment
{
private:
	int getEdgeBetween(unsigned int v1, unsigned int v2);
	bool isOnEdge(unsigned int v, unsigned int e);

public:
	// Cfg : Configuration of the vertices
	// Values:
	//         20 IN,
	//         1X Intersected edge X{1,2,3} of the face,
	//         0X Coincident vertice X{1,2,3} of the face,
	//         0 otherwise
	unsigned int m_cfg1, m_cfg2; 
	BOP_Index m_v1, m_v2;     // if cfgX >0, vX is the vertice index of the face
	BOP_Segment();

	static bool isIn(unsigned int cfg);
	static bool isEdge(unsigned int cfg);
	static bool isVertex(unsigned int cfg);
	static bool isDefined(unsigned int cfg);
	static bool isUndefined(unsigned int cfg);
	static unsigned int getEdge(unsigned int cfg);
	static BOP_Index getVertex(unsigned int cfg);
	static unsigned int createEdgeCfg(unsigned int edge);
	static unsigned int createVertexCfg(BOP_Index vertex);
	static unsigned int createInCfg();
	static unsigned int createUndefinedCfg();
	void invert();  
	void sort();
	unsigned int getConfig();

	friend   ostream &operator<<(ostream &stream, const BOP_Segment &c);
};

#endif
