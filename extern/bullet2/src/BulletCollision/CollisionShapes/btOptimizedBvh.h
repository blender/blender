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


#include "LinearMath/btVector3.h"


//http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vclang/html/vclrf__m128.asp


#include <vector>


class btStridingMeshInterface;

/// btOptimizedBvhNode contains both internal and leaf node information.
/// It hasn't been optimized yet for storage. Some obvious optimizations are:
/// Removal of the pointers (can already be done, they are not used for traversal)
/// and storing aabbmin/max as quantized integers.
/// 'subpart' doesn't need an integer either. It allows to re-use graphics triangle
/// meshes stored in a non-uniform way (like batches/subparts of triangle-fans
ATTRIBUTE_ALIGNED16 (struct btOptimizedBvhNode)
{

	btVector3	m_aabbMin;
	btVector3	m_aabbMax;

//these 2 pointers are obsolete, the stackless traversal just uses the escape index
	btOptimizedBvhNode*	m_leftChild;
	btOptimizedBvhNode*	m_rightChild;

	int	m_escapeIndex;

	//for child nodes
	int	m_subPart;
	int	m_triangleIndex;

};

class btNodeOverlapCallback
{
public:
	virtual ~btNodeOverlapCallback() {};

	virtual void processNode(const btOptimizedBvhNode* node) = 0;
};

#include "LinearMath/btAlignedAllocator.h"
#include "LinearMath/btAlignedObjectArray.h"

//typedef std::vector< unsigned , allocator_type >     container_type;
const unsigned size = (1 << 20);
typedef btAlignedAllocator< btOptimizedBvhNode , size >  allocator_type;

//typedef btAlignedObjectArray<btOptimizedBvhNode, allocator_type>	NodeArray;

typedef btAlignedObjectArray<btOptimizedBvhNode>	NodeArray;


///OptimizedBvh store an AABB tree that can be quickly traversed on CPU (and SPU,GPU in future)
class btOptimizedBvh
{
	NodeArray			m_leafNodes;

	btOptimizedBvhNode*	m_rootNode1;
	
	btOptimizedBvhNode*	m_contiguousNodes;
	int					m_curNodeIndex;

	int					m_numNodes;



public:
	btOptimizedBvh();

	virtual ~btOptimizedBvh();
	
	void	build(btStridingMeshInterface* triangles);

	btOptimizedBvhNode*	buildTree	(NodeArray&	leafNodes,int startIndex,int endIndex);

	int	calcSplittingAxis(NodeArray&	leafNodes,int startIndex,int endIndex);

	int	sortAndCalcSplittingIndex(NodeArray&	leafNodes,int startIndex,int endIndex,int splitAxis);
	
	void	walkTree(btOptimizedBvhNode* rootNode,btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const;
	
	void	walkStacklessTree(btOptimizedBvhNode* rootNode,btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const;
	

	//OptimizedBvhNode*	GetRootNode() { return m_rootNode1;}

	int					getNumNodes() { return m_numNodes;}

	void	reportAabbOverlappingNodex(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const;

	void	reportSphereOverlappingNodex(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const;


};


#endif //OPTIMIZED_BVH_H

