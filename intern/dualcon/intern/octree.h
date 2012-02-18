/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Tao Ju, Nicholas Bishop
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef OCTREE_H
#define OCTREE_H

#include <cassert>
#include <cstring>
#include <stdio.h>
#include <math.h>
#include "GeoCommon.h"
#include "Projections.h"
#include "ModelReader.h"
#include "MemoryAllocator.h"
#include "cubes.h"
#include "Queue.h"
#include "manifold_table.h"
#include "dualcon.h"

/**
 * Main class and structures for scan-convertion, sign-generation, 
 * and surface reconstruction.
 *
 * @author Tao Ju
 */


/* Global defines */
// Uncomment to see debug information
// #define IN_DEBUG_MODE

// Uncomment to see more output messages during repair
// #define IN_VERBOSE_MODE

/* Set scan convert params */

#define EDGE_FLOATS 4

union Node;

struct InternalNode {
	/* Treat as bitfield, bit N indicates whether child N exists or not */
	unsigned char has_child;
	/* Treat as bitfield, bit N indicates whether child N is a leaf or not */
	unsigned char child_is_leaf;

	/* Can have up to eight children */
	Node *children[0];
};


/**
 * Bits order
 *
 * Leaf node:
 * Byte 0,1(0-11): edge parity
 * Byte 1(4,5,6): mask of primary edges intersections stored
 * Byte 1(7): in flood fill mode, whether the cell is in process
 * Byte 2(0-8): signs
 * Byte 3,4: in coloring mode, the mask for edges
 * Byte 5: edge intersections(4 bytes per inter, or 12 bytes if USE_HERMIT)
 */
struct LeafNode /* TODO: remove this attribute once everything is fixed */ {
	unsigned short edge_parity : 12;
	unsigned short primary_edge_intersections : 3;

	/* XXX: maybe actually unused? */
	unsigned short in_process : 1;

	/* bitfield */
	char signs;

	int minimizer_index;
	
	unsigned short flood_fill;

	float edge_intersections[0];
};

/* Doesn't get directly allocated anywhere, just used for passing
   pointers to nodes that could be internal or leaf. */
union Node {
	InternalNode internal;
	LeafNode leaf;
};

/* Global variables */
extern const int edgemask[3];
extern const int faceMap[6][4];
extern const int cellProcFaceMask[12][3];
extern const int cellProcEdgeMask[6][5];
extern const int faceProcFaceMask[3][4][3];
extern const int edgeProcEdgeMask[3][2][5];
extern const int faceProcEdgeMask[3][4][6];
extern const int processEdgeMask[3][4];
extern const int dirCell[3][4][3]; 
extern const int dirEdge[3][4];

/**
 * Structures for detecting/patching open cycles on the dual surface
 */

struct PathElement
{
	// Origin
	int pos[3];

	// link
	PathElement* next;
};

struct PathList
{
	// Head
	PathElement* head;
	PathElement* tail;

	// Length of the list
	int length;

	// Next list
	PathList* next;
};


/**
 * Class for building and processing an octree
 */
class Octree
{
public:
	/* Public members */

	/// Memory allocators
	VirtualMemoryAllocator * alloc[9];
	VirtualMemoryAllocator * leafalloc[4];

	/// Root node
	Node* root;

	/// Model reader
	ModelReader* reader;

	/// Marching cubes table
	Cubes* cubes;

	/// Length of grid
	int dimen;
	int mindimen, minshift;

	/// Maximum depth
	int maxDepth;
	
	/// The lower corner of the bounding box and the size
	float origin[3];
	float range;

	/// Counting information
	int nodeCount;
	int nodeSpace;
	int nodeCounts[9];

	int actualQuads, actualVerts;

	PathList* ringList;

	int maxTrianglePerCell;
	int outType; // 0 for OFF, 1 for PLY, 2 for VOL

	// For flood filling
	int use_flood_fill;
	float thresh;

	int use_manifold;

	float hermite_num;

	DualConMode mode;

public:
	/**
	 * Construtor
	 */
	Octree(ModelReader* mr,
			 DualConAllocOutput alloc_output_func,
			 DualConAddVert add_vert_func,
			 DualConAddQuad add_quad_func,
			 DualConFlags flags, DualConMode mode, int depth,
			 float threshold, float hermite_num);

	/**
	 * Destructor
	 */
	~Octree();

	/**
	 * Scan convert
	 */
	void scanConvert();

	void *getOutputMesh() { return output_mesh; }

private:
	/* Helper functions */
	
	/**
	 * Initialize memory allocators
	 */
	void initMemory();

	/**
	 * Release memory
	 */
	void freeMemory();

	/**
	 * Print memory usage
	 */
	void printMemUsage();


	/**
	 * Methods to set / restore minimum edges
	 */
	void resetMinimalEdges();

	void cellProcParity(Node* node, int leaf, int depth);
	void faceProcParity(Node* node[2], int leaf[2], int depth[2], int maxdep, int dir);
	void edgeProcParity(Node* node[4], int leaf[4], int depth[4], int maxdep, int dir);

	void processEdgeParity(LeafNode* node[4], int depths[4], int maxdep, int dir);

	/**
	 * Add triangles to the tree
	 */
	void addTrian();
	void addTrian(Triangle* trian, int triind);
	InternalNode* addTrian(InternalNode* node, Projections* p, int height);

	/**
	 * Method to update minimizer in a cell: update edge intersections instead
	 */
	LeafNode* updateCell(LeafNode* node, Projections* p);

	/* Routines to detect and patch holes */
	int numRings;
	int totRingLengths;
	int maxRingLength;

	/**
	 * Entry routine.
	 */
	void trace();
	/**
	 * Trace the given node, find patches and fill them in
	 */
	Node* trace(Node* node, int* st, int len, int depth, PathList*& paths);
	/**
	 * Look for path on the face and add to paths
	 */
	void findPaths(Node* node[2], int leaf[2], int depth[2], int* st[2], int maxdep, int dir, PathList*& paths);
	/**
	 * Combine two list1 and list2 into list1 using connecting paths list3, 
	 * while closed paths are appended to rings
	 */
	void combinePaths(PathList*& list1, PathList* list2, PathList* paths, PathList*& rings);
	/**
	 * Helper function: combine current paths in list1 and list2 to a single path and append to list3
	 */
	PathList* combineSinglePath(PathList*& head1, PathList* pre1, PathList*& list1, PathList*& head2, PathList* pre2, PathList*& list2);
	
	/**
	 * Functions to patch rings in a node
	 */
	Node* patch(Node* node, int st[3], int len, PathList* rings);
	Node* patchSplit(Node* node, int st[3], int len, PathList* rings, int dir, PathList*& nrings1, PathList*& nrings2);
	Node* patchSplitSingle(Node* node, int st[3], int len, PathElement* head, int dir, PathList*& nrings1, PathList*& nrings2);
	Node* connectFace(Node* node, int st[3], int len, int dir, PathElement* f1, PathElement* f2);
	Node* locateCell(InternalNode* node, int st[3], int len, int ori[3], int dir, int side, Node*& rleaf, int rst[3], int& rlen);
	void compressRing(PathElement*& ring);
	void getFacePoint(PathElement* leaf, int dir, int& x, int& y, float& p, float& q);
	LeafNode* patchAdjacent(InternalNode* node, int len, int st1[3], LeafNode* leaf1, int st2[3], LeafNode* leaf2, int walkdir, int inc, int dir, int side, float alpha);
	int findPair(PathElement* head, int pos, int dir, PathElement*& pre1, PathElement*& pre2);
	int getSide(PathElement* e, int pos, int dir);
	int isEqual(PathElement* e1, PathElement* e2)	;
	void preparePrimalEdgesMask(InternalNode* node);
	void testFacePoint(PathElement* e1, PathElement* e2);
	
	/**
	 * Path-related functions
	 */
	void deletePath(PathList*& head, PathList* pre, PathList*& curr);
	void printPath(PathList* path);
	void printPath(PathElement* path);
	void printElement(PathElement* ele);
	void printPaths(PathList* path);
	void checkElement(PathElement* ele);
	void checkPath(PathElement* path);


	/**
	 * Routines to build signs to create a partitioned volume
	 *(after patching rings)
	 */
	void buildSigns();
	void buildSigns(unsigned char table[], Node* node, int isLeaf, int sg, int rvalue[8]);

	/************************************************************************/
	/* To remove disconnected components */
	/************************************************************************/
	void floodFill();
	void clearProcessBits(Node* node, int height);
	int floodFill(LeafNode* leaf, int st[3], int len, int height, int threshold);
	int floodFill(Node* node, int st[3], int len, int height, int threshold);

	/**
	 * Write out polygon file
	 */
	void writeOut();
	
	void countIntersection(Node* node, int height, int& nedge, int& ncell, int& nface);
	void generateMinimizer(Node* node, int st[3], int len, int height, int& offset);
	void computeMinimizer(LeafNode* leaf, int st[3], int len, float rvalue[3]);
	/**
	 * Traversal functions to generate polygon model
	 * op: 0 for counting, 1 for writing OBJ, 2 for writing OFF, 3 for writing PLY
	 */
	void cellProcContour(Node* node, int leaf, int depth);
	void faceProcContour(Node* node[2], int leaf[2], int depth[2], int maxdep, int dir);
	void edgeProcContour(Node* node[4], int leaf[4], int depth[4], int maxdep, int dir);
	void processEdgeWrite(Node* node[4], int depths[4], int maxdep, int dir);

	/* output callbacks/data */
	DualConAllocOutput alloc_output;
	DualConAddVert add_vert;
	DualConAddQuad add_quad;
	void *output_mesh;
	
private:
	/************ Operators for all nodes ************/

	/// Lookup table
	int numChildrenTable[256];
	int childrenCountTable[256][8];
	int childrenIndexTable[256][8];
	int numEdgeTable[8];
	int edgeCountTable[8][3];

	/// Build up lookup table
	void buildTable()
	{
		for(int i = 0; i < 256; i ++)
		{
			numChildrenTable[i] = 0;
			int count = 0;
			for(int j = 0; j < 8; j ++)
			{
				numChildrenTable[i] +=((i >> j) & 1);
				childrenCountTable[i][j] = count;
				childrenIndexTable[i][count] = j;
				count +=((i >> j) & 1);
			}
		}

		for(int i = 0; i < 8; i ++)
		{
			numEdgeTable[i] = 0;
			int count = 0;
			for(int j = 0; j < 3; j ++)
			{
				numEdgeTable[i] +=((i >> j) & 1);
				edgeCountTable[i][j] = count;
				count +=((i >> j) & 1);
			}
		}
	}

	int getSign(Node* node, int height, int index)
	{
		if(height == 0)
		{
			return getSign(&node->leaf, index);
		}
		else
		{
			if(hasChild(&node->internal, index))
			{
				return getSign(getChild(&node->internal, getChildCount(&node->internal, index)),
							   height - 1,
							   index);
			}
			else
			{
				return getSign(getChild(&node->internal, 0),
							   height - 1,
							   7 - getChildIndex(&node->internal, 0));
			}
		}
	}

	/************ Operators for leaf nodes ************/

	void printInfo(int st[3])
	{
		printf("INFO AT: %d %d %d\n", st[0] >> minshift, st[1] >>minshift, st[2] >> minshift);
		LeafNode* leaf = (LeafNode*)locateLeafCheck(st);
		if(leaf)
			printInfo(leaf);
		else
			printf("Leaf not exists!\n");
	}

	void printInfo(const LeafNode* leaf)
	{
		/*
		printf("Edge mask: ");
		for(int i = 0; i < 12; i ++)
		{
			printf("%d ", getEdgeParity(leaf, i));
		}
		printf("\n");
		printf("Stored edge mask: ");
		for(i = 0; i < 3; i ++)
		{
			printf("%d ", getStoredEdgesParity(leaf, i));
		}
		printf("\n");
		*/
		printf("Sign mask: ");
		for(int i = 0; i < 8; i ++)
		{
			printf("%d ", getSign(leaf, i));
		}
		printf("\n");

	}

	/// Retrieve signs
	int getSign(const LeafNode* leaf, int index)
	{
		return ((leaf->signs >> index) & 1);		
	}

	/// Set sign
	void setSign(LeafNode* leaf, int index) 
	{
		leaf->signs |= (1 << index);
	}

	void setSign(LeafNode* leaf, int index, int sign) 
	{
		leaf->signs &= (~(1 << index));
		leaf->signs |= ((sign & 1) << index);
	}

	int getSignMask(const LeafNode* leaf)
	{
		return leaf->signs;
	}

	void setInProcessAll(int st[3], int dir)
	{
		int nst[3], eind;
		for(int i = 0; i < 4; i ++)
		{
			nst[0] = st[0] + dirCell[dir][i][0] * mindimen;
			nst[1] = st[1] + dirCell[dir][i][1] * mindimen;
			nst[2] = st[2] + dirCell[dir][i][2] * mindimen;
			eind = dirEdge[dir][i];

			LeafNode* cell = locateLeafCheck(nst);
			assert(cell);

			setInProcess(cell, eind);
		}
	}

	void flipParityAll(int st[3], int dir)
	{
		int nst[3], eind;
		for(int i = 0; i < 4; i ++)
		{
			nst[0] = st[0] + dirCell[dir][i][0] * mindimen;
			nst[1] = st[1] + dirCell[dir][i][1] * mindimen;
			nst[2] = st[2] + dirCell[dir][i][2] * mindimen;
			eind = dirEdge[dir][i];

			LeafNode* cell = locateLeaf(nst);
			flipEdge(cell, eind);
		}
	}

	void setInProcess(LeafNode* leaf, int eind)
	{
		assert(eind >= 0 && eind <= 11);

		leaf->flood_fill |= (1 << eind);
	}
	
	void setOutProcess(LeafNode* leaf, int eind)
	{
		assert(eind >= 0 && eind <= 11);
		
		leaf->flood_fill &= ~(1 << eind);
	}

	int isInProcess(LeafNode* leaf, int eind)
	{
		assert(eind >= 0 && eind <= 11);
		
		return (leaf->flood_fill >> eind) & 1;
	}

	/// Generate signs at the corners from the edge parity
	void generateSigns(LeafNode* leaf, unsigned char table[], int start)
	{
		leaf->signs = table[leaf->edge_parity]; 

		if((start ^ leaf->signs) & 1)
		{
			leaf->signs = ~(leaf->signs);
		}
	}

	/// Get edge parity
	int getEdgeParity(LeafNode* leaf, int index) 
	{
		assert(index >= 0 && index <= 11);
		
		return (leaf->edge_parity >> index) & 1;
	}

	/// Get edge parity on a face
	int getFaceParity(LeafNode* leaf, int index)
	{
		int a = getEdgeParity(leaf, faceMap[index][0]) + 
				getEdgeParity(leaf, faceMap[index][1]) + 
				getEdgeParity(leaf, faceMap[index][2]) + 
				getEdgeParity(leaf, faceMap[index][3]);
		return (a & 1);
	}
	int getFaceEdgeNum(LeafNode* leaf, int index)
	{
		int a = getEdgeParity(leaf, faceMap[index][0]) + 
				getEdgeParity(leaf, faceMap[index][1]) + 
				getEdgeParity(leaf, faceMap[index][2]) + 
				getEdgeParity(leaf, faceMap[index][3]);
		return a;
	}

	/// Set edge parity
	void flipEdge(LeafNode* leaf, int index) 
	{
		assert(index >= 0 && index <= 11);

		leaf->edge_parity ^= (1 << index);
	}
	
	/// Set 1
	void setEdge(LeafNode* leaf, int index) 
	{
		assert(index >= 0 && index <= 11);

		leaf->edge_parity |= (1 << index);
	}
	
	/// Set 0
	void resetEdge(LeafNode* leaf, int index) 
	{
		assert(index >= 0 && index <= 11);

		leaf->edge_parity &= ~(1 << index);
	}

	/// Flipping with a new intersection offset
	void createPrimalEdgesMask(LeafNode* leaf)
	{
		leaf->primary_edge_intersections = getPrimalEdgesMask2(leaf);
	}

	void setStoredEdgesParity(LeafNode* leaf, int pindex)
	{
		assert(pindex <= 2 && pindex >= 0);
		
		leaf->primary_edge_intersections |= (1 << pindex);
	}
	int getStoredEdgesParity(LeafNode* leaf, int pindex)
	{
		assert(pindex <= 2 && pindex >= 0);
		
		return (leaf->primary_edge_intersections >> pindex) & 1;
	}

	LeafNode* flipEdge(LeafNode* leaf, int index, float alpha)
	{
		flipEdge(leaf, index);

		if((index & 3) == 0)
		{
			int ind = index / 4;
			if(getEdgeParity(leaf, index) && ! getStoredEdgesParity(leaf, ind))
			{
				// Create a new node
				int num = getNumEdges(leaf) + 1;
				setStoredEdgesParity(leaf, ind);
				int count = getEdgeCount(leaf, ind);
				LeafNode* nleaf = createLeaf(num);
				*nleaf = *leaf;

				setEdgeOffset(nleaf, alpha, count);

				if(num > 1)
				{
					float *pts = leaf->edge_intersections;
					float *npts = nleaf->edge_intersections;
					for(int i = 0; i < count; i ++)
					{
						for(int j = 0; j < EDGE_FLOATS; j ++)
						{
							npts[i * EDGE_FLOATS + j] = pts[i * EDGE_FLOATS + j];
						}
					}
					for(int i = count + 1; i < num; i ++)
					{
						for(int j = 0; j < EDGE_FLOATS; j ++)
						{
							npts[i * EDGE_FLOATS + j] = pts[(i - 1) * EDGE_FLOATS + j];
						}
					}
				}

				
				removeLeaf(num-1, (LeafNode*)leaf);
				leaf = nleaf;
			}
		}

		return leaf;
	}

	/// Update parent link
	void updateParent(InternalNode* node, int len, int st[3], LeafNode* leaf)
	{
		// First, locate the parent
		int count;
		InternalNode* parent = locateParent(node, len, st, count);

		// Update
		setChild(parent, count, (Node*)leaf);
	}

	void updateParent(InternalNode* node, int len, int st[3]) 
	{
		if(len == dimen)
		{
			root = (Node*)node;
			return;
		}

		// First, locate the parent
		int count;
		InternalNode* parent = locateParent(len, st, count);

		// UPdate
		setChild(parent, count, (Node*)node);
	}

	/// Find edge intersection on a given edge
	int getEdgeIntersectionByIndex(int st[3], int index, float pt[3], int check)
	{
		// First, locat the leaf
		LeafNode* leaf;
		if(check)
		{
			leaf = locateLeafCheck(st);
		}
		else
		{
			leaf = locateLeaf(st);
		}

		if(leaf && getStoredEdgesParity(leaf, index))
		{
			float off = getEdgeOffset(leaf, getEdgeCount(leaf, index));
			pt[0] =(float) st[0];
			pt[1] =(float) st[1];
			pt[2] =(float) st[2];
			pt[index] += off * mindimen;

			return 1;
		}
		else
		{
			return 0;
		}
	}

	/// Retrieve number of edges intersected
	int getPrimalEdgesMask(LeafNode* leaf)
	{
		return leaf->primary_edge_intersections;
	}

	int getPrimalEdgesMask2(LeafNode* leaf)
	{
		return (((leaf->edge_parity &   0x1) >> 0) |
				((leaf->edge_parity &  0x10) >> 3) |
				((leaf->edge_parity & 0x100) >> 6));
	}

	/// Get the count for a primary edge
	int getEdgeCount(LeafNode* leaf, int index)
	{
		return edgeCountTable[getPrimalEdgesMask(leaf)][index];
	}
	int getNumEdges(LeafNode* leaf)
	{
		return numEdgeTable[getPrimalEdgesMask(leaf)];
	}

	int getNumEdges2(LeafNode* leaf)
	{
		return numEdgeTable[getPrimalEdgesMask2(leaf)];
	}

	/// Set edge intersection
	void setEdgeOffset(LeafNode* leaf, float pt, int count)
	{
		float *pts = leaf->edge_intersections;
		pts[EDGE_FLOATS * count] = pt;
		pts[EDGE_FLOATS * count + 1] = 0;
		pts[EDGE_FLOATS * count + 2] = 0;
		pts[EDGE_FLOATS * count + 3] = 0;
	}

	/// Set multiple edge intersections
	void setEdgeOffsets(LeafNode* leaf, float pt[3], int len)
	{
		float * pts = leaf->edge_intersections;
		for(int i = 0; i < len; i ++)
		{
			pts[i] = pt[i];
		}
	}

	/// Retrieve edge intersection
	float getEdgeOffset(LeafNode* leaf, int count)
	{
		return leaf->edge_intersections[4 * count];
	}

	/// Update method
	LeafNode* updateEdgeOffsets(LeafNode* leaf, int oldlen, int newlen, float offs[3])
	{
		// First, create a new leaf node
		LeafNode* nleaf = createLeaf(newlen);
		*nleaf = *leaf;

		// Next, fill in the offsets
		setEdgeOffsets(nleaf, offs, newlen);

		// Finally, delete the old leaf
		removeLeaf(oldlen, leaf);

		return nleaf;
	}

	/// Set minimizer index
	void setMinimizerIndex(LeafNode* leaf, int index)
	{
		leaf->minimizer_index = index;
	}

	/// Get minimizer index
	int getMinimizerIndex(LeafNode* leaf)
	{
		return leaf->minimizer_index;
	}
	
	int getMinimizerIndex(LeafNode* leaf, int eind)
	{
		int add = manifold_table[getSignMask(leaf)].pairs[eind][0] - 1;
		assert(add >= 0);
		return leaf->minimizer_index + add;
	}

	void getMinimizerIndices(LeafNode* leaf, int eind, int inds[2])
	{
		const int* add = manifold_table[getSignMask(leaf)].pairs[eind];
		inds[0] = leaf->minimizer_index + add[0] - 1;
		if(add[0] == add[1])
		{
			inds[1] = -1;
		}
		else
		{
			inds[1] = leaf->minimizer_index + add[1] - 1;
		}
	}


	/// Set edge intersection
	void setEdgeOffsetNormal(LeafNode* leaf, float pt, float a, float b, float c, int count)
	{
		float * pts = leaf->edge_intersections;
		pts[4 * count] = pt;
		pts[4 * count + 1] = a;
		pts[4 * count + 2] = b;
		pts[4 * count + 3] = c;
	}

	float getEdgeOffsetNormal(LeafNode* leaf, int count, float& a, float& b, float& c)
	{
		float * pts = leaf->edge_intersections;
		a = pts[4 * count + 1];
		b = pts[4 * count + 2];
		c = pts[4 * count + 3];
		return pts[4 * count];
	}

	/// Set multiple edge intersections
	void setEdgeOffsetsNormals(LeafNode* leaf, float pt[], float a[], float b[], float c[], int len)
	{
		float *pts = leaf->edge_intersections;
		for(int i = 0; i < len; i ++)
		{
			if(pt[i] > 1 || pt[i] < 0)
			{
				printf("\noffset: %f\n", pt[i]);
			}
			pts[i * 4] = pt[i];
			pts[i * 4 + 1] = a[i];
			pts[i * 4 + 2] = b[i];
			pts[i * 4 + 3] = c[i];
		}
	}

	/// Retrieve complete edge intersection
	void getEdgeIntersectionByIndex(LeafNode* leaf, int index, int st[3], int len, float pt[3], float nm[3])
	{
		int count = getEdgeCount(leaf, index);
		float *pts = leaf->edge_intersections;
		
		float off = pts[4 * count];
		
		pt[0] = (float) st[0];
		pt[1] = (float) st[1];
		pt[2] = (float) st[2];
		pt[index] +=(off * len);

		nm[0] = pts[4 * count + 1];
		nm[1] = pts[4 * count + 2];
		nm[2] = pts[4 * count + 3];
	}

	float getEdgeOffsetNormalByIndex(LeafNode* leaf, int index, float nm[3])
	{
		int count = getEdgeCount(leaf, index);
		float *pts = leaf->edge_intersections;
		
		float off = pts[4 * count];
		
		nm[0] = pts[4 * count + 1];
		nm[1] = pts[4 * count + 2];
		nm[2] = pts[4 * count + 3];

		return off;
	}

	void fillEdgeIntersections(LeafNode* leaf, int st[3], int len, float pts[12][3], float norms[12][3])
	{
		int i;
		// int stt[3] = {0, 0, 0};

		// The three primal edges are easy
		int pmask[3] = {0, 4, 8};
		for(i = 0; i < 3; i ++)
		{
			if(getEdgeParity(leaf, pmask[i]))
			{
				// getEdgeIntersectionByIndex(leaf, i, stt, 1, pts[pmask[i]], norms[pmask[i]]);
				getEdgeIntersectionByIndex(leaf, i, st, len, pts[pmask[i]], norms[pmask[i]]);
			}
		}
		
		// 3 face adjacent cubes
		int fmask[3][2] = {{6,10},{2,9},{1,5}};
		int femask[3][2] = {{1,2},{0,2},{0,1}};
		for(i = 0; i < 3; i ++)
		{
			int e1 = getEdgeParity(leaf, fmask[i][0]);
			int e2 = getEdgeParity(leaf, fmask[i][1]);
			if(e1 || e2)
			{
				int nst[3] = {st[0], st[1], st[2]};
				nst[i] += len;
				// int nstt[3] = {0, 0, 0};
				// nstt[i] += 1;
				LeafNode* node = locateLeaf(nst);
				
				if(e1)
				{
					// getEdgeIntersectionByIndex(node, femask[i][0], nstt, 1, pts[fmask[i][0]], norms[fmask[i][0]]);
					getEdgeIntersectionByIndex(node, femask[i][0], nst, len, pts[fmask[i][0]], norms[fmask[i][0]]);
				}
				if(e2)
				{
					// getEdgeIntersectionByIndex(node, femask[i][1], nstt, 1, pts[fmask[i][1]], norms[fmask[i][1]]);
					getEdgeIntersectionByIndex(node, femask[i][1], nst, len, pts[fmask[i][1]], norms[fmask[i][1]]);
				}
			}
		}
		
		// 3 edge adjacent cubes
		int emask[3] = {3, 7, 11};
		int eemask[3] = {0, 1, 2};
		for(i = 0; i < 3; i ++)
		{
			if(getEdgeParity(leaf, emask[i]))
			{
				int nst[3] = {st[0] + len, st[1] + len, st[2] + len};
				nst[i] -= len;
				// int nstt[3] = {1, 1, 1};
				// nstt[i] -= 1;
				LeafNode* node = locateLeaf(nst);
				
				// getEdgeIntersectionByIndex(node, eemask[i], nstt, 1, pts[emask[i]], norms[emask[i]]);
				getEdgeIntersectionByIndex(node, eemask[i], nst, len, pts[emask[i]], norms[emask[i]]);
			}
		}
	}


	void fillEdgeIntersections(LeafNode* leaf, int st[3], int len, float pts[12][3], float norms[12][3], int parity[12])
	{
		int i;
		for(i = 0; i < 12; i ++)
		{
			parity[i] = 0;
		}
		// int stt[3] = {0, 0, 0};

		// The three primal edges are easy
		int pmask[3] = {0, 4, 8};
		for(i = 0; i < 3; i ++)
		{
			if(getStoredEdgesParity(leaf, i))
			{
				// getEdgeIntersectionByIndex(leaf, i, stt, 1, pts[pmask[i]], norms[pmask[i]]);
				getEdgeIntersectionByIndex(leaf, i, st, len, pts[pmask[i]], norms[pmask[i]]);
				parity[pmask[i]] = 1;
			}
		}
		
		// 3 face adjacent cubes
		int fmask[3][2] = {{6,10},{2,9},{1,5}};
		int femask[3][2] = {{1,2},{0,2},{0,1}};
		for(i = 0; i < 3; i ++)
		{
			{
				int nst[3] = {st[0], st[1], st[2]};
				nst[i] += len;
				// int nstt[3] = {0, 0, 0};
				// nstt[i] += 1;
				LeafNode* node = locateLeafCheck(nst);
				if(node == NULL)
				{
					continue;
				}
		
				int e1 = getStoredEdgesParity(node, femask[i][0]);
				int e2 = getStoredEdgesParity(node, femask[i][1]);
				
				if(e1)
				{
					// getEdgeIntersectionByIndex(node, femask[i][0], nstt, 1, pts[fmask[i][0]], norms[fmask[i][0]]);
					getEdgeIntersectionByIndex(node, femask[i][0], nst, len, pts[fmask[i][0]], norms[fmask[i][0]]);
					parity[fmask[i][0]] = 1;
				}
				if(e2)
				{
					// getEdgeIntersectionByIndex(node, femask[i][1], nstt, 1, pts[fmask[i][1]], norms[fmask[i][1]]);
					getEdgeIntersectionByIndex(node, femask[i][1], nst, len, pts[fmask[i][1]], norms[fmask[i][1]]);
					parity[fmask[i][1]] = 1;
				}
			}
		}
		
		// 3 edge adjacent cubes
		int emask[3] = {3, 7, 11};
		int eemask[3] = {0, 1, 2};
		for(i = 0; i < 3; i ++)
		{
//			if(getEdgeParity(leaf, emask[i]))
			{
				int nst[3] = {st[0] + len, st[1] + len, st[2] + len};
				nst[i] -= len;
				// int nstt[3] = {1, 1, 1};
				// nstt[i] -= 1;
				LeafNode* node = locateLeafCheck(nst);
				if(node == NULL)
				{
					continue;
				}
				
				if(getStoredEdgesParity(node, eemask[i]))
				{
					// getEdgeIntersectionByIndex(node, eemask[i], nstt, 1, pts[emask[i]], norms[emask[i]]);
					getEdgeIntersectionByIndex(node, eemask[i], nst, len, pts[emask[i]], norms[emask[i]]);
					parity[emask[i]] = 1;
				}
			}
		}
	}

	void fillEdgeOffsetsNormals(LeafNode* leaf, int st[3], int len, float pts[12], float norms[12][3], int parity[12])
	{
		int i;
		for(i = 0; i < 12; i ++)
		{
			parity[i] = 0;
		}
		// int stt[3] = {0, 0, 0};

		// The three primal edges are easy
		int pmask[3] = {0, 4, 8};
		for(i = 0; i < 3; i ++)
		{
			if(getStoredEdgesParity(leaf, i))
			{
				pts[pmask[i]] = getEdgeOffsetNormalByIndex(leaf, i, norms[pmask[i]]);
				parity[pmask[i]] = 1;
			}
		}
		
		// 3 face adjacent cubes
		int fmask[3][2] = {{6,10},{2,9},{1,5}};
		int femask[3][2] = {{1,2},{0,2},{0,1}};
		for(i = 0; i < 3; i ++)
		{
			{
				int nst[3] = {st[0], st[1], st[2]};
				nst[i] += len;
				// int nstt[3] = {0, 0, 0};
				// nstt[i] += 1;
				LeafNode* node = locateLeafCheck(nst);
				if(node == NULL)
				{
					continue;
				}
		
				int e1 = getStoredEdgesParity(node, femask[i][0]);
				int e2 = getStoredEdgesParity(node, femask[i][1]);
				
				if(e1)
				{
					pts[fmask[i][0]] = getEdgeOffsetNormalByIndex(node, femask[i][0], norms[fmask[i][0]]);
					parity[fmask[i][0]] = 1;
				}
				if(e2)
				{
					pts[fmask[i][1]] = getEdgeOffsetNormalByIndex(node, femask[i][1], norms[fmask[i][1]]);
					parity[fmask[i][1]] = 1;
				}
			}
		}
		
		// 3 edge adjacent cubes
		int emask[3] = {3, 7, 11};
		int eemask[3] = {0, 1, 2};
		for(i = 0; i < 3; i ++)
		{
//			if(getEdgeParity(leaf, emask[i]))
			{
				int nst[3] = {st[0] + len, st[1] + len, st[2] + len};
				nst[i] -= len;
				// int nstt[3] = {1, 1, 1};
				// nstt[i] -= 1;
				LeafNode* node = locateLeafCheck(nst);
				if(node == NULL)
				{
					continue;
				}
				
				if(getStoredEdgesParity(node, eemask[i]))
				{
					pts[emask[i]] = getEdgeOffsetNormalByIndex(node, eemask[i], norms[emask[i]]);
					parity[emask[i]] = 1;
				}
			}
		}
	}


	/// Update method
	LeafNode* updateEdgeOffsetsNormals(LeafNode* leaf, int oldlen, int newlen, float offs[3], float a[3], float b[3], float c[3])
	{
		// First, create a new leaf node
		LeafNode* nleaf = createLeaf(newlen);
		*nleaf = *leaf;

		// Next, fill in the offsets
		setEdgeOffsetsNormals(nleaf, offs, a, b, c, newlen);

		// Finally, delete the old leaf
		removeLeaf(oldlen, leaf);

		return nleaf;
	}

	/// Locate a leaf
	/// WARNING: assuming this leaf already exists!
	
	LeafNode* locateLeaf(int st[3])
	{
		Node* node = (Node*)root;
		for(int i = GRID_DIMENSION - 1; i > GRID_DIMENSION - maxDepth - 1; i --)
		{
			int index =(((st[0] >> i) & 1) << 2) | 
						(((st[1] >> i) & 1) << 1) | 
						(((st[2] >> i) & 1));
			node = getChild(&node->internal, getChildCount(&node->internal, index));
		}

		return &node->leaf;
	}
	
	LeafNode* locateLeaf(InternalNode* parent, int len, int st[3])
	{
		Node *node = (Node*)parent;
		int index;
		for(int i = len / 2; i >= mindimen; i >>= 1)
		{
			index =(((st[0] & i) ? 4 : 0) | 
					((st[1] & i) ? 2 : 0) | 
					((st[2] & i) ? 1 : 0));
			node = getChild(&node->internal,
							getChildCount(&node->internal, index));
		}

		return &node->leaf;
	}

	LeafNode* locateLeafCheck(int st[3])
	{
		Node* node = (Node*)root;
		for(int i = GRID_DIMENSION - 1; i > GRID_DIMENSION - maxDepth - 1; i --)
		{
			int index =(((st[0] >> i) & 1) << 2) | 
						(((st[1] >> i) & 1) << 1) | 
						(((st[2] >> i) & 1));
			if(!hasChild(&node->internal, index))
			{
				return NULL;
			}
			node = getChild(&node->internal, getChildCount(&node->internal, index));
		}

		return &node->leaf;
	}

	InternalNode* locateParent(int len, int st[3], int& count)
	{
		InternalNode* node = (InternalNode*)root;
		InternalNode* pre = NULL;
		int index = 0;
		for(int i = dimen / 2; i >= len; i >>= 1)
		{
			index =(((st[0] & i) ? 4 : 0) | 
					((st[1] & i) ? 2 : 0) | 
					((st[2] & i) ? 1 : 0));
			pre = node;
			node = &getChild(node, getChildCount(node, index))->internal;
		}

		count = getChildCount(pre, index);
		return pre;
	}
	
	InternalNode* locateParent(InternalNode* parent, int len, int st[3], int& count)
	{
		InternalNode* node = parent;
		InternalNode* pre = NULL;
		int index = 0;
		for(int i = len / 2; i >= mindimen; i >>= 1)
		{
			index =(((st[0] & i) ? 4 : 0) | 
					((st[1] & i) ? 2 : 0) | 
					((st[2] & i) ? 1 : 0));
			pre = node;
			node = (InternalNode*)getChild(node, getChildCount(node, index));
		}

		count = getChildCount(pre, index);
		return pre;
	}
	
	/************ Operators for internal nodes ************/

	/// If child index exists
	int hasChild(InternalNode* node, int index)
	{
		return (node->has_child >> index) & 1;
	}

	/// Test if child is leaf
	int isLeaf(InternalNode* node, int index)
	{
		return (node->child_is_leaf >> index) & 1;
	}

	/// Get the pointer to child index
	Node* getChild(InternalNode* node, int count)
	{
		return node->children[count];
	};

	/// Get total number of children
	int getNumChildren(InternalNode* node)
	{
		return numChildrenTable[node->has_child];
	}

	/// Get the count of children
	int getChildCount(InternalNode* node, int index)
	{
		return childrenCountTable[node->has_child][index];
	}
	int getChildIndex(InternalNode* node, int count)
	{
		return childrenIndexTable[node->has_child][count];
	}
	int* getChildCounts(InternalNode* node)
	{
		return childrenCountTable[node->has_child];
	}

	/// Get all children
	void fillChildren(InternalNode* node, Node* children[8], int leaf[8])
	{
		int count = 0;
		for(int i = 0; i < 8; i ++)
		{	
			leaf[i] = isLeaf(node, i);
			if(hasChild(node, i))
			{
				children[i] = getChild(node, count);
				count ++;
			}
			else
			{
			 	children[i] = NULL;
				leaf[i] = 0;
			}
		}
	}

	/// Sets the child pointer
	void setChild(InternalNode* node, int count, Node* chd)
	{
		node->children[count] = chd;
	}
	void setInternalChild(InternalNode* node, int index, int count, InternalNode* chd)
	{
		setChild(node, count, (Node*)chd);
		node->has_child |= (1 << index);
	}
	void setLeafChild(InternalNode* node, int index, int count, LeafNode* chd)
	{
		setChild(node, count, (Node*)chd);
		node->has_child |=(1 << index);
		node->child_is_leaf |= (1 << index);
	}

	/// Add a kid to an existing internal node
	/// Fix me: can we do this without wasting memory ?
	/// Fixed: using variable memory
	InternalNode* addChild(InternalNode* node, int index, Node* child, int aLeaf)
	{
		// Create new internal node
		int num = getNumChildren(node);
		InternalNode* rnode = createInternal(num + 1);

		// Establish children
		int i;
		int count1 = 0, count2 = 0;
		for(i = 0; i < 8; i ++)
		{
			if(i == index)
			{
				if(aLeaf)
				{
					setLeafChild(rnode, i, count2, &child->leaf);
				}
				else
				{
					setInternalChild(rnode, i, count2, &child->internal);
				}
				count2 ++;
			}
			else if(hasChild(node, i))
			{
				if(isLeaf(node, i))
				{
					setLeafChild(rnode, i, count2, &getChild(node, count1)->leaf);
				}
				else
				{
					setInternalChild(rnode, i, count2, &getChild(node, count1)->internal);
				}
				count1 ++;
				count2 ++;
			}
		}

		removeInternal(num, node);
		return rnode;
	}

	/// Allocate a node
	InternalNode* createInternal(int length)
	{
		InternalNode* inode = (InternalNode*)alloc[length]->allocate();
		inode->has_child = 0;
		inode->child_is_leaf = 0;
		return inode;
	}
	
	LeafNode* createLeaf(int length)
	{
		assert(length <= 3);

		LeafNode* lnode = (LeafNode*)leafalloc[length]->allocate();
		lnode->edge_parity = 0;
		lnode->primary_edge_intersections = 0;
		lnode->signs = 0;

		return lnode;
	}

	void removeInternal(int num, InternalNode* node)
	{
		alloc[num]->deallocate(node);
	}

	void removeLeaf(int num, LeafNode* leaf)
	{
		assert(num >= 0 && num <= 3);
		leafalloc[num]->deallocate(leaf);
	}

	/// Add a leaf (by creating a new par node with the leaf added)
	InternalNode* addLeafChild(InternalNode* par, int index, int count,
							   LeafNode* leaf)
	{
		int num = getNumChildren(par) + 1;
		InternalNode* npar = createInternal(num);
		*npar = *par;
		
		if(num == 1)
		{
			setLeafChild(npar, index, 0, leaf);
		}
		else
		{
			int i;
			for(i = 0; i < count; i ++)
			{
				setChild(npar, i, getChild(par, i));
			}
			setLeafChild(npar, index, count, leaf);
			for(i = count + 1; i < num; i ++)
			{
				setChild(npar, i, getChild(par, i - 1));
			}
		}
		
		removeInternal(num-1, par);
		return npar;
	}

	InternalNode* addInternalChild(InternalNode* par, int index, int count,
								   InternalNode* node)
	{
		int num = getNumChildren(par) + 1;
		InternalNode* npar = createInternal(num);
		*npar = *par;
		
		if(num == 1)
		{
			setInternalChild(npar, index, 0, node);
		}
		else
		{
			int i;
			for(i = 0; i < count; i ++)
			{
				setChild(npar, i, getChild(par, i));
			}
			setInternalChild(npar, index, count, node);
			for(i = count + 1; i < num; i ++)
			{
				setChild(npar, i, getChild(par, i - 1));
			}
		}
		
		removeInternal(num-1, par);
		return npar;
	}
};

#endif
