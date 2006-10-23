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

#include "btOptimizedBvh.h"
#include "btStridingMeshInterface.h"
#include "LinearMath/btAabbUtil2.h"



void btOptimizedBvh::build(btStridingMeshInterface* triangles)
{
	//int countTriangles = 0;

	

	// NodeArray	triangleNodes;

	struct	NodeTriangleCallback : public btInternalTriangleIndexCallback
	{
		NodeArray&	m_triangleNodes;

		NodeTriangleCallback(NodeArray&	triangleNodes)
			:m_triangleNodes(triangleNodes)
		{

		}

		virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
		{

			btOptimizedBvhNode node;
			node.m_aabbMin = btVector3(1e30f,1e30f,1e30f); 
			node.m_aabbMax = btVector3(-1e30f,-1e30f,-1e30f); 
			node.m_aabbMin.setMin(triangle[0]);
			node.m_aabbMax.setMax(triangle[0]);
			node.m_aabbMin.setMin(triangle[1]);
			node.m_aabbMax.setMax(triangle[1]);
			node.m_aabbMin.setMin(triangle[2]);
			node.m_aabbMax.setMax(triangle[2]);

			node.m_escapeIndex = -1;
			node.m_leftChild = 0;
			node.m_rightChild = 0;


			//for child nodes
			node.m_subPart = partId;
			node.m_triangleIndex = triangleIndex;

			
			m_triangleNodes.push_back(node);
		}
	};

	

	NodeTriangleCallback	callback(m_leafNodes);

	btVector3 aabbMin(-1e30f,-1e30f,-1e30f);
	btVector3 aabbMax(1e30f,1e30f,1e30f);

	triangles->InternalProcessAllTriangles(&callback,aabbMin,aabbMax);

	//now we have an array of leafnodes in m_leafNodes

	m_contiguousNodes = new btOptimizedBvhNode[2*m_leafNodes.size()];
	m_curNodeIndex = 0;

	m_rootNode1 = buildTree(m_leafNodes,0,m_leafNodes.size());


	///create the leafnodes first
//	btOptimizedBvhNode* leafNodes = new btOptimizedBvhNode;
}

btOptimizedBvh::~btOptimizedBvh()
{
	if (m_contiguousNodes)
		delete []m_contiguousNodes;
}

btOptimizedBvhNode*	btOptimizedBvh::buildTree	(NodeArray&	leafNodes,int startIndex,int endIndex)
{
	btOptimizedBvhNode* internalNode;

	int splitAxis, splitIndex, i;
	int numIndices =endIndex-startIndex;
	int curIndex = m_curNodeIndex;

	assert(numIndices>0);

	if (numIndices==1)
	{
		return new (&m_contiguousNodes[m_curNodeIndex++]) btOptimizedBvhNode(leafNodes[startIndex]);
	}
	//calculate Best Splitting Axis and where to split it. Sort the incoming 'leafNodes' array within range 'startIndex/endIndex'.
	
	splitAxis = calcSplittingAxis(leafNodes,startIndex,endIndex);

	splitIndex = sortAndCalcSplittingIndex(leafNodes,startIndex,endIndex,splitAxis);

	internalNode = &m_contiguousNodes[m_curNodeIndex++];
	
	internalNode->m_aabbMax.setValue(-1e30f,-1e30f,-1e30f);
	internalNode->m_aabbMin.setValue(1e30f,1e30f,1e30f);
	
	for (i=startIndex;i<endIndex;i++)
	{
		internalNode->m_aabbMax.setMax(leafNodes[i].m_aabbMax);
		internalNode->m_aabbMin.setMin(leafNodes[i].m_aabbMin);
	}

	

	//internalNode->m_escapeIndex;
	internalNode->m_leftChild = buildTree(leafNodes,startIndex,splitIndex);
	internalNode->m_rightChild = buildTree(leafNodes,splitIndex,endIndex);

	internalNode->m_escapeIndex  = m_curNodeIndex - curIndex;
	return internalNode;
}

int	btOptimizedBvh::sortAndCalcSplittingIndex(NodeArray&	leafNodes,int startIndex,int endIndex,int splitAxis)
{
	int i;
	int splitIndex =startIndex;
	int numIndices = endIndex - startIndex;
	float splitValue;

	btVector3 means(0.f,0.f,0.f);
	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		means+=center;
	}
	means *= (1.f/(float)numIndices);
	
	splitValue = means[splitAxis];
	
	//sort leafNodes so all values larger then splitValue comes first, and smaller values start from 'splitIndex'.
	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		if (center[splitAxis] > splitValue)
		{
			//swap
			btOptimizedBvhNode tmp = leafNodes[i];
			leafNodes[i] = leafNodes[splitIndex];
			leafNodes[splitIndex] = tmp;
			splitIndex++;
		}
	}
	if ((splitIndex==startIndex) || (splitIndex == (endIndex-1)))
	{
		splitIndex = startIndex+ (numIndices>>1);
	}
	return splitIndex;
}


int	btOptimizedBvh::calcSplittingAxis(NodeArray&	leafNodes,int startIndex,int endIndex)
{
	int i;

	btVector3 means(0.f,0.f,0.f);
	btVector3 variance(0.f,0.f,0.f);
	int numIndices = endIndex-startIndex;

	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		means+=center;
	}
	means *= (1.f/(float)numIndices);
		
	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		btVector3 diff2 = center-means;
		diff2 = diff2 * diff2;
		variance += diff2;
	}
	variance *= (1.f/	((float)numIndices-1)	);
	
	return variance.maxAxis();
}



void	btOptimizedBvh::reportAabbOverlappingNodex(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	//either choose recursive traversal (walkTree) or stackless (walkStacklessTree)

	//walkTree(m_rootNode1,nodeCallback,aabbMin,aabbMax);

	walkStacklessTree(m_rootNode1,nodeCallback,aabbMin,aabbMax);
}

void	btOptimizedBvh::walkTree(btOptimizedBvhNode* rootNode,btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	bool isLeafNode, aabbOverlap = TestAabbAgainstAabb2(aabbMin,aabbMax,rootNode->m_aabbMin,rootNode->m_aabbMax);
	if (aabbOverlap)
	{
		isLeafNode = (!rootNode->m_leftChild && !rootNode->m_rightChild);
		if (isLeafNode)
		{
			nodeCallback->processNode(rootNode);
		} else
		{
			walkTree(rootNode->m_leftChild,nodeCallback,aabbMin,aabbMax);
			walkTree(rootNode->m_rightChild,nodeCallback,aabbMin,aabbMax);
		}
	}

}

int maxIterations = 0;

void	btOptimizedBvh::walkStacklessTree(btOptimizedBvhNode* rootNode,btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	int escapeIndex, curIndex = 0;
	int walkIterations = 0;
	bool aabbOverlap, isLeafNode;

	while (curIndex < m_curNodeIndex)
	{
		//catch bugs in tree data
		assert (walkIterations < m_curNodeIndex);

		walkIterations++;
		aabbOverlap = TestAabbAgainstAabb2(aabbMin,aabbMax,rootNode->m_aabbMin,rootNode->m_aabbMax);
		isLeafNode = (!rootNode->m_leftChild && !rootNode->m_rightChild);
		
		if (isLeafNode && aabbOverlap)
		{
			nodeCallback->processNode(rootNode);
		} 
		
		if (aabbOverlap || isLeafNode)
		{
			rootNode++;
			curIndex++;
		} else
		{
			escapeIndex = rootNode->m_escapeIndex;
			rootNode += escapeIndex;
			curIndex += escapeIndex;
		}
		
	}

	if (maxIterations < walkIterations)
		maxIterations = walkIterations;

}


void	btOptimizedBvh::reportSphereOverlappingNodex(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{

}

