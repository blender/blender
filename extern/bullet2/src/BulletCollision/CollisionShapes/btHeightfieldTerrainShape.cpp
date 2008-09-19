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

#include "btHeightfieldTerrainShape.h"

#include "LinearMath/btTransformUtil.h"


btHeightfieldTerrainShape::btHeightfieldTerrainShape(int width,int length,void* heightfieldData,btScalar maxHeight,int upAxis,bool useFloatData,bool flipQuadEdges)
:m_localScaling(btScalar(1.),btScalar(1.),btScalar(1.)),
m_width(width),
m_length(length),
m_heightfieldDataUnknown(heightfieldData),
m_maxHeight(maxHeight),
m_upAxis(upAxis),
m_useFloatData(useFloatData),
m_flipQuadEdges(flipQuadEdges),
m_useDiamondSubdivision(false)
{


	btScalar	quantizationMargin = 1.f;

	//enlarge the AABB to avoid division by zero when initializing the quantization values
	btVector3 clampValue(quantizationMargin,quantizationMargin,quantizationMargin);

	btVector3	halfExtents(0,0,0);

	switch (m_upAxis)
	{
	case 0:
		{
			halfExtents.setValue(
				m_maxHeight,
				m_width,
				m_length);
			break;
		}
	case 1:
		{
			halfExtents.setValue(
				m_width,
				m_maxHeight,
				m_length);
			break;
		};
	case 2:
		{
			halfExtents.setValue(
				m_width,
				m_length,
				m_maxHeight
			);
			break;
		}
	default:
		{
			//need to get valid m_upAxis
			btAssert(0);
		}
	}

	halfExtents*= btScalar(0.5);
	
	m_localAabbMin = -halfExtents - clampValue;
	m_localAabbMax = halfExtents + clampValue;
	btVector3 aabbSize = m_localAabbMax - m_localAabbMin;

}


btHeightfieldTerrainShape::~btHeightfieldTerrainShape()
{
}



void btHeightfieldTerrainShape::getAabb(const btTransform& t,btVector3& aabbMin,btVector3& aabbMax) const
{
/*
	aabbMin.setValue(-1e30f,-1e30f,-1e30f);
	aabbMax.setValue(1e30f,1e30f,1e30f);
*/

	btVector3 halfExtents = (m_localAabbMax-m_localAabbMin)* m_localScaling * btScalar(0.5);

	btMatrix3x3 abs_b = t.getBasis().absolute();  
	btPoint3 center = t.getOrigin();
	btVector3 extent = btVector3(abs_b[0].dot(halfExtents),
		   abs_b[1].dot(halfExtents),
		  abs_b[2].dot(halfExtents));
	extent += btVector3(getMargin(),getMargin(),getMargin());

	aabbMin = center - extent;
	aabbMax = center + extent;


}

btScalar	btHeightfieldTerrainShape::getHeightFieldValue(int x,int y) const
{
	btScalar val = 0.f;
	if (m_useFloatData)
	{
		val = m_heightfieldDataFloat[(y*m_width)+x];
	} else
	{
		//assume unsigned short int
		unsigned char heightFieldValue = m_heightfieldDataUnsignedChar[(y*m_width)+x];
		val = heightFieldValue* (m_maxHeight/btScalar(65535));
	}
	return val;
}





void	btHeightfieldTerrainShape::getVertex(int x,int y,btVector3& vertex) const
{

	btAssert(x>=0);
	btAssert(y>=0);
	btAssert(x<m_width);
	btAssert(y<m_length);


	btScalar	height = getHeightFieldValue(x,y);

	switch (m_upAxis)
	{
	case 0:
		{
		vertex.setValue(
			height,
			(-m_width/2 ) + x,
			(-m_length/2 ) + y
			);
			break;
		}
	case 1:
		{
			vertex.setValue(
			(-m_width/2 ) + x,
			height,
			(-m_length/2 ) + y
			);
			break;
		};
	case 2:
		{
			vertex.setValue(
			(-m_width/2 ) + x,
			(-m_length/2 ) + y,
			height
			);
			break;
		}
	default:
		{
			//need to get valid m_upAxis
			btAssert(0);
		}
	}

	vertex*=m_localScaling;
	
}


void btHeightfieldTerrainShape::quantizeWithClamp(int* out, const btVector3& point) const
{
	

	btVector3 clampedPoint(point);
	clampedPoint.setMax(m_localAabbMin);
	clampedPoint.setMin(m_localAabbMax);

	btVector3 v = (clampedPoint );// * m_quantization;

	out[0] = (int)(v.getX());
	out[1] = (int)(v.getY());
	out[2] = (int)(v.getZ());
	//correct for

}


void	btHeightfieldTerrainShape::processAllTriangles(btTriangleCallback* callback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	(void)callback;
	(void)aabbMax;
	(void)aabbMin;

	//quantize the aabbMin and aabbMax, and adjust the start/end ranges

	int	quantizedAabbMin[3];
	int	quantizedAabbMax[3];

	btVector3	localAabbMin = aabbMin*btVector3(1.f/m_localScaling[0],1.f/m_localScaling[1],1.f/m_localScaling[2]);
	btVector3	localAabbMax = aabbMax*btVector3(1.f/m_localScaling[0],1.f/m_localScaling[1],1.f/m_localScaling[2]);
	
	quantizeWithClamp(quantizedAabbMin, localAabbMin);
	quantizeWithClamp(quantizedAabbMax, localAabbMax);
	
	

	int startX=0;
	int endX=m_width-1;
	int startJ=0;
	int endJ=m_length-1;

	switch (m_upAxis)
	{
	case 0:
		{
			quantizedAabbMin[1]+=m_width/2-1;
			quantizedAabbMax[1]+=m_width/2+1;
			quantizedAabbMin[2]+=m_length/2-1;
			quantizedAabbMax[2]+=m_length/2+1;

			if (quantizedAabbMin[1]>startX)
				startX = quantizedAabbMin[1];
			if (quantizedAabbMax[1]<endX)
				endX = quantizedAabbMax[1];
			if (quantizedAabbMin[2]>startJ)
				startJ = quantizedAabbMin[2];
			if (quantizedAabbMax[2]<endJ)
				endJ = quantizedAabbMax[2];
			break;
		}
	case 1:
		{
			quantizedAabbMin[0]+=m_width/2-1;
			quantizedAabbMax[0]+=m_width/2+1;
			quantizedAabbMin[2]+=m_length/2-1;
			quantizedAabbMax[2]+=m_length/2+1;

			if (quantizedAabbMin[0]>startX)
				startX = quantizedAabbMin[0];
			if (quantizedAabbMax[0]<endX)
				endX = quantizedAabbMax[0];
			if (quantizedAabbMin[2]>startJ)
				startJ = quantizedAabbMin[2];
			if (quantizedAabbMax[2]<endJ)
				endJ = quantizedAabbMax[2];
			break;
		};
	case 2:
		{
			quantizedAabbMin[0]+=m_width/2-1;
			quantizedAabbMax[0]+=m_width/2+1;
			quantizedAabbMin[1]+=m_length/2-1;
			quantizedAabbMax[1]+=m_length/2+1;

			if (quantizedAabbMin[0]>startX)
				startX = quantizedAabbMin[0];
			if (quantizedAabbMax[0]<endX)
				endX = quantizedAabbMax[0];
			if (quantizedAabbMin[1]>startJ)
				startJ = quantizedAabbMin[1];
			if (quantizedAabbMax[1]<endJ)
				endJ = quantizedAabbMax[1];
			break;
		}
	default:
		{
			//need to get valid m_upAxis
			btAssert(0);
		}
	}

	
  

	for(int j=startJ; j<endJ; j++)
	{
		for(int x=startX; x<endX; x++)
		{
			btVector3 vertices[3];
			if (m_flipQuadEdges || (m_useDiamondSubdivision && ((j+x) & 1)))
			{
        //first triangle
        getVertex(x,j,vertices[0]);
        getVertex(x+1,j,vertices[1]);
        getVertex(x+1,j+1,vertices[2]);
        callback->processTriangle(vertices,x,j);
        //second triangle
        getVertex(x,j,vertices[0]);
        getVertex(x+1,j+1,vertices[1]);
        getVertex(x,j+1,vertices[2]);
        callback->processTriangle(vertices,x,j);				
			} else
			{
        //first triangle
        getVertex(x,j,vertices[0]);
        getVertex(x,j+1,vertices[1]);
        getVertex(x+1,j,vertices[2]);
        callback->processTriangle(vertices,x,j);
        //second triangle
        getVertex(x+1,j,vertices[0]);
        getVertex(x,j+1,vertices[1]);
        getVertex(x+1,j+1,vertices[2]);
        callback->processTriangle(vertices,x,j);
			}
		}
	}

	

}

void	btHeightfieldTerrainShape::calculateLocalInertia(btScalar ,btVector3& inertia)
{
	//moving concave objects not supported
	
	inertia.setValue(btScalar(0.),btScalar(0.),btScalar(0.));
}

void	btHeightfieldTerrainShape::setLocalScaling(const btVector3& scaling)
{
	m_localScaling = scaling;
}
const btVector3& btHeightfieldTerrainShape::getLocalScaling() const
{
	return m_localScaling;
}
