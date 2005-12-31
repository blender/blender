/*
 * Copyright (c) 2006 Erwin Coumans http://continuousphysics.com/Bullet/
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.
 * Erwin Coumans makes no representations about the suitability 
 * of this software for any purpose.  
 * It is provided "as is" without express or implied warranty.
*/

#include "OptimizedBvh.h"
#include "StridingMeshInterface.h"
#include "AabbUtil2.h"



void OptimizedBvh::Build(StridingMeshInterface* triangles)
{
	int countTriangles = 0;

	

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

	int numIndices =endIndex-startIndex;
	assert(numIndices>0);

	int curIndex = m_curNodeIndex;

	if (numIndices==1)
	{
		return new (&m_contiguousNodes[m_curNodeIndex++]) OptimizedBvhNode(leafNodes[startIndex]);
	}
	//calculate Best Splitting Axis and where to split it. Sort the incoming 'leafNodes' array within range 'startIndex/endIndex'.
	
	int splitAxis = CalcSplittingAxis(leafNodes,startIndex,endIndex);

	int splitIndex = SortAndCalcSplittingIndex(leafNodes,startIndex,endIndex,splitAxis);

	OptimizedBvhNode* internalNode = &m_contiguousNodes[m_curNodeIndex++];
	
	internalNode->m_aabbMax.setValue(-1e30f,-1e30f,-1e30f);
	internalNode->m_aabbMin.setValue(1e30f,1e30f,1e30f);
	
	for (int i=startIndex;i<endIndex;i++)
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
	int splitIndex =startIndex;
	int numIndices = endIndex - startIndex;

	SimdVector3 means(0.f,0.f,0.f);
	for (int i=startIndex;i<endIndex;i++)
	{
		SimdVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		means+=center;
	}
	means *= (1.f/(float)numIndices);
	
	float splitValue = means[splitAxis];
	
	//sort leafNodes so all values larger then splitValue comes first, and smaller values start from 'splitIndex'.
	for (int i=startIndex;i<endIndex;i++)
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
	SimdVector3 means(0.f,0.f,0.f);
	int numIndices = endIndex-startIndex;

	for (int i=startIndex;i<endIndex;i++)
	{
		SimdVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		means+=center;
	}
	means *= (1.f/(float)numIndices);
		
	SimdVector3 variance(0.f,0.f,0.f);

	for (int i=startIndex;i<endIndex;i++)
	{
		SimdVector3 center = 0.5f*(leafNodes[i].m_aabbMax+leafNodes[i].m_aabbMin);
		SimdVector3 diff2 = center-means;
		diff2 = diff2 * diff2;
		variance += diff2;
	}
	variance *= (1.f/	((float)numIndices-1)	);
	
	int biggestAxis = variance.maxAxis();
	return biggestAxis;

}


	
void	OptimizedBvh::ReportAabbOverlappingNodex(NodeOverlapCallback* nodeCallback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const
{
	if (aabbMin.length() > 1000.f)
	{
		for (int i=0;i<m_leafNodes.size();i++)
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
	bool aabbOverlap = TestAabbAgainstAabb2(aabbMin,aabbMax,rootNode->m_aabbMin,rootNode->m_aabbMax);
	if (aabbOverlap)
	{
		bool isLeafNode = (!rootNode->m_leftChild && !rootNode->m_rightChild);
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
	int curIndex = 0;
	int walkIterations = 0;

	while (curIndex < m_curNodeIndex)
	{
		//catch bugs in tree data
		assert (walkIterations < m_curNodeIndex);

		walkIterations++;
		bool aabbOverlap = TestAabbAgainstAabb2(aabbMin,aabbMax,rootNode->m_aabbMin,rootNode->m_aabbMax);
		bool isLeafNode = (!rootNode->m_leftChild && !rootNode->m_rightChild);
		
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
			int escapeIndex = rootNode->m_escapeIndex;
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

