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
#include "LinearMath/btIDebugDraw.h"


btOptimizedBvh::btOptimizedBvh() : m_useQuantization(false), 
					//m_traversalMode(TRAVERSAL_STACKLESS_CACHE_FRIENDLY)
					m_traversalMode(TRAVERSAL_STACKLESS)
					//m_traversalMode(TRAVERSAL_RECURSIVE)
					,m_subtreeHeaderCount(0) //PCK: add this line
{ 

}


void btOptimizedBvh::build(btStridingMeshInterface* triangles, bool useQuantizedAabbCompression, const btVector3& bvhAabbMin, const btVector3& bvhAabbMax)
{
	m_useQuantization = useQuantizedAabbCompression;


	// NodeArray	triangleNodes;

	struct	NodeTriangleCallback : public btInternalTriangleIndexCallback
	{

		NodeArray&	m_triangleNodes;

		NodeTriangleCallback& operator=(NodeTriangleCallback& other)
		{
			m_triangleNodes = other.m_triangleNodes;
			return *this;
		}
		
		NodeTriangleCallback(NodeArray&	triangleNodes)
			:m_triangleNodes(triangleNodes)
		{
		}

		virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
		{
			btOptimizedBvhNode node;
			btVector3	aabbMin,aabbMax;
			aabbMin.setValue(btScalar(1e30),btScalar(1e30),btScalar(1e30));
			aabbMax.setValue(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30)); 
			aabbMin.setMin(triangle[0]);
			aabbMax.setMax(triangle[0]);
			aabbMin.setMin(triangle[1]);
			aabbMax.setMax(triangle[1]);
			aabbMin.setMin(triangle[2]);
			aabbMax.setMax(triangle[2]);

			//with quantization?
			node.m_aabbMinOrg = aabbMin;
			node.m_aabbMaxOrg = aabbMax;

			node.m_escapeIndex = -1;
	
			//for child nodes
			node.m_subPart = partId;
			node.m_triangleIndex = triangleIndex;
			m_triangleNodes.push_back(node);
		}
	};
	struct	QuantizedNodeTriangleCallback : public btInternalTriangleIndexCallback
	{
		QuantizedNodeArray&	m_triangleNodes;
		const btOptimizedBvh* m_optimizedTree; // for quantization

		QuantizedNodeTriangleCallback& operator=(QuantizedNodeTriangleCallback& other)
		{
			m_triangleNodes = other.m_triangleNodes;
			m_optimizedTree = other.m_optimizedTree;
			return *this;
		}

		QuantizedNodeTriangleCallback(QuantizedNodeArray&	triangleNodes,const btOptimizedBvh* tree)
			:m_triangleNodes(triangleNodes),m_optimizedTree(tree)
		{
		}

		virtual void internalProcessTriangleIndex(btVector3* triangle,int partId,int  triangleIndex)
		{
			btAssert(partId==0);
			//negative indices are reserved for escapeIndex
			btAssert(triangleIndex>=0);

			btQuantizedBvhNode node;
			btVector3	aabbMin,aabbMax;
			aabbMin.setValue(btScalar(1e30),btScalar(1e30),btScalar(1e30));
			aabbMax.setValue(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30)); 
			aabbMin.setMin(triangle[0]);
			aabbMax.setMax(triangle[0]);
			aabbMin.setMin(triangle[1]);
			aabbMax.setMax(triangle[1]);
			aabbMin.setMin(triangle[2]);
			aabbMax.setMax(triangle[2]);

			//PCK: add these checks for zero dimensions of aabb
			const btScalar MIN_AABB_DIMENSION = btScalar(0.002);
			const btScalar MIN_AABB_HALF_DIMENSION = btScalar(0.001);
			if (aabbMax.x() - aabbMin.x() < MIN_AABB_DIMENSION)
			{
				aabbMax.setX(aabbMax.x() + MIN_AABB_HALF_DIMENSION);
				aabbMin.setX(aabbMin.x() - MIN_AABB_HALF_DIMENSION);
			}
			if (aabbMax.y() - aabbMin.y() < MIN_AABB_DIMENSION)
			{
				aabbMax.setY(aabbMax.y() + MIN_AABB_HALF_DIMENSION);
				aabbMin.setY(aabbMin.y() - MIN_AABB_HALF_DIMENSION);
			}
			if (aabbMax.z() - aabbMin.z() < MIN_AABB_DIMENSION)
			{
				aabbMax.setZ(aabbMax.z() + MIN_AABB_HALF_DIMENSION);
				aabbMin.setZ(aabbMin.z() - MIN_AABB_HALF_DIMENSION);
			}

			m_optimizedTree->quantizeWithClamp(&node.m_quantizedAabbMin[0],aabbMin);
			m_optimizedTree->quantizeWithClamp(&node.m_quantizedAabbMax[0],aabbMax);

			node.m_escapeIndexOrTriangleIndex = triangleIndex;

			m_triangleNodes.push_back(node);
		}
	};
	


	int numLeafNodes = 0;

	
	if (m_useQuantization)
	{

		//initialize quantization values
		setQuantizationValues(bvhAabbMin,bvhAabbMax);

		QuantizedNodeTriangleCallback	callback(m_quantizedLeafNodes,this);

	
		triangles->InternalProcessAllTriangles(&callback,m_bvhAabbMin,m_bvhAabbMax);

		//now we have an array of leafnodes in m_leafNodes
		numLeafNodes = m_quantizedLeafNodes.size();


		m_quantizedContiguousNodes.resize(2*numLeafNodes);


	} else
	{
		NodeTriangleCallback	callback(m_leafNodes);

		btVector3 aabbMin(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30));
		btVector3 aabbMax(btScalar(1e30),btScalar(1e30),btScalar(1e30));

		triangles->InternalProcessAllTriangles(&callback,aabbMin,aabbMax);

		//now we have an array of leafnodes in m_leafNodes
		numLeafNodes = m_leafNodes.size();

		m_contiguousNodes.resize(2*numLeafNodes);
	}

	m_curNodeIndex = 0;

	buildTree(0,numLeafNodes);

	///if the entire tree is small then subtree size, we need to create a header info for the tree
	if(m_useQuantization && !m_SubtreeHeaders.size())
	{
		btBvhSubtreeInfo& subtree = m_SubtreeHeaders.expand();
		subtree.setAabbFromQuantizeNode(m_quantizedContiguousNodes[0]);
		subtree.m_rootNodeIndex = 0;
		subtree.m_subtreeSize = m_quantizedContiguousNodes[0].isLeafNode() ? 1 : m_quantizedContiguousNodes[0].getEscapeIndex();
	}

	//PCK: update the copy of the size
	m_subtreeHeaderCount = m_SubtreeHeaders.size();

	//PCK: clear m_quantizedLeafNodes and m_leafNodes, they are temporary
	m_quantizedLeafNodes.clear();
	m_leafNodes.clear();
}



void	btOptimizedBvh::refitPartial(btStridingMeshInterface* meshInterface,const btVector3& aabbMin,const btVector3& aabbMax)
{
	//incrementally initialize quantization values
	btAssert(m_useQuantization);

	btAssert(aabbMin.getX() > m_bvhAabbMin.getX());
	btAssert(aabbMin.getY() > m_bvhAabbMin.getY());
	btAssert(aabbMin.getZ() > m_bvhAabbMin.getZ());

	btAssert(aabbMax.getX() < m_bvhAabbMax.getX());
	btAssert(aabbMax.getY() < m_bvhAabbMax.getY());
	btAssert(aabbMax.getZ() < m_bvhAabbMax.getZ());

	///we should update all quantization values, using updateBvhNodes(meshInterface);
	///but we only update chunks that overlap the given aabb
	
	unsigned short	quantizedQueryAabbMin[3];
	unsigned short	quantizedQueryAabbMax[3];

	quantizeWithClamp(&quantizedQueryAabbMin[0],aabbMin);
	quantizeWithClamp(&quantizedQueryAabbMax[0],aabbMax);

	int i;
	for (i=0;i<this->m_SubtreeHeaders.size();i++)
	{
		btBvhSubtreeInfo& subtree = m_SubtreeHeaders[i];

		//PCK: unsigned instead of bool
		unsigned overlap = testQuantizedAabbAgainstQuantizedAabb(quantizedQueryAabbMin,quantizedQueryAabbMax,subtree.m_quantizedAabbMin,subtree.m_quantizedAabbMax);
		if (overlap != 0)
		{
			updateBvhNodes(meshInterface,subtree.m_rootNodeIndex,subtree.m_rootNodeIndex+subtree.m_subtreeSize,i);

			subtree.setAabbFromQuantizeNode(m_quantizedContiguousNodes[subtree.m_rootNodeIndex]);
		}
	}
	
}

///just for debugging, to visualize the individual patches/subtrees
#ifdef DEBUG_PATCH_COLORS
btVector3 color[4]=
{
	btVector3(255,0,0),
	btVector3(0,255,0),
	btVector3(0,0,255),
	btVector3(0,255,255)
};
#endif //DEBUG_PATCH_COLORS


void	btOptimizedBvh::updateBvhNodes(btStridingMeshInterface* meshInterface,int firstNode,int endNode,int index)
{
	(void)index;

	btAssert(m_useQuantization);

	int nodeSubPart=0;

	//get access info to trianglemesh data
		const unsigned char *vertexbase;
		int numverts;
		PHY_ScalarType type;
		int stride;
		const unsigned char *indexbase;
		int indexstride;
		int numfaces;
		PHY_ScalarType indicestype;
		meshInterface->getLockedReadOnlyVertexIndexBase(&vertexbase,numverts,	type,stride,&indexbase,indexstride,numfaces,indicestype,nodeSubPart);

		btVector3	triangleVerts[3];
		btVector3	aabbMin,aabbMax;
		const btVector3& meshScaling = meshInterface->getScaling();
		
		int i;
		for (i=endNode-1;i>=firstNode;i--)
		{


			btQuantizedBvhNode& curNode = m_quantizedContiguousNodes[i];
			if (curNode.isLeafNode())
			{
				//recalc aabb from triangle data
				int nodeTriangleIndex = curNode.getTriangleIndex();
				//triangles->getLockedReadOnlyVertexIndexBase(vertexBase,numVerts,

				int* gfxbase = (int*)(indexbase+nodeTriangleIndex*indexstride);
				
				
				for (int j=2;j>=0;j--)
				{
					
					int graphicsindex = gfxbase[j];
					btScalar* graphicsbase = (btScalar*)(vertexbase+graphicsindex*stride);
#ifdef DEBUG_PATCH_COLORS
					btVector3 mycolor = color[index&3];
					graphicsbase[8] = mycolor.getX();
					graphicsbase[9] = mycolor.getY();
					graphicsbase[10] = mycolor.getZ();
#endif //DEBUG_PATCH_COLORS


					triangleVerts[j] = btVector3(
						graphicsbase[0]*meshScaling.getX(),
						graphicsbase[1]*meshScaling.getY(),
						graphicsbase[2]*meshScaling.getZ());
				}


				
				aabbMin.setValue(btScalar(1e30),btScalar(1e30),btScalar(1e30));
				aabbMax.setValue(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30)); 
				aabbMin.setMin(triangleVerts[0]);
				aabbMax.setMax(triangleVerts[0]);
				aabbMin.setMin(triangleVerts[1]);
				aabbMax.setMax(triangleVerts[1]);
				aabbMin.setMin(triangleVerts[2]);
				aabbMax.setMax(triangleVerts[2]);

				quantizeWithClamp(&curNode.m_quantizedAabbMin[0],aabbMin);
				quantizeWithClamp(&curNode.m_quantizedAabbMax[0],aabbMax);
				
			} else
			{
				//combine aabb from both children

				btQuantizedBvhNode* leftChildNode = &m_quantizedContiguousNodes[i+1];
				
				btQuantizedBvhNode* rightChildNode = leftChildNode->isLeafNode() ? &m_quantizedContiguousNodes[i+2] :
					&m_quantizedContiguousNodes[i+1+leftChildNode->getEscapeIndex()];
				

				{
					for (int i=0;i<3;i++)
					{
						curNode.m_quantizedAabbMin[i] = leftChildNode->m_quantizedAabbMin[i];
						if (curNode.m_quantizedAabbMin[i]>rightChildNode->m_quantizedAabbMin[i])
							curNode.m_quantizedAabbMin[i]=rightChildNode->m_quantizedAabbMin[i];

						curNode.m_quantizedAabbMax[i] = leftChildNode->m_quantizedAabbMax[i];
						if (curNode.m_quantizedAabbMax[i] < rightChildNode->m_quantizedAabbMax[i])
							curNode.m_quantizedAabbMax[i] = rightChildNode->m_quantizedAabbMax[i];
					}
				}
			}

		}

		meshInterface->unLockReadOnlyVertexBase(nodeSubPart);

		
}

void	btOptimizedBvh::setQuantizationValues(const btVector3& bvhAabbMin,const btVector3& bvhAabbMax,btScalar quantizationMargin)
{
	//enlarge the AABB to avoid division by zero when initializing the quantization values
	btVector3 clampValue(quantizationMargin,quantizationMargin,quantizationMargin);
	m_bvhAabbMin = bvhAabbMin - clampValue;
	m_bvhAabbMax = bvhAabbMax + clampValue;
	btVector3 aabbSize = m_bvhAabbMax - m_bvhAabbMin;
	m_bvhQuantization = btVector3(btScalar(65535.0),btScalar(65535.0),btScalar(65535.0)) / aabbSize;
}


void	btOptimizedBvh::refit(btStridingMeshInterface* meshInterface)
{
	if (m_useQuantization)
	{
		//calculate new aabb
		btVector3 aabbMin,aabbMax;
		meshInterface->calculateAabbBruteForce(aabbMin,aabbMax);

		setQuantizationValues(aabbMin,aabbMax);

		updateBvhNodes(meshInterface,0,m_curNodeIndex,0);

		///now update all subtree headers

		int i;
		for (i=0;i<m_SubtreeHeaders.size();i++)
		{
			btBvhSubtreeInfo& subtree = m_SubtreeHeaders[i];
			subtree.setAabbFromQuantizeNode(m_quantizedContiguousNodes[subtree.m_rootNodeIndex]);
		}

	} else
	{

	}
}



btOptimizedBvh::~btOptimizedBvh()
{
}

#ifdef DEBUG_TREE_BUILDING
int gStackDepth = 0;
int gMaxStackDepth = 0;
#endif //DEBUG_TREE_BUILDING

void	btOptimizedBvh::buildTree	(int startIndex,int endIndex)
{
#ifdef DEBUG_TREE_BUILDING
	gStackDepth++;
	if (gStackDepth > gMaxStackDepth)
		gMaxStackDepth = gStackDepth;
#endif //DEBUG_TREE_BUILDING


	int splitAxis, splitIndex, i;
	int numIndices =endIndex-startIndex;
	int curIndex = m_curNodeIndex;

	assert(numIndices>0);

	if (numIndices==1)
	{
#ifdef DEBUG_TREE_BUILDING
		gStackDepth--;
#endif //DEBUG_TREE_BUILDING
		
		assignInternalNodeFromLeafNode(m_curNodeIndex,startIndex);

		m_curNodeIndex++;
		return;	
	}
	//calculate Best Splitting Axis and where to split it. Sort the incoming 'leafNodes' array within range 'startIndex/endIndex'.
	
	splitAxis = calcSplittingAxis(startIndex,endIndex);

	splitIndex = sortAndCalcSplittingIndex(startIndex,endIndex,splitAxis);

	int internalNodeIndex = m_curNodeIndex;
	
	setInternalNodeAabbMax(m_curNodeIndex,btVector3(btScalar(-1e30),btScalar(-1e30),btScalar(-1e30)));
	setInternalNodeAabbMin(m_curNodeIndex,btVector3(btScalar(1e30),btScalar(1e30),btScalar(1e30)));
	
	for (i=startIndex;i<endIndex;i++)
	{
		mergeInternalNodeAabb(m_curNodeIndex,getAabbMin(i),getAabbMax(i));
	}

	m_curNodeIndex++;
	

	//internalNode->m_escapeIndex;
	
	int leftChildNodexIndex = m_curNodeIndex;

	//build left child tree
	buildTree(startIndex,splitIndex);

	int rightChildNodexIndex = m_curNodeIndex;
	//build right child tree
	buildTree(splitIndex,endIndex);

#ifdef DEBUG_TREE_BUILDING
	gStackDepth--;
#endif //DEBUG_TREE_BUILDING

	int escapeIndex = m_curNodeIndex - curIndex;

	if (m_useQuantization)
	{
		//escapeIndex is the number of nodes of this subtree
		const int sizeQuantizedNode =sizeof(btQuantizedBvhNode);
		const int treeSizeInBytes = escapeIndex * sizeQuantizedNode;
		if (treeSizeInBytes > MAX_SUBTREE_SIZE_IN_BYTES)
		{
			updateSubtreeHeaders(leftChildNodexIndex,rightChildNodexIndex);
		}
	}

	setInternalNodeEscapeIndex(internalNodeIndex,escapeIndex);

}

void	btOptimizedBvh::updateSubtreeHeaders(int leftChildNodexIndex,int rightChildNodexIndex)
{
	btAssert(m_useQuantization);

	btQuantizedBvhNode& leftChildNode = m_quantizedContiguousNodes[leftChildNodexIndex];
	int leftSubTreeSize = leftChildNode.isLeafNode() ? 1 : leftChildNode.getEscapeIndex();
	int leftSubTreeSizeInBytes =  leftSubTreeSize * sizeof(btQuantizedBvhNode);
	
	btQuantizedBvhNode& rightChildNode = m_quantizedContiguousNodes[rightChildNodexIndex];
	int rightSubTreeSize = rightChildNode.isLeafNode() ? 1 : rightChildNode.getEscapeIndex();
	int rightSubTreeSizeInBytes =  rightSubTreeSize * sizeof(btQuantizedBvhNode);

	if(leftSubTreeSizeInBytes <= MAX_SUBTREE_SIZE_IN_BYTES)
	{
		btBvhSubtreeInfo& subtree = m_SubtreeHeaders.expand();
		subtree.setAabbFromQuantizeNode(leftChildNode);
		subtree.m_rootNodeIndex = leftChildNodexIndex;
		subtree.m_subtreeSize = leftSubTreeSize;
	}

	if(rightSubTreeSizeInBytes <= MAX_SUBTREE_SIZE_IN_BYTES)
	{
		btBvhSubtreeInfo& subtree = m_SubtreeHeaders.expand();
		subtree.setAabbFromQuantizeNode(rightChildNode);
		subtree.m_rootNodeIndex = rightChildNodexIndex;
		subtree.m_subtreeSize = rightSubTreeSize;
	}

	//PCK: update the copy of the size
	m_subtreeHeaderCount = m_SubtreeHeaders.size();
}


int	btOptimizedBvh::sortAndCalcSplittingIndex(int startIndex,int endIndex,int splitAxis)
{
	int i;
	int splitIndex =startIndex;
	int numIndices = endIndex - startIndex;
	btScalar splitValue;

	btVector3 means(btScalar(0.),btScalar(0.),btScalar(0.));
	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = btScalar(0.5)*(getAabbMax(i)+getAabbMin(i));
		means+=center;
	}
	means *= (btScalar(1.)/(btScalar)numIndices);
	
	splitValue = means[splitAxis];
	
	//sort leafNodes so all values larger then splitValue comes first, and smaller values start from 'splitIndex'.
	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = btScalar(0.5)*(getAabbMax(i)+getAabbMin(i));
		if (center[splitAxis] > splitValue)
		{
			//swap
			swapLeafNodes(i,splitIndex);
			splitIndex++;
		}
	}

	//if the splitIndex causes unbalanced trees, fix this by using the center in between startIndex and endIndex
	//otherwise the tree-building might fail due to stack-overflows in certain cases.
	//unbalanced1 is unsafe: it can cause stack overflows
	//bool unbalanced1 = ((splitIndex==startIndex) || (splitIndex == (endIndex-1)));

	//unbalanced2 should work too: always use center (perfect balanced trees)	
	//bool unbalanced2 = true;

	//this should be safe too:
	int rangeBalancedIndices = numIndices/3;
	bool unbalanced = ((splitIndex<=(startIndex+rangeBalancedIndices)) || (splitIndex >=(endIndex-1-rangeBalancedIndices)));
	
	if (unbalanced)
	{
		splitIndex = startIndex+ (numIndices>>1);
	}

	bool unbal = (splitIndex==startIndex) || (splitIndex == (endIndex));
	btAssert(!unbal);

	return splitIndex;
}


int	btOptimizedBvh::calcSplittingAxis(int startIndex,int endIndex)
{
	int i;

	btVector3 means(btScalar(0.),btScalar(0.),btScalar(0.));
	btVector3 variance(btScalar(0.),btScalar(0.),btScalar(0.));
	int numIndices = endIndex-startIndex;

	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = btScalar(0.5)*(getAabbMax(i)+getAabbMin(i));
		means+=center;
	}
	means *= (btScalar(1.)/(btScalar)numIndices);
		
	for (i=startIndex;i<endIndex;i++)
	{
		btVector3 center = btScalar(0.5)*(getAabbMax(i)+getAabbMin(i));
		btVector3 diff2 = center-means;
		diff2 = diff2 * diff2;
		variance += diff2;
	}
	variance *= (btScalar(1.)/	((btScalar)numIndices-1)	);
	
	return variance.maxAxis();
}



void	btOptimizedBvh::reportAabbOverlappingNodex(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	//either choose recursive traversal (walkTree) or stackless (walkStacklessTree)

	if (m_useQuantization)
	{
		///quantize query AABB
		unsigned short int quantizedQueryAabbMin[3];
		unsigned short int quantizedQueryAabbMax[3];
		quantizeWithClamp(quantizedQueryAabbMin,aabbMin);
		quantizeWithClamp(quantizedQueryAabbMax,aabbMax);

		switch (m_traversalMode)
		{
		case TRAVERSAL_STACKLESS:
				walkStacklessQuantizedTree(nodeCallback,quantizedQueryAabbMin,quantizedQueryAabbMax,0,m_curNodeIndex);
			break;
		case TRAVERSAL_STACKLESS_CACHE_FRIENDLY:
				walkStacklessQuantizedTreeCacheFriendly(nodeCallback,quantizedQueryAabbMin,quantizedQueryAabbMax);
			break;
		case TRAVERSAL_RECURSIVE:
			{
				const btQuantizedBvhNode* rootNode = &m_quantizedContiguousNodes[0];
				walkRecursiveQuantizedTreeAgainstQueryAabb(rootNode,nodeCallback,quantizedQueryAabbMin,quantizedQueryAabbMax);
			}
			break;
		default:
			//unsupported
			btAssert(0);
		}
	} else
	{
		walkStacklessTree(nodeCallback,aabbMin,aabbMax);
	}
}


int maxIterations = 0;

void	btOptimizedBvh::walkStacklessTree(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	btAssert(!m_useQuantization);

	const btOptimizedBvhNode* rootNode = &m_contiguousNodes[0];
	int escapeIndex, curIndex = 0;
	int walkIterations = 0;
	bool isLeafNode;
	//PCK: unsigned instead of bool
	unsigned aabbOverlap;

	while (curIndex < m_curNodeIndex)
	{
		//catch bugs in tree data
		assert (walkIterations < m_curNodeIndex);

		walkIterations++;
		aabbOverlap = TestAabbAgainstAabb2(aabbMin,aabbMax,rootNode->m_aabbMinOrg,rootNode->m_aabbMaxOrg);
		isLeafNode = rootNode->m_escapeIndex == -1;
		
		//PCK: unsigned instead of bool
		if (isLeafNode && (aabbOverlap != 0))
		{
			nodeCallback->processNode(rootNode->m_subPart,rootNode->m_triangleIndex);
		} 
		
		//PCK: unsigned instead of bool
		if ((aabbOverlap != 0) || isLeafNode)
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

/*
///this was the original recursive traversal, before we optimized towards stackless traversal
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
*/

void btOptimizedBvh::walkRecursiveQuantizedTreeAgainstQueryAabb(const btQuantizedBvhNode* currentNode,btNodeOverlapCallback* nodeCallback,unsigned short int* quantizedQueryAabbMin,unsigned short int* quantizedQueryAabbMax) const
{
	btAssert(m_useQuantization);
	
	bool isLeafNode;
	//PCK: unsigned instead of bool
	unsigned aabbOverlap;

	//PCK: unsigned instead of bool
	aabbOverlap = testQuantizedAabbAgainstQuantizedAabb(quantizedQueryAabbMin,quantizedQueryAabbMax,currentNode->m_quantizedAabbMin,currentNode->m_quantizedAabbMax);
	isLeafNode = currentNode->isLeafNode();
		
	//PCK: unsigned instead of bool
	if (aabbOverlap != 0)
	{
		if (isLeafNode)
		{
			nodeCallback->processNode(0,currentNode->getTriangleIndex());
		} else
		{
			//process left and right children
			const btQuantizedBvhNode* leftChildNode = currentNode+1;
			walkRecursiveQuantizedTreeAgainstQueryAabb(leftChildNode,nodeCallback,quantizedQueryAabbMin,quantizedQueryAabbMax);

			const btQuantizedBvhNode* rightChildNode = leftChildNode->isLeafNode() ? leftChildNode+1:leftChildNode+leftChildNode->getEscapeIndex();
			walkRecursiveQuantizedTreeAgainstQueryAabb(rightChildNode,nodeCallback,quantizedQueryAabbMin,quantizedQueryAabbMax);
		}
	}		
}







void	btOptimizedBvh::walkStacklessQuantizedTree(btNodeOverlapCallback* nodeCallback,unsigned short int* quantizedQueryAabbMin,unsigned short int* quantizedQueryAabbMax,int startNodeIndex,int endNodeIndex) const
{
	btAssert(m_useQuantization);
	
	int curIndex = startNodeIndex;
	int walkIterations = 0;
	int subTreeSize = endNodeIndex - startNodeIndex;

	const btQuantizedBvhNode* rootNode = &m_quantizedContiguousNodes[startNodeIndex];
	int escapeIndex;
	
	bool isLeafNode;
	//PCK: unsigned instead of bool
	unsigned aabbOverlap;

	while (curIndex < endNodeIndex)
	{

//#define VISUALLY_ANALYZE_BVH 1
#ifdef VISUALLY_ANALYZE_BVH
		//some code snippet to debugDraw aabb, to visually analyze bvh structure
		static int drawPatch = 0;
		//need some global access to a debugDrawer
		extern btIDebugDraw* debugDrawerPtr;
		if (curIndex==drawPatch)
		{
			btVector3 aabbMin,aabbMax;
			aabbMin = unQuantize(rootNode->m_quantizedAabbMin);
			aabbMax = unQuantize(rootNode->m_quantizedAabbMax);
			btVector3	color(1,0,0);
			debugDrawerPtr->drawAabb(aabbMin,aabbMax,color);
		}
#endif//VISUALLY_ANALYZE_BVH

		//catch bugs in tree data
		assert (walkIterations < subTreeSize);

		walkIterations++;
		//PCK: unsigned instead of bool
		aabbOverlap = testQuantizedAabbAgainstQuantizedAabb(quantizedQueryAabbMin,quantizedQueryAabbMax,rootNode->m_quantizedAabbMin,rootNode->m_quantizedAabbMax);
		isLeafNode = rootNode->isLeafNode();
		
		if (isLeafNode && aabbOverlap)
		{
			nodeCallback->processNode(0,rootNode->getTriangleIndex());
		} 
		
		//PCK: unsigned instead of bool
		if ((aabbOverlap != 0) || isLeafNode)
		{
			rootNode++;
			curIndex++;
		} else
		{
			escapeIndex = rootNode->getEscapeIndex();
			rootNode += escapeIndex;
			curIndex += escapeIndex;
		}
	}
	if (maxIterations < walkIterations)
		maxIterations = walkIterations;

}

//This traversal can be called from Playstation 3 SPU
void	btOptimizedBvh::walkStacklessQuantizedTreeCacheFriendly(btNodeOverlapCallback* nodeCallback,unsigned short int* quantizedQueryAabbMin,unsigned short int* quantizedQueryAabbMax) const
{
	btAssert(m_useQuantization);

	int i;


	for (i=0;i<this->m_SubtreeHeaders.size();i++)
	{
		const btBvhSubtreeInfo& subtree = m_SubtreeHeaders[i];

		//PCK: unsigned instead of bool
		unsigned overlap = testQuantizedAabbAgainstQuantizedAabb(quantizedQueryAabbMin,quantizedQueryAabbMax,subtree.m_quantizedAabbMin,subtree.m_quantizedAabbMax);
		if (overlap != 0)
		{
			walkStacklessQuantizedTree(nodeCallback,quantizedQueryAabbMin,quantizedQueryAabbMax,
				subtree.m_rootNodeIndex,
				subtree.m_rootNodeIndex+subtree.m_subtreeSize);
		}
	}
}




void	btOptimizedBvh::reportSphereOverlappingNodex(btNodeOverlapCallback* nodeCallback,const btVector3& aabbMin,const btVector3& aabbMax) const
{
	(void)nodeCallback;
	(void)aabbMin;
	(void)aabbMax;
	//not yet, please use aabb
	btAssert(0);
}




btVector3	btOptimizedBvh::unQuantize(const unsigned short* vecIn) const
{
	btVector3	vecOut;
	vecOut.setValue(
		(btScalar)(vecIn[0]) / (m_bvhQuantization.getX()),
		(btScalar)(vecIn[1]) / (m_bvhQuantization.getY()),
		(btScalar)(vecIn[2]) / (m_bvhQuantization.getZ()));
	vecOut += m_bvhAabbMin;
	return vecOut;
}


void	btOptimizedBvh::swapLeafNodes(int i,int splitIndex)
{
	if (m_useQuantization)
	{
			btQuantizedBvhNode tmp = m_quantizedLeafNodes[i];
			m_quantizedLeafNodes[i] = m_quantizedLeafNodes[splitIndex];
			m_quantizedLeafNodes[splitIndex] = tmp;
	} else
	{
			btOptimizedBvhNode tmp = m_leafNodes[i];
			m_leafNodes[i] = m_leafNodes[splitIndex];
			m_leafNodes[splitIndex] = tmp;
	}
}

void	btOptimizedBvh::assignInternalNodeFromLeafNode(int internalNode,int leafNodeIndex)
{
	if (m_useQuantization)
	{
		m_quantizedContiguousNodes[internalNode] = m_quantizedLeafNodes[leafNodeIndex];
	} else
	{
		m_contiguousNodes[internalNode] = m_leafNodes[leafNodeIndex];
	}
}

//PCK: include
#include <new>

//PCK: consts
static const unsigned BVH_ALIGNMENT = 16;
static const unsigned BVH_ALIGNMENT_MASK = BVH_ALIGNMENT-1;

static const unsigned BVH_ALIGNMENT_BLOCKS = 2;



unsigned int btOptimizedBvh::getAlignmentSerializationPadding()
{
	return BVH_ALIGNMENT_BLOCKS * BVH_ALIGNMENT;
}

unsigned btOptimizedBvh::calculateSerializeBufferSize()
{
	unsigned baseSize = sizeof(btOptimizedBvh) + getAlignmentSerializationPadding();
	baseSize += sizeof(btBvhSubtreeInfo) * m_subtreeHeaderCount;
	if (m_useQuantization)
	{
		return baseSize + m_curNodeIndex * sizeof(btQuantizedBvhNode);
	}
	return baseSize + m_curNodeIndex * sizeof(btOptimizedBvhNode);
}

bool btOptimizedBvh::serialize(void *o_alignedDataBuffer, unsigned i_dataBufferSize, bool i_swapEndian)
{
	assert(m_subtreeHeaderCount == m_SubtreeHeaders.size());
	m_subtreeHeaderCount = m_SubtreeHeaders.size();

/*	if (i_dataBufferSize < calculateSerializeBufferSize() || o_alignedDataBuffer == NULL || (((unsigned)o_alignedDataBuffer & BVH_ALIGNMENT_MASK) != 0))
	{
		///check alignedment for buffer?
		btAssert(0);
		return false;
	}
*/

	btOptimizedBvh *targetBvh = (btOptimizedBvh *)o_alignedDataBuffer;

	// construct the class so the virtual function table, etc will be set up
	// Also, m_leafNodes and m_quantizedLeafNodes will be initialized to default values by the constructor
	new (targetBvh) btOptimizedBvh;

	if (i_swapEndian)
	{
		targetBvh->m_curNodeIndex = btSwapEndian(m_curNodeIndex);


		btSwapVector3Endian(m_bvhAabbMin,targetBvh->m_bvhAabbMin);
		btSwapVector3Endian(m_bvhAabbMax,targetBvh->m_bvhAabbMax);
		btSwapVector3Endian(m_bvhQuantization,targetBvh->m_bvhQuantization);

		targetBvh->m_traversalMode = (btTraversalMode)btSwapEndian(m_traversalMode);
		targetBvh->m_subtreeHeaderCount = btSwapEndian(m_subtreeHeaderCount);
	}
	else
	{
		targetBvh->m_curNodeIndex = m_curNodeIndex;
		targetBvh->m_bvhAabbMin = m_bvhAabbMin;
		targetBvh->m_bvhAabbMax = m_bvhAabbMax;
		targetBvh->m_bvhQuantization = m_bvhQuantization;
		targetBvh->m_traversalMode = m_traversalMode;
		targetBvh->m_subtreeHeaderCount = m_subtreeHeaderCount;
	}

	targetBvh->m_useQuantization = m_useQuantization;

	unsigned char *nodeData = (unsigned char *)targetBvh;
	nodeData += sizeof(btOptimizedBvh);
	
	unsigned sizeToAdd = 0;//(BVH_ALIGNMENT-((unsigned)nodeData & BVH_ALIGNMENT_MASK))&BVH_ALIGNMENT_MASK;
	nodeData += sizeToAdd;
	
	int nodeCount = m_curNodeIndex;

	if (m_useQuantization)
	{
		targetBvh->m_quantizedContiguousNodes.initializeFromBuffer(nodeData, nodeCount, nodeCount);

		if (i_swapEndian)
		{
			for (int nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
			{
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[0] = btSwapEndian(m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[0]);
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[1] = btSwapEndian(m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[1]);
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[2] = btSwapEndian(m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[2]);

				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[0] = btSwapEndian(m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[0]);
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[1] = btSwapEndian(m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[1]);
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[2] = btSwapEndian(m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[2]);

				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_escapeIndexOrTriangleIndex = btSwapEndian(m_quantizedContiguousNodes[nodeIndex].m_escapeIndexOrTriangleIndex);
			}
		}
		else
		{
			for (int nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
			{
	
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[0] = m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[0];
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[1] = m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[1];
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[2] = m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[2];

				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[0] = m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[0];
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[1] = m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[1];
				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[2] = m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[2];

				targetBvh->m_quantizedContiguousNodes[nodeIndex].m_escapeIndexOrTriangleIndex = m_quantizedContiguousNodes[nodeIndex].m_escapeIndexOrTriangleIndex;


			}
		}
		nodeData += sizeof(btQuantizedBvhNode) * nodeCount;
	}
	else
	{
		targetBvh->m_contiguousNodes.initializeFromBuffer(nodeData, nodeCount, nodeCount);

		if (i_swapEndian)
		{
			for (int nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
			{
				btSwapVector3Endian(m_contiguousNodes[nodeIndex].m_aabbMinOrg, targetBvh->m_contiguousNodes[nodeIndex].m_aabbMinOrg);
				btSwapVector3Endian(m_contiguousNodes[nodeIndex].m_aabbMaxOrg, targetBvh->m_contiguousNodes[nodeIndex].m_aabbMaxOrg);

				targetBvh->m_contiguousNodes[nodeIndex].m_escapeIndex = btSwapEndian(m_contiguousNodes[nodeIndex].m_escapeIndex);
				targetBvh->m_contiguousNodes[nodeIndex].m_subPart = btSwapEndian(m_contiguousNodes[nodeIndex].m_subPart);
				targetBvh->m_contiguousNodes[nodeIndex].m_triangleIndex = btSwapEndian(m_contiguousNodes[nodeIndex].m_triangleIndex);
			}
		}
		else
		{
			for (int nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
			{
				targetBvh->m_contiguousNodes[nodeIndex].m_aabbMinOrg = m_contiguousNodes[nodeIndex].m_aabbMinOrg;
				targetBvh->m_contiguousNodes[nodeIndex].m_aabbMaxOrg = m_contiguousNodes[nodeIndex].m_aabbMaxOrg;

				targetBvh->m_contiguousNodes[nodeIndex].m_escapeIndex = m_contiguousNodes[nodeIndex].m_escapeIndex;
				targetBvh->m_contiguousNodes[nodeIndex].m_subPart = m_contiguousNodes[nodeIndex].m_subPart;
				targetBvh->m_contiguousNodes[nodeIndex].m_triangleIndex = m_contiguousNodes[nodeIndex].m_triangleIndex;
			}
		}
		nodeData += sizeof(btOptimizedBvhNode) * nodeCount;
	}

	sizeToAdd = 0;//(BVH_ALIGNMENT-((unsigned)nodeData & BVH_ALIGNMENT_MASK))&BVH_ALIGNMENT_MASK;
	nodeData += sizeToAdd;

	// Now serialize the subtree headers
	targetBvh->m_SubtreeHeaders.initializeFromBuffer(nodeData, m_subtreeHeaderCount, m_subtreeHeaderCount);
	if (i_swapEndian)
	{
		for (int i = 0; i < m_subtreeHeaderCount; i++)
		{
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMin[0] = btSwapEndian(m_SubtreeHeaders[i].m_quantizedAabbMin[0]);
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMin[1] = btSwapEndian(m_SubtreeHeaders[i].m_quantizedAabbMin[1]);
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMin[2] = btSwapEndian(m_SubtreeHeaders[i].m_quantizedAabbMin[2]);

			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMax[0] = btSwapEndian(m_SubtreeHeaders[i].m_quantizedAabbMax[0]);
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMax[1] = btSwapEndian(m_SubtreeHeaders[i].m_quantizedAabbMax[1]);
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMax[2] = btSwapEndian(m_SubtreeHeaders[i].m_quantizedAabbMax[2]);

			targetBvh->m_SubtreeHeaders[i].m_rootNodeIndex = btSwapEndian(m_SubtreeHeaders[i].m_rootNodeIndex);
			targetBvh->m_SubtreeHeaders[i].m_subtreeSize = btSwapEndian(m_SubtreeHeaders[i].m_subtreeSize);
		}
	}
	else
	{
		for (int i = 0; i < m_subtreeHeaderCount; i++)
		{
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMin[0] = (m_SubtreeHeaders[i].m_quantizedAabbMin[0]);
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMin[1] = (m_SubtreeHeaders[i].m_quantizedAabbMin[1]);
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMin[2] = (m_SubtreeHeaders[i].m_quantizedAabbMin[2]);

			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMax[0] = (m_SubtreeHeaders[i].m_quantizedAabbMax[0]);
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMax[1] = (m_SubtreeHeaders[i].m_quantizedAabbMax[1]);
			targetBvh->m_SubtreeHeaders[i].m_quantizedAabbMax[2] = (m_SubtreeHeaders[i].m_quantizedAabbMax[2]);

			targetBvh->m_SubtreeHeaders[i].m_rootNodeIndex = (m_SubtreeHeaders[i].m_rootNodeIndex);
			targetBvh->m_SubtreeHeaders[i].m_subtreeSize = (m_SubtreeHeaders[i].m_subtreeSize);
			targetBvh->m_SubtreeHeaders[i] = m_SubtreeHeaders[i];
		}
	}

	nodeData += sizeof(btBvhSubtreeInfo) * m_subtreeHeaderCount;

	return true;
}

btOptimizedBvh *btOptimizedBvh::deSerializeInPlace(void *i_alignedDataBuffer, unsigned i_dataBufferSize, bool i_swapEndian)
{

	if (i_alignedDataBuffer == NULL)// || (((unsigned)i_alignedDataBuffer & BVH_ALIGNMENT_MASK) != 0))
	{
		return NULL;
	}
	btOptimizedBvh *bvh = (btOptimizedBvh *)i_alignedDataBuffer;

	if (i_swapEndian)
	{
		bvh->m_curNodeIndex = btSwapEndian(bvh->m_curNodeIndex);

		btUnSwapVector3Endian(bvh->m_bvhAabbMin);
		btUnSwapVector3Endian(bvh->m_bvhAabbMax);
		btUnSwapVector3Endian(bvh->m_bvhQuantization);

		bvh->m_traversalMode = (btTraversalMode)btSwapEndian(bvh->m_traversalMode);
		bvh->m_subtreeHeaderCount = btSwapEndian(bvh->m_subtreeHeaderCount);
	}

	int calculatedBufSize = bvh->calculateSerializeBufferSize();
	btAssert(calculatedBufSize <= i_dataBufferSize);

	if (calculatedBufSize > i_dataBufferSize)
	{
		return NULL;
	}

	unsigned char *nodeData = (unsigned char *)bvh;
	nodeData += sizeof(btOptimizedBvh);
	
	unsigned sizeToAdd = 0;//(BVH_ALIGNMENT-((unsigned)nodeData & BVH_ALIGNMENT_MASK))&BVH_ALIGNMENT_MASK;
	nodeData += sizeToAdd;
	
	int nodeCount = bvh->m_curNodeIndex;

	// Must call placement new to fill in virtual function table, etc, but we don't want to overwrite most data, so call a special version of the constructor
	// Also, m_leafNodes and m_quantizedLeafNodes will be initialized to default values by the constructor
	new (bvh) btOptimizedBvh(*bvh, false);

	if (bvh->m_useQuantization)
	{
		bvh->m_quantizedContiguousNodes.initializeFromBuffer(nodeData, nodeCount, nodeCount);

		if (i_swapEndian)
		{
			for (int nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
			{
				bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[0] = btSwapEndian(bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[0]);
				bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[1] = btSwapEndian(bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[1]);
				bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[2] = btSwapEndian(bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMin[2]);

				bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[0] = btSwapEndian(bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[0]);
				bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[1] = btSwapEndian(bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[1]);
				bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[2] = btSwapEndian(bvh->m_quantizedContiguousNodes[nodeIndex].m_quantizedAabbMax[2]);

				bvh->m_quantizedContiguousNodes[nodeIndex].m_escapeIndexOrTriangleIndex = btSwapEndian(bvh->m_quantizedContiguousNodes[nodeIndex].m_escapeIndexOrTriangleIndex);
			}
		}
		nodeData += sizeof(btQuantizedBvhNode) * nodeCount;
	}
	else
	{
		bvh->m_contiguousNodes.initializeFromBuffer(nodeData, nodeCount, nodeCount);

		if (i_swapEndian)
		{
			for (int nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
			{
				btUnSwapVector3Endian(bvh->m_contiguousNodes[nodeIndex].m_aabbMinOrg);
				btUnSwapVector3Endian(bvh->m_contiguousNodes[nodeIndex].m_aabbMaxOrg);
				
				bvh->m_contiguousNodes[nodeIndex].m_escapeIndex = btSwapEndian(bvh->m_contiguousNodes[nodeIndex].m_escapeIndex);
				bvh->m_contiguousNodes[nodeIndex].m_subPart = btSwapEndian(bvh->m_contiguousNodes[nodeIndex].m_subPart);
				bvh->m_contiguousNodes[nodeIndex].m_triangleIndex = btSwapEndian(bvh->m_contiguousNodes[nodeIndex].m_triangleIndex);
			}
		}
		nodeData += sizeof(btOptimizedBvhNode) * nodeCount;
	}

	sizeToAdd = 0;//(BVH_ALIGNMENT-((unsigned)nodeData & BVH_ALIGNMENT_MASK))&BVH_ALIGNMENT_MASK;
	nodeData += sizeToAdd;

	// Now serialize the subtree headers
	bvh->m_SubtreeHeaders.initializeFromBuffer(nodeData, bvh->m_subtreeHeaderCount, bvh->m_subtreeHeaderCount);
	if (i_swapEndian)
	{
		for (int i = 0; i < bvh->m_subtreeHeaderCount; i++)
		{
			bvh->m_SubtreeHeaders[i].m_quantizedAabbMin[0] = btSwapEndian(bvh->m_SubtreeHeaders[i].m_quantizedAabbMin[0]);
			bvh->m_SubtreeHeaders[i].m_quantizedAabbMin[1] = btSwapEndian(bvh->m_SubtreeHeaders[i].m_quantizedAabbMin[1]);
			bvh->m_SubtreeHeaders[i].m_quantizedAabbMin[2] = btSwapEndian(bvh->m_SubtreeHeaders[i].m_quantizedAabbMin[2]);

			bvh->m_SubtreeHeaders[i].m_quantizedAabbMax[0] = btSwapEndian(bvh->m_SubtreeHeaders[i].m_quantizedAabbMax[0]);
			bvh->m_SubtreeHeaders[i].m_quantizedAabbMax[1] = btSwapEndian(bvh->m_SubtreeHeaders[i].m_quantizedAabbMax[1]);
			bvh->m_SubtreeHeaders[i].m_quantizedAabbMax[2] = btSwapEndian(bvh->m_SubtreeHeaders[i].m_quantizedAabbMax[2]);

			bvh->m_SubtreeHeaders[i].m_rootNodeIndex = btSwapEndian(bvh->m_SubtreeHeaders[i].m_rootNodeIndex);
			bvh->m_SubtreeHeaders[i].m_subtreeSize = btSwapEndian(bvh->m_SubtreeHeaders[i].m_subtreeSize);
		}
	}

	return bvh;
}

// Constructor that prevents btVector3's default constructor from being called
btOptimizedBvh::btOptimizedBvh(btOptimizedBvh &self, bool ownsMemory) :
m_bvhAabbMin(self.m_bvhAabbMin),
m_bvhAabbMax(self.m_bvhAabbMax),
m_bvhQuantization(self.m_bvhQuantization)
{
	
}

