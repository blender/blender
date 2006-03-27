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

#include "OptimizedBvh.h"
#include "StridingMeshInterface.h"
#include "AabbUtil2.h"



void OptimizedBvh::Build(StridingMeshInterface* triangles)
{
	//int countTriangles = 0;

	

	// NodeArray	triangleNodes;

	struct	NodeTriangleCallback : public InternalTriangleIndexCallback
	{
		NodeArray&	m_triangleNodes;

		NodeTriangleCallback(NodeArray&	triangleNodes)
			:m_triangleNodes(triangleNodes)
		{

		}

		virtual void InternalProcessTriangleIndex(SimdVector3* triangle,int partId,int  triangleIndex)
		{

			OptimizedBvhNode node;
			node.m_aabbMin = SimdVector3(1e30f,1e30f,1e30f); 
			node.m_aabbMax = SimdVector3(-1e30f,-1e30f,-1e30f); 
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

	SimdVector3 aabbMin(-1e30f,-1e30f,-1e30f);
	SimdVector3 aabbMax(1e30f,1e30f,1e30f);

	triangles->InternalProcessAllTriangles(&callback,aabbMin,aabbMax);

	//now we have an array of leafnodes in m_leafNodes

	m_contiguousNodes = new OptimizedBvhNode[2*m_leafNodes.size()];
	m_curNodeIndex = 0;

	m_rootNode1 = BuildTree(m_leafNodes,0,m_leafNodes.size());


	///create the leafnodes first
//	OptimizedBvhNode* leafNodes = new OptimizedBvhNode;
}


OptimizedBvhNode*	OptimizedBvh::BuildTree	(NodeArray&	leafNodes,int startIndex,int endIndex)
{
	OptimizedBvhNode* internalNode;

	int splitAxis, splitIndex, i;
	int numIndices =endIndex-startIndex;
	int curIndex = m_curNodeIndex;

	assert(numIndices>0);

	if (numIndices==1)
	{
		return new (&m_contiguousNodes[m_curNodeIndex++]) OptimizedBvhNode(leafNodes[startIndex]);
	}
	//calculate Best Splitting Axis and where to split it. Sort the incoming 'leafNodes' array within range 'startIndex/endIndex'.
	
	splitAxis = CalcSplittingAxis(leafNodes,startIndex,endIndex);

	splitIndex = SortAndCalcSplittingIndex(leafNodes,startIndex,endIndex,splitAxis);

	internalNode = &m_contiguousNodes[m_curNodeIndex++];
	
	internalNode->m_aabbMax.setValue(-1e30f,-1e30f,-1e30f);
	internalNode->m_aabbMin.setValue(1e30f,1e30f,1e30f);
	
	for (i=startIndex;i<endIndex;i++)
	{
		internalNode->m_aabbMax.setMax(leafNodes[i].m_aabbMax);
		internalNode->m_aabbMin.setMin(leafNodes[i].m_aabbMin);
	}

	

	//internalNode->m_escapeIndex;
	internalNode->m_leftChild = BuildTree(leafNodes,startIndex,splitIndex);
	internalNode->m_rightChild = BuildTree(leafNodes,splitIndex,endIndex);

	internalNode->m_escapeIndex  = m_curNodeIndex - curIndex;
	return internalNode;
}

int	OptimizedBvh::SortAndCalcSplittingIndex(NodeArray&	leafNodes,int startIndex,int endIndex,int splitAxis)
{
	int i;
	int splitIndex =startIndex;
	int numIndices = endIndex - startIndex;
	float splitValue;

	SimdVector3 means(0.f,0.f,0.f);
	for (i=startIndex;i<endIndex;i++)
	{
		SimdVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		means+=center;
	}
	means *= (1.f/(float)numIndices);
	
	splitValue = means[splitAxis];
	
	//sort leafNodes so all values larger then splitValue comes first, and smaller values start from 'splitIndex'.
	for (i=startIndex;i<endIndex;i++)
	{
		SimdVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		if (center[splitAxis] > splitValue)
		{
			//swap
			OptimizedBvhNode tmp = leafNodes[i];
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


int	OptimizedBvh::CalcSplittingAxis(NodeArray&	leafNodes,int startIndex,int endIndex)
{
	int i;

	SimdVector3 means(0.f,0.f,0.f);
	SimdVector3 variance(0.f,0.f,0.f);
	int numIndices = endIndex-startIndex;

	for (i=startIndex;i<endIndex;i++)
	{
		SimdVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		means+=center;
	}
	means *= (1.f/(float)numIndices);
		
	for (i=startIndex;i<endIndex;i++)
	{
		SimdVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		SimdVector3 diff2 = center-means;
		diff2 = diff2 * diff2;
		variance += diff2;
	}
	variance *= (1.f/	((float)numIndices-1)	);
	
	return variance.maxAxis();
}


	
void	OptimizedBvh::ReportAabbOverlappingNodex(NodeOverlapCallback* nodeCallback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{
	if (aabbMin.length() > 1000.f)
	{
		for (size_t i=0;i<m_leafNodes.size();i++)
		{
			const OptimizedBvhNode&	node = m_leafNodes[i];
			nodeCallback->ProcessNode(&node);
		}
	} else
	{
		//WalkTree(m_rootNode1,nodeCallback,aabbMin,aabbMax);
		WalkStacklessTree(m_rootNode1,nodeCallback,aabbMin,aabbMax);
	}
}

void	OptimizedBvh::WalkTree(OptimizedBvhNode* rootNode,NodeOverlapCallback* nodeCallback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{
	bool isLeafNode, aabbOverlap = TestAabbAgainstAabb2(aabbMin,aabbMax,rootNode->m_aabbMin,rootNode->m_aabbMax);
	if (aabbOverlap)
	{
		isLeafNode = (!rootNode->m_leftChild && !rootNode->m_rightChild);
		if (isLeafNode)
		{
			nodeCallback->ProcessNode(rootNode);
		} else
		{
			WalkTree(rootNode->m_leftChild,nodeCallback,aabbMin,aabbMax);
			WalkTree(rootNode->m_rightChild,nodeCallback,aabbMin,aabbMax);
		}
	}

}

int maxIterations = 0;

void	OptimizedBvh::WalkStacklessTree(OptimizedBvhNode* rootNode,NodeOverlapCallback* nodeCallback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
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
			nodeCallback->ProcessNode(rootNode);
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


void	OptimizedBvh::ReportSphereOverlappingNodex(NodeOverlapCallback* nodeCallback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{

}

