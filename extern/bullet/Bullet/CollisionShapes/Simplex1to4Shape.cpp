/*
 * Copyright (c) 2005 Erwin Coumans http://www.erwincoumans.com
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/
#include "Simplex1to4Shape.h"
#include "SimdMatrix3x3.h"

BU_Simplex1to4::BU_Simplex1to4()
:m_numVertices(0)
{
}

BU_Simplex1to4::BU_Simplex1to4(const SimdPoint3& pt0)
:m_numVertices(0)
{
	AddVertex(pt0);
}

BU_Simplex1to4::BU_Simplex1to4(const SimdPoint3& pt0,const SimdPoint3& pt1)
:m_numVertices(0)
{
	AddVertex(pt0);
	AddVertex(pt1);
}

BU_Simplex1to4::BU_Simplex1to4(const SimdPoint3& pt0,const SimdPoint3& pt1,const SimdPoint3& pt2)
:m_numVertices(0)
{
	AddVertex(pt0);
	AddVertex(pt1);
	AddVertex(pt2);
}

BU_Simplex1to4::BU_Simplex1to4(const SimdPoint3& pt0,const SimdPoint3& pt1,const SimdPoint3& pt2,const SimdPoint3& pt3)
:m_numVertices(0)
{
	AddVertex(pt0);
	AddVertex(pt1);
	AddVertex(pt2);
	AddVertex(pt3);
}





void BU_Simplex1to4::AddVertex(const SimdPoint3& pt)
{
	m_vertices[m_numVertices++] = pt;
}


int	BU_Simplex1to4::GetNumVertices() const
{
	return m_numVertices;
}

int BU_Simplex1to4::GetNumEdges() const
{
	//euler formula, F-E+V = 2, so E = F+V-2

	switch (m_numVertices)
	{
	case 0:
		return 0;
	case 1: return 0;
	case 2: return 1;
	case 3: return 3;
	case 4: return 6;


	}

	return 0;
}

void BU_Simplex1to4::GetEdge(int i,SimdPoint3& pa,SimdPoint3& pb) const
{
	
    switch (m_numVertices)
	{

	case 2: 
		pa = m_vertices[0];
		pb = m_vertices[1];
		break;
	case 3:  
		switch (i)
		{
		case 0:
			pa = m_vertices[0];
			pb = m_vertices[1];
			break;
		case 1:
			pa = m_vertices[1];
			pb = m_vertices[2];
			break;
		case 2:
			pa = m_vertices[2];
			pb = m_vertices[0];
			break;

		}
		break;
	case 4: 
		switch (i)
		{
		case 0:
			pa = m_vertices[0];
			pb = m_vertices[1];
			break;
		case 1:
			pa = m_vertices[1];
			pb = m_vertices[2];
			break;
		case 2:
			pa = m_vertices[2];
			pb = m_vertices[0];
			break;
		case 3:
			pa = m_vertices[0];
			pb = m_vertices[3];
			break;
		case 4:
			pa = m_vertices[1];
			pb = m_vertices[3];
			break;
		case 5:
			pa = m_vertices[2];
			pb = m_vertices[3];
			break;
		}

	}




}

void BU_Simplex1to4::GetVertex(int i,SimdPoint3& vtx) const
{
	vtx = m_vertices[i];
}

int	BU_Simplex1to4::GetNumPlanes() const
{
	switch (m_numVertices)
	{
	case 0:
			return 0;
	case 1:
			return 0;
	case 2:
			return 0;
	case 3:
			return 2;
	case 4:
			return 4;
	default:
		{
		}
	}
	return 0;
}


void BU_Simplex1to4::GetPlane(SimdVector3& planeNormal,SimdPoint3& planeSupport,int i) const
{
	
}

int BU_Simplex1to4::GetIndex(int i) const
{
	return 0;
}

bool BU_Simplex1to4::IsInside(const SimdPoint3& pt,SimdScalar tolerance) const
{
	return false;
}

