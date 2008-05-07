/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef HEIGHTFIELD_TERRAIN_SHAPE_H
#define HEIGHTFIELD_TERRAIN_SHAPE_H

#include "btConcaveShape.h"

///btHeightfieldTerrainShape simulates a 2D heightfield terrain 
class btHeightfieldTerrainShape : public btConcaveShape
{
protected:
	btVector3	m_localAabbMin;
	btVector3	m_localAabbMax;
	
	///terrain data
	int	m_width;
	int m_length;
	btScalar	m_maxHeight;
	union
	{
		unsigned char*	m_heightfieldDataUnsignedChar;
		btScalar*			m_heightfieldDataFloat;
		void*			m_heightfieldDataUnknown;
	};
	
	bool	m_useFloatData;
	bool	m_flipQuadEdges;
  bool  m_useDiamondSubdivision;

	int	m_upAxis;
	
	btVector3	m_localScaling;

	virtual btScalar	getHeightFieldValue(int x,int y) const;
	void		quantizeWithClamp(int* out, const btVector3& point) const;
	void		getVertex(int x,int y,btVector3& vertex) const;

	inline bool testQuantizedAabbAgainstQuantizedAabb(int* aabbMin1, int* aabbMax1,const  int* aabbMin2,const  int* aabbMax2) const
	{
		bool overlap = true;
		overlap = (aabbMin1[0] > aabbMax2[0] || aabbMax1[0] < aabbMin2[0]) ? false : overlap;
		overlap = (aabbMin1[2] > aabbMax2[2] || aabbMax1[2] < aabbMin2[2]) ? false : overlap;
		overlap = (aabbMin1[1] > aabbMax2[1] || aabbMax1[1] < aabbMin2[1]) ? false : overlap;
		return overlap;
	}

public:
	btHeightfieldTerrainShape(int width,int height,void* heightfieldData, btScalar maxHeight,int upAxis,bool useFloatData,bool flipQuadEdges);

	virtual ~btHeightfieldTerrainShape();


	void setUseDiamondSubdivision(bool useDiamondSubdivision=true) { m_useDiamondSubdivision = useDiamondSubdivision;}

	virtual int	getShapeType() const
	{
		return TERRAIN_SHAPE_PROXYTYPE;
	}

	virtual void getAabb(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const;

	virtual void	processAllTriangles(btTriangleCallback* callback,const btVector3& aabbMin,const btVector3& aabbMax) const;

	virtual void	calculateLocalInertia(btScalar mass,btVector3& inertia);

	virtual void	setLocalScaling(const btVector3& scaling);
	
	virtual const btVector3& getLocalScaling() const;
	
	//debugging
	virtual char*	getName()const {return "HEIGHTFIELD";}

};

#endif //HEIGHTFIELD_TERRAIN_SHAPE_H
