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
 
#include "BOP_Segment.h"

#define UNDEFINED 0

/**
 * Constructs a new segment.
 */
BOP_Segment::BOP_Segment(){
	m_cfg1  = UNDEFINED;
	m_cfg2  = UNDEFINED;  
}

/**
 * Returns the relative edge index between two relative vertex indices.
 * @param v1 relative vertex index
 * @param v2 relative vertex index
 * @return relative edge index between two relative vertex indices, -1 otherwise
 */
int BOP_Segment::getEdgeBetween(unsigned int v1, unsigned int v2) 
{
	if ((v1 == 1 && v2 == 2) || (v1 == 2 && v2 == 1)) return 1;
	if ((v1 == 3 && v2 == 2) || (v1 == 2 && v2 == 3)) return 2;
	if ((v1 == 1 && v2 == 3) || (v1 == 3 && v2 == 1)) return 3;
	return -1;
}

/** 
 * Returns if a relative vertex index is on a relative edge index.
 * @param v relative vertex index
 * @param e relative edge index
 * @return true if the relative vertex index is on the relative edge index, 
 * false otherwise.
 */
bool BOP_Segment::isOnEdge(unsigned int v, unsigned int e)
{
	if (v == 1 && (e == 1 || e == 3)) return true;
	if (v == 2 && (e == 1 || e == 2)) return true;
	if (v == 3 && (e == 2 || e == 3)) return true;
	return false;
}

/**
 * Inverts the segment, swapping ends data.
 */
void BOP_Segment::invert()
{
	BOP_Index aux = m_v1;
	m_v1    = m_v2;
	m_v2    = aux;
	aux     = m_cfg1;
	m_cfg1  = m_cfg2;
	m_cfg2  = aux;
}

/**
 * Sorts the segment according to ends configuration.
 * The criterion to sort is ...
 *
 *   UNDEFINED < VERTEX < EDGE < IN
 *   cfg1 > cfg2
 *
 * so ... 
 *
 *   VERTEX(cfg1) => UNDEFINED(cfg2) || VERTEX(cfg2)
 *   EDGE(cfg1) =>   UNDEFINED(cfg2) || VERTEX(cfg2) || EDGE(cfg2)
 *   IN(cfg1) =>     UNDEFINED(cfg2) || VERTEX(cfg2) || EDGE(cfg2) || IN(cfg2)
 */
void BOP_Segment::sort()
{
	if (m_cfg1 < m_cfg2) invert();
}

/**
 * Returns if the specified end segment configuration is IN.
 * @return true if the specified end segment configuration is IN, false otherwise
 */
bool BOP_Segment::isIn(unsigned int cfg)
{
	return (cfg == 20);
}

/**
 * Returns if the specified end segment configuration is EDGE.
 * @return true if the specified end segment configuration is EDGE, false otherwise
 */
bool BOP_Segment::isEdge(unsigned int cfg)
{
	return (cfg > 10) && (cfg < 20);
}

/**
 * Returns if the specified end segment configuration is VERTEX.
 * @return true if the specified end segment configuration is VERTEX, false otherwise
 */
bool BOP_Segment::isVertex(unsigned int cfg)
{
	return (cfg!=UNDEFINED) && (cfg < 10);
}

/**
 * Returns if the specified end segment configuration is DEFINED (not UNDEFINED).
 * @return true if the specified end segment configuration is DEFINED, false otherwise
 */
bool BOP_Segment::isDefined(unsigned int cfg)
{
	return (cfg != UNDEFINED);
}   

/**
 * Returns if the specified end segment configuration is UNDEFINED.
 * @return true if the specified end segment configuration is UNDEFINED, false otherwise
 */
bool BOP_Segment::isUndefined(unsigned int cfg)
{
	return (cfg == UNDEFINED);
}   

/**
 * Returns the relative edge index from the specified end segment configuration.
 * @return relative edge index from the specified end segment configuration
 */
unsigned int BOP_Segment::getEdge(unsigned int cfg)
{
	return cfg-10;
}   

/**
 * Returns the relative vertex index from the specified end segment configuration.
 * @return relative vertex index from the specified end segment configuration
 */
BOP_Index BOP_Segment::getVertex(unsigned int cfg)
{
	return cfg;
}   

/**
 * Returns the end segment configuration for the specified relative edge index.
 * @return end segment configuration for the specified relative edge index
 */
unsigned int BOP_Segment::createEdgeCfg(unsigned int edge)
{
	return 10+edge;
}

/**
 * Returns the end segment configuration for the specified relative vertex index.
 * @return end segment configuration for the specified relative vertex index
 */
unsigned int BOP_Segment::createVertexCfg(BOP_Index vertex)
{
	return vertex;
}

/**
 * Returns the end segment IN configuration.
 * @return end segment IN configuration
 */
unsigned int BOP_Segment::createInCfg()
{
	return 20;
}

/**
 * Returns the end segment UNDEFINED configuration.
 * @return end segment UNDEFINED configuration
 */
unsigned int BOP_Segment::createUndefinedCfg()
{
	return UNDEFINED;
}

/**
 * Returns the inner segment configuration.
 * @return inner segment configuration
 */
unsigned int BOP_Segment::getConfig() 
{
	if (isUndefined(m_cfg1)) return m_cfg2;
	else if (isUndefined(m_cfg2)) return m_cfg1;
	else if (isVertex(m_cfg1)) {
		// v1 is vertex
		if (isVertex(m_cfg2)) {
			// v2 is vertex
			return createEdgeCfg(getEdgeBetween(getVertex(m_cfg1),getVertex(m_cfg2)));
		}
		else if (isEdge(m_cfg2)) {
			// v2 is edge
			if (isOnEdge(m_cfg1,getEdge(m_cfg2))) return m_cfg2;
			else return createInCfg(); //IN
		} 
		else return createInCfg(); //IN
	}
	else if (isEdge(m_cfg1)) {
		// v1 is edge
		if (isVertex(m_cfg2)) {
			// v2 is vertex
			if (isOnEdge(m_cfg2,getEdge(m_cfg1))) return m_cfg1;
			else return createInCfg(); //IN
		}
		else if (isEdge(m_cfg2)) {
			// v2 is edge
			if (m_cfg1 == m_cfg2) return m_cfg1;
			else return createInCfg(); // IN
		}
		else return createInCfg(); // IN
	}
	else return createInCfg(); // IN
}

/**
 * Implements operator <<
 */
ostream &operator<<(ostream &stream, const BOP_Segment &c)
{
	cout << "m_v1: " << c.m_v1 << "(" << c.m_cfg1 << ") m_v2: " << c.m_v2 << "(" << c.m_cfg2 << ")";
	return stream;
}
