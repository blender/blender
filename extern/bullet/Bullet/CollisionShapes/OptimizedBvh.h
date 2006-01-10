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

#ifndef OPTIMIZED_BVH_H
#define OPTIMIZED_BVH_H
#include "SimdVector3.h"
#include <vector>

class StridingMeshInterface;

/// OptimizedBvhNode contains both internal and leaf node information.
/// It hasn't been optimized yet for storage. Some obvious optimizations are:
/// Removal of the pointers (can already be done, they are not used for traversal)
/// and storing aabbmin/max as quantized integers.
/// 'subpart' doesn't need an integer either. It allows to re-use graphics triangle
/// meshes stored in a non-uniform way (like batches/subparts of triangle-fans
struct OptimizedBvhNode
{

	SimdVector3	m_aabbMin;
	SimdVector3	m_aabbMax;

//these 2 pointers are obsolete, the stackless traversal just uses the escape index
	OptimizedBvhNode*	m_leftChild;
	OptimizedBvhNode*	m_rightChild;

	int	m_escapeIndex;

	//for child nodes
	int	m_subPart;
	int	m_triangleIndex;

};

class NodeOverlapCallback
{
public:
	virtual void ProcessNode(const OptimizedBvhNode* node) = 0;
};

typedef std::vector<OptimizedBvhNode>	NodeArray;


///OptimizedBvh store an AABB tree that can be quickly traversed on CPU (and SPU,GPU in future)
class OptimizedBvh
{
	OptimizedBvhNode*	m_rootNode1;
	
	OptimizedBvhNode*	m_contiguousNodes;
	int					m_curNodeIndex;

	int					m_numNodes;

	NodeArray			m_leafNodes;

public:
	OptimizedBvh() :m_rootNode1(0), m_numNodes(0) { }
	
	void	Build(StridingMeshInterface* triangles);

	OptimizedBvhNode*	BuildTree	(NodeArray&	leafNodes,int startIndex,int endIndex);

	int	CalcSplittingAxis(NodeArray&	leafNodes,int startIndex,int endIndex);

	int	SortAndCalcSplittingIndex(NodeArray&	leafNodes,int startIndex,int endIndex,int splitAxis);
	
	void	WalkTree(OptimizedBvhNode* rootNode,NodeOverlapCallback* nodeCallback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const;
	
	void	WalkStacklessTree(OptimizedBvhNode* rootNode,NodeOverlapCallback* nodeCallback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const;
	

	//OptimizedBvhNode*	GetRootNode() { return m_rootNode1;}

	int					GetNumNodes() { return m_numNodes;}

	void	ReportAabbOverlappingNodex(NodeOverlapCallback* nodeCallback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const;

	void	ReportSphereOverlappingNodex(NodeOverlapCallback* nodeCallback,const SimdVector3& aabbMin,const SimdVector3& aabbMax) const;


};


#endif //OPTIMIZED_BVH_H