/** \file elbeem/intern/ntl_bsptree.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Tree container for fast triangle intersects
 *
 *****************************************************************************/
#ifndef NTL_TREE_H
#define NTL_TREE_H

#include "ntl_vector3dim.h"
#include "ntl_ray.h"


#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2

#define BSP_STACK_SIZE 50

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

//! bsp tree stack classes, defined in ntl_bsptree.cpp,
//  detailed definition unnecesseary here
class BSPNode;
class BSPStackElement;
class BSPStack;
class TriangleBBox;
class ntlScene;
class ntlTriangle;


//! Class for a bsp tree for triangles
class ntlTree
{
	public:

		//! Default constructor
		ntlTree();
		//! Constructor with init
		ntlTree(int depth, int objnum, ntlScene *scene, int triFlagMask);
		//! Destructor
		~ntlTree();

		//! subdivide tree
		void subdivide(BSPNode *node, int depth, int axis);

		//! intersect ray with BSPtree
		void intersect(const ntlRay &ray, gfxReal &distance, ntlVec3Gfx &normal, ntlTriangle *&tri, int flags, bool forceNonsmooth) const;
		//! intersect along +X ray
		void intersectX(const ntlRay &ray, gfxReal &distance, ntlVec3Gfx &normal, ntlTriangle *&tri, int flags, bool forceNonsmooth) const;

		//! Returns number of nodes
		int getCurrentNodes( void ) { return mCurrentNodes; }

	protected:

		// check if a triangle is in a node
		bool checkAABBTriangle(ntlVec3Gfx &min, ntlVec3Gfx &max, ntlTriangle *tri);


		// VARs

		//! distance to plane function for nodes
		gfxReal distanceToPlane(BSPNode *curr, ntlVec3Gfx plane, ntlRay ray) const;

		//! return ordering of children nodes relatice to origin point
		void getChildren(BSPNode *curr, ntlVec3Gfx origin, BSPNode *&node_near, BSPNode *&node_far) const;

		//! delete a node of the tree with all sub nodes, dont delete root members
		void deleteNode(BSPNode *curr);

		//inline bool isLeaf(BSPNode *node) const { return (node->child[0] == NULL); }


		//! AABB for tree
		ntlVec3Gfx mStart,mEnd;

		//! maximum depth of tree
		int mMaxDepth;

		//! maximum number of objects in one node
		int mMaxListLength;

		//! root node pointer
		BSPNode *mpRoot;
		//! count no. of node
		int mNumNodes;
		int mAbortSubdiv;

		//! stack for the node pointers
		BSPStack *mpNodeStack;
		//stack<BSPNode *> nodestack;

		//! pointer to vertex array
		vector<ntlVec3Gfx> *mpVertices;

		//! pointer to vertex array
		vector<ntlVec3Gfx> *mpVertNormals;

		//! vector for all the triangles
		vector<ntlTriangle> *mpTriangles;
		vector<ntlTriangle *> *mppTriangles;

		//! temporary array for triangle distribution to nodes
		char *mpTriDist;

		//! temporary array for triangle bounding boxes
		TriangleBBox *mpTBB;

		//! triangle mask - include only triangles that match mask
		int mTriangleMask;

		//! Status vars (max depth, # of current nodes)
		int mCurrentDepth, mCurrentNodes;

		//! duplicated triangles, inited during subdivide 
		int mTriDoubles; 

private:
#ifdef WITH_CXX_GUARDEDALLOC
       MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:ntlTree")
#endif
};


#endif


