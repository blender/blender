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
	virtual ~NodeOverlapCallback() {};

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
	virtual ~OptimizedBvh() {};
	
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

