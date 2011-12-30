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
 * Contributor(s): Tao Ju
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef OCTREE_H
#define OCTREE_H

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
// Uncomment to use Dual Contouring on Hermit representation
// for better sharp feature reproduction, but more mem is required
// The number indicates how far do we allow the minimizer to shoot
// outside the cell
#define USE_HERMIT 1.0f

#ifdef USE_HERMIT
//#define CINDY
#endif

///#define QIANYI

//#define TESTMANIFOLD


/* Set output options */
// Comment out to prevent writing output files
#define OUTPUT_REPAIRED


/* Set node bytes */
#ifdef USE_HERMIT
#define EDGE_BYTES 16
#define EDGE_FLOATS 4
#else
#define EDGE_BYTES 4
#define EDGE_FLOATS 1
#endif

#define CINDY_BYTES 0

/*#define LEAF_EXTRA_BYTES FLOOD_BYTES + CINDY_BYTES

#ifdef USE_HERMIT
#define LEAF_NODE_BYTES 7 + LEAF_EXTRA_BYTES
#else
#define LEAF_NODE_BYTES 3 + LEAF_EXTRA_BYTES
#endif*/

#define INTERNAL_NODE_BYTES 2
#define POINTER_BYTES 8
#define FLOOD_FILL_BYTES 2

#define signtype short
#define nodetype int
#define numtype int

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
	int pos[3] ;

	// link
	PathElement* next ;
};

struct PathList
{
	// Head
	PathElement* head ;
	PathElement* tail ;

	// Length of the list
	int length ;

	// Next list
	PathList* next ;
};


/**
 * Class for building and processing an octree
 */
class Octree
{
public:
	/* Public members */

	/// Memory allocators
	VirtualMemoryAllocator * alloc[ 9 ] ;
	VirtualMemoryAllocator * leafalloc[ 4 ] ;

	/// Root node
	UCHAR* root ;

	/// Model reader
	ModelReader* reader ;

	/// Marching cubes table
	Cubes* cubes ;

	/// Length of grid
	int dimen ;
	int mindimen, minshift ;

	/// Maximum depth
	int maxDepth ;
	
	/// The lower corner of the bounding box and the size
	float origin[3];
	float range;

	/// Counting information
	int nodeCount ;
	int nodeSpace ;
	int nodeCounts[ 9 ] ;

	int actualQuads, actualVerts ;

	PathList* ringList ;

	int maxTrianglePerCell ;
	int outType ; // 0 for OFF, 1 for PLY, 2 for VOL

	// For flood filling
	int use_flood_fill;
	float thresh ;

	int use_manifold;

	// testing
	FILE* testfout ;

	float hermite_num;

	DualConMode mode;

	int leaf_node_bytes;
	int leaf_extra_bytes;
	int flood_bytes;

public:
	/**
	 * Construtor
	 */
	Octree ( ModelReader* mr,
			 DualConAllocOutput alloc_output_func,
			 DualConAddVert add_vert_func,
			 DualConAddQuad add_quad_func,
			 DualConFlags flags, DualConMode mode, int depth,
			 float threshold, float hermite_num ) ;

	/**
	 * Destructor
	 */
	~Octree ( ) ;

	/**
	 * Scan convert
	 */
	void scanConvert() ;

	void *getOutputMesh() { return output_mesh; }

private:
	/* Helper functions */
	
	/**
	 * Initialize memory allocators
	 */
	void initMemory ( ) ;

	/**
	 * Release memory
	 */
	void freeMemory ( ) ;

	/**
	 * Print memory usage
	 */
	void printMemUsage( ) ;


	/**
	 * Methods to set / restore minimum edges
	 */
	void resetMinimalEdges( ) ;

	void cellProcParity ( UCHAR* node, int leaf, int depth ) ;
	void faceProcParity ( UCHAR* node[2], int leaf[2], int depth[2], int maxdep, int dir ) ;
	void edgeProcParity ( UCHAR* node[4], int leaf[4], int depth[4], int maxdep, int dir ) ;

	void processEdgeParity ( UCHAR* node[4], int depths[4], int maxdep, int dir ) ;

	/**
	 * Add triangles to the tree
	 */
	void addTrian ( );
	void addTrian ( Triangle* trian, int triind );
	UCHAR* addTrian ( UCHAR* node, Projections* p, int height );

	/**
	 * Method to update minimizer in a cell: update edge intersections instead
	 */
	UCHAR* updateCell( UCHAR* node, Projections* p ) ;

	/* Routines to detect and patch holes */
	int numRings ;
	int totRingLengths ;
	int maxRingLength ;

	/**
	 * Entry routine.
	 */
	void trace ( ) ;
	/**
	 * Trace the given node, find patches and fill them in
	 */
	UCHAR* trace ( UCHAR* node, int* st, int len, int depth, PathList*& paths ) ;
	/**
	 * Look for path on the face and add to paths
	 */
	void findPaths ( UCHAR* node[2], int leaf[2], int depth[2], int* st[2], int maxdep, int dir, PathList*& paths ) ;
	/**
	 * Combine two list1 and list2 into list1 using connecting paths list3, 
	 * while closed paths are appended to rings
	 */
	void combinePaths ( PathList*& list1, PathList* list2, PathList* paths, PathList*& rings ) ;
	/**
	 * Helper function: combine current paths in list1 and list2 to a single path and append to list3
	 */
	PathList* combineSinglePath ( PathList*& head1, PathList* pre1, PathList*& list1, PathList*& head2, PathList* pre2, PathList*& list2 ) ;
	
	/**
	 * Functions to patch rings in a node
	 */
	UCHAR* patch ( UCHAR* node, int st[3], int len, PathList* rings ) ;
	UCHAR* patchSplit ( UCHAR* node, int st[3], int len, PathList* rings, int dir, PathList*& nrings1, PathList*& nrings2 ) ;
	UCHAR* patchSplitSingle ( UCHAR* node, int st[3], int len, PathElement* head, int dir, PathList*& nrings1, PathList*& nrings2 ) ;
	UCHAR* connectFace ( UCHAR* node, int st[3], int len, int dir, PathElement* f1, PathElement* f2 ) ;
	UCHAR* locateCell( UCHAR* node, int st[3], int len, int ori[3], int dir, int side, UCHAR*& rleaf, int rst[3], int& rlen ) ;
	void compressRing ( PathElement*& ring ) ;
	void getFacePoint( PathElement* leaf, int dir, int& x, int& y, float& p, float& q ) ;
	UCHAR* patchAdjacent( UCHAR* node, int len, int st1[3], UCHAR* leaf1, int st2[3], UCHAR* leaf2, int walkdir, int inc, int dir, int side, float alpha ) ;
	int findPair ( PathElement* head, int pos, int dir, PathElement*& pre1, PathElement*& pre2 ) ;
	int getSide( PathElement* e, int pos, int dir ) ;
	int isEqual( PathElement* e1, PathElement* e2 )	;
	void preparePrimalEdgesMask( UCHAR* node ) ;
	void testFacePoint( PathElement* e1, PathElement* e2 ) ;
	
	/**
	 * Path-related functions
	 */
	void deletePath ( PathList*& head, PathList* pre, PathList*& curr ) ;
	void printPath( PathList* path ) ;
	void printPath( PathElement* path ) ;
	void printElement( PathElement* ele ) ;
	void printPaths( PathList* path ) ;
	void checkElement ( PathElement* ele ) ;
	void checkPath( PathElement* path ) ;


	/**
	 * Routines to build signs to create a partitioned volume
	 * (after patching rings)
	 */
	void buildSigns( ) ;
	void buildSigns( unsigned char table[], UCHAR* node, int isLeaf, int sg, int rvalue[8] ) ;

	/************************************************************************/
	/* To remove disconnected components */
	/************************************************************************/
	void floodFill( ) ;
	void clearProcessBits( UCHAR* node, int height ) ;
	int floodFill( UCHAR* node, int st[3], int len, int height, int threshold ) ;

	/**
	 * Write out polygon file
	 */
	void writeOut();
	void writeOFF ( char* fname ) ;
	void writePLY ( char* fname ) ;
	void writeOpenEdges( FILE* fout ) ;
	void writeAllEdges( FILE* fout, int mode ) ;
	void writeAllEdges( FILE* fout, UCHAR* node, int height, int st[3], int len, int mode ) ;
	
	void writeOctree( char* fname ) ;
	void writeOctree( FILE* fout, UCHAR* node, int depth ) ;
#ifdef USE_HERMIT
	void writeOctreeGeom( char* fname ) ;
	void writeOctreeGeom( FILE* fout, UCHAR* node, int st[3], int len, int depth ) ;
#endif
#ifdef USE_HERMIT
	void writeDCF ( char* fname ) ;
	void writeDCF ( FILE* fout, UCHAR* node, int height, int st[3], int len ) ;
	void countEdges ( UCHAR* node, int height, int& nedge, int mode ) ;
	void countIntersection( UCHAR* node, int height, int& nedge, int& ncell, int& nface ) ;
	void generateMinimizer( UCHAR* node, int st[3], int len, int height, int& offset ) ;
	void computeMinimizer( UCHAR* leaf, int st[3], int len, float rvalue[3] ) ;
	/**
	 * Traversal functions to generate polygon model
	 * op: 0 for counting, 1 for writing OBJ, 2 for writing OFF, 3 for writing PLY
	 */
	void cellProcContour ( UCHAR* node, int leaf, int depth ) ;
	void faceProcContour ( UCHAR* node[2], int leaf[2], int depth[2], int maxdep, int dir ) ;
	void edgeProcContour ( UCHAR* node[4], int leaf[4], int depth[4], int maxdep, int dir ) ;
	void processEdgeWrite ( UCHAR* node[4], int depths[4], int maxdep, int dir ) ;
#else
	void countIntersection( UCHAR* node, int height, int& nquad, int& nvert ) ;
	void writeVertex( UCHAR* node, int st[3], int len, int height, int& offset, FILE* fout ) ;
	void writeQuad( UCHAR* node, int st[3], int len, int height, FILE* fout ) ;
#endif

	/**
	 * Write out original model
	 */
	void writeModel( char* fname ) ;

	/************************************************************************/
	/* Write out original vertex tags */
	/************************************************************************/
#ifdef CINDY
	void writeTags( char* fname ) ;
	void readVertices( ) ;
	void readVertex(  UCHAR* node, int st[3], int len, int height, float v[3], int index ) ;
    void outputTags( UCHAR* node, int height, FILE* fout ) ;
	void clearCindyBits( UCHAR* node, int height ) ;
#endif

	/* output callbacks/data */
	DualConAllocOutput alloc_output;
	DualConAddVert add_vert;
	DualConAddQuad add_quad;
	void *output_mesh;
	
private:
	/************ Operators for all nodes ************/

	/**
	 * Bits order
	 *
	 * Leaf node:
	 * Byte 0,1 (0-11): edge parity
	 * Byte 1 (4,5,6): mask of primary edges intersections stored
	 * Byte 1 (7): in flood fill mode, whether the cell is in process
	 * Byte 2 (0-8): signs
	 * Byte 3 (or 5) -- : edge intersections ( 4 bytes per inter, or 12 bytes if USE_HERMIT )
	 * Byte 3,4: in coloring mode, the mask for edges
	 *
	 * Internal node:
	 * Byte 0: child mask
	 * Byte 1: leaf child mask
	 */

	/// Lookup table
	int numChildrenTable[ 256 ] ;
	int childrenCountTable[ 256 ][ 8 ] ;
	int childrenIndexTable[ 256 ][ 8 ] ;
	int numEdgeTable[ 8 ] ;
	int edgeCountTable[ 8 ][ 3 ] ;

	/// Build up lookup table
	void buildTable ( )
	{
		for ( int i = 0 ; i < 256 ; i ++ )
		{
			numChildrenTable[ i ] = 0 ;
			int count = 0 ;
			for ( int j = 0 ; j < 8 ; j ++ )
			{
				numChildrenTable[ i ] += ( ( i >> j ) & 1 ) ;
				childrenCountTable[ i ][ j ] = count ;
				childrenIndexTable[ i ][ count ] = j ;
				count += ( ( i >> j ) & 1 ) ;
			}
		}

		for ( int i = 0 ; i < 8 ; i ++ )
		{
			numEdgeTable[ i ] = 0 ;
			int count = 0 ;
			for ( int j = 0 ; j < 3 ; j ++ )
			{
				numEdgeTable[ i ] += ( ( i >> j ) & 1 ) ;
				edgeCountTable[ i ][ j ] = count ;
				count += ( ( i >> j ) & 1 ) ;
			}
		}
	};

	int getSign( UCHAR* node, int height, int index )
	{
		if ( height == 0 )
		{
			return getSign( node, index ) ;
		}
		else
		{
			if ( hasChild( node, index ) )
			{
				return getSign( getChild( node, getChildCount( node, index ) ), height - 1, index ) ;
			}
			else
			{
				return getSign( getChild( node, 0 ), height - 1, 7 - getChildIndex( node, 0 ) ) ;
			}
		}
	}

	/************ Operators for leaf nodes ************/

	void printInfo( int st[3] )
	{
		printf("INFO AT: %d %d %d\n", st[0] >> minshift, st[1] >>minshift, st[2] >> minshift ) ;
		UCHAR* leaf = locateLeafCheck( st ) ;
		if ( leaf == NULL )
		{
			printf("Leaf not exists!\n") ;
		}
		else
		{
			printInfo( leaf ) ;
		}
	}

	void printInfo( UCHAR* leaf )
	{
		/*
		printf("Edge mask: ") ;
		for ( int i = 0 ; i < 12 ; i ++ )
		{
			printf("%d ", getEdgeParity( leaf, i ) ) ;
		}
		printf("\n") ;
		printf("Stored edge mask: ") ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			printf("%d ", getStoredEdgesParity( leaf, i ) ) ;
		}
		printf("\n") ;
		*/
		printf("Sign mask: ") ;
		for ( int i = 0 ; i < 8 ; i ++ )
		{
			printf("%d ", getSign( leaf, i ) ) ;
		}
		printf("\n") ;

	}

	/// Retrieve signs
	int getSign ( UCHAR* leaf, int index )
	{
		return (( leaf[2] >> index ) & 1 );		
	};

	/// Set sign
	void setSign ( UCHAR* leaf, int index ) 
	{
		leaf[2] |= ( 1 << index ) ;
	};

	void setSign ( UCHAR* leaf, int index, int sign ) 
	{
		leaf[2] &= ( ~ ( 1 << index ) ) ;
		leaf[2] |= ( ( sign & 1 ) << index ) ;
	};

	int getSignMask( UCHAR* leaf )
	{
		return leaf[2] ;
	}

	void setInProcessAll( int st[3], int dir )
	{
		int nst[3], eind ;
		for ( int i = 0 ; i < 4 ; i ++ )
		{
			nst[0] = st[0] + dirCell[dir][i][0] * mindimen ;
			nst[1] = st[1] + dirCell[dir][i][1] * mindimen ;
			nst[2] = st[2] + dirCell[dir][i][2] * mindimen ;
			eind = dirEdge[dir][i] ;

			UCHAR* cell = locateLeafCheck( nst ) ;
			if ( cell == NULL )
			{
				printf("Wrong!\n") ;
			}
			setInProcess( cell, eind ) ;
		}
	}

	void flipParityAll( int st[3], int dir )
	{
		int nst[3], eind ;
		for ( int i = 0 ; i < 4 ; i ++ )
		{
			nst[0] = st[0] + dirCell[dir][i][0] * mindimen ;
			nst[1] = st[1] + dirCell[dir][i][1] * mindimen ;
			nst[2] = st[2] + dirCell[dir][i][2] * mindimen ;
			eind = dirEdge[dir][i] ;

			UCHAR* cell = locateLeaf( nst ) ;
			flipEdge( cell, eind ) ;
		}
	}

	void setInProcess( UCHAR* leaf, int eind )
	{
		// leaf[1] |= ( 1 << 7 ) ;
		( (USHORT*) (leaf + leaf_node_bytes - (flood_bytes + CINDY_BYTES)))[0] |= ( 1 << eind ) ;
	}
	void setOutProcess( UCHAR* leaf, int eind )
	{
		// leaf[1] &= ( ~ ( 1 << 7 ) ) ;
		( (USHORT*) (leaf + leaf_node_bytes - (flood_bytes + CINDY_BYTES)))[0] &= ( ~ ( 1 << eind ) ) ;
	}

	int isInProcess( UCHAR* leaf, int eind )
	{
		//int a = ( ( leaf[1] >> 7 ) & 1 ) ;
		int a = ( ( ( (USHORT*) (leaf + leaf_node_bytes - (flood_bytes + CINDY_BYTES)))[0] >> eind ) & 1 ) ;
		return a ;
	}

#ifndef USE_HERMIT
	/// Set minimizer index
	void setEdgeIntersectionIndex( UCHAR* leaf, int count, int index )
	{
		((int *)( leaf + leaf_node_bytes ))[ count ] = index ;
	}

	/// Get minimizer index
	int getEdgeIntersectionIndex( UCHAR* leaf, int count )
	{
		return 	((int *)( leaf + leaf_node_bytes ))[ count ] ;
	}

	/// Get all intersection indices associated with a cell
	void fillEdgeIntersectionIndices( UCHAR* leaf, int st[3], int len, int inds[12] )
	{
		int i ;

		// The three primal edges are easy
		int pmask[3] = { 0, 4, 8 } ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			if ( getEdgeParity( leaf, pmask[i] ) )
			{
				inds[pmask[i]] = getEdgeIntersectionIndex( leaf, getEdgeCount( leaf, i ) ) ;
			}
		}
		
		// 3 face adjacent cubes
		int fmask[3][2] = {{6,10},{2,9},{1,5}} ;
		int femask[3][2] = {{1,2},{0,2},{0,1}} ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			int e1 = getEdgeParity( leaf, fmask[i][0] ) ;
			int e2 = getEdgeParity( leaf, fmask[i][1] ) ;
			if ( e1 || e2 )
			{
				int nst[3] = {st[0], st[1], st[2]} ;
				nst[ i ] += len ;
				// int nstt[3] = {0, 0, 0} ;
				// nstt[ i ] += 1 ;
				UCHAR* node = locateLeaf( nst ) ;
				
				if ( e1 )
				{
					inds[ fmask[i][0] ] = getEdgeIntersectionIndex( node, getEdgeCount( node, femask[i][0] ) ) ;
				}
				if ( e2 )
				{
					inds[ fmask[i][1] ] = getEdgeIntersectionIndex( node, getEdgeCount( node, femask[i][1] ) ) ;
				}
			}
		}
		
		// 3 edge adjacent cubes
		int emask[3] = {3, 7, 11} ;
		int eemask[3] = {0, 1, 2} ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			if ( getEdgeParity( leaf, emask[i] ) )
			{
				int nst[3] = {st[0] + len, st[1] + len, st[2] + len} ;
				nst[ i ] -= len ;
				// int nstt[3] = {1, 1, 1} ;
				// nstt[ i ] -= 1 ;
				UCHAR* node = locateLeaf( nst ) ;
				
				inds[ emask[i] ] = getEdgeIntersectionIndex( node, getEdgeCount( node, eemask[i] ) ) ;
			}
		}
	}


#endif
	
	/// Generate signs at the corners from the edge parity
	void generateSigns ( UCHAR* leaf, UCHAR table[], int start )
	{
		leaf[2] = table[ ( ((USHORT *) leaf)[ 0 ] ) & ( ( 1 << 12 ) - 1 ) ] ; 

		if ( ( start ^ leaf[2] ) & 1 ) 
		{
			leaf[2] = ~ ( leaf[2] ) ;
		}
	}

	/// Get edge parity
	int getEdgeParity( UCHAR* leaf, int index ) 
	{
		int a = ( ( ((USHORT *) leaf)[ 0 ] >> index ) & 1 ) ;
		return 	a ;
	};

	/// Get edge parity on a face
	int getFaceParity ( UCHAR* leaf, int index )
	{
		int a = getEdgeParity( leaf, faceMap[ index ][ 0 ] ) + 
				getEdgeParity( leaf, faceMap[ index ][ 1 ] ) + 
				getEdgeParity( leaf, faceMap[ index ][ 2 ] ) + 
				getEdgeParity( leaf, faceMap[ index ][ 3 ] ) ;
		return ( a & 1 ) ;
	}
	int getFaceEdgeNum ( UCHAR* leaf, int index )
	{
		int a = getEdgeParity( leaf, faceMap[ index ][ 0 ] ) + 
				getEdgeParity( leaf, faceMap[ index ][ 1 ] ) + 
				getEdgeParity( leaf, faceMap[ index ][ 2 ] ) + 
				getEdgeParity( leaf, faceMap[ index ][ 3 ] ) ;
		return a ;
	}

	/// Set edge parity
	void flipEdge( UCHAR* leaf, int index ) 
	{
		((USHORT *) leaf)[ 0 ] ^= ( 1 << index ) ;	
	};
	/// Set 1
	void setEdge( UCHAR* leaf, int index ) 
	{
		((USHORT *) leaf)[ 0 ] |= ( 1 << index ) ;	
	};
	/// Set 0
	void resetEdge( UCHAR* leaf, int index ) 
	{
		((USHORT *) leaf)[ 0 ] &= ( ~ ( 1 << index ) ) ;	
	};

	/// Flipping with a new intersection offset
	void createPrimalEdgesMask( UCHAR* leaf )
	{
		int mask = (( leaf[0] & 1 ) | ( (leaf[0] >> 3) & 2 ) | ( (leaf[1] & 1) << 2 ) ) ;
		leaf[1] |= ( mask << 4 ) ;

	}

	void setStoredEdgesParity( UCHAR* leaf, int pindex )
	{
		leaf[1] |= ( 1 << ( 4 + pindex ) ) ;
	}
	int getStoredEdgesParity( UCHAR* leaf, int pindex )
	{
		return ( ( leaf[1] >> ( 4 + pindex ) ) & 1 ) ;
	}

	UCHAR* flipEdge( UCHAR* leaf, int index, float alpha ) 
	{
		flipEdge( leaf, index ) ;

		if ( ( index & 3 ) == 0 )
		{
			int ind = index / 4 ;
			if ( getEdgeParity( leaf, index ) && ! getStoredEdgesParity( leaf, ind ) )
			{
				// Create a new node
				int num = getNumEdges( leaf ) + 1 ;
				setStoredEdgesParity( leaf, ind ) ;
				int count = getEdgeCount( leaf, ind ) ;
				UCHAR* nleaf = createLeaf( num ) ;
				for ( int i = 0 ; i < leaf_node_bytes ; i ++ )
				{
					nleaf[i] = leaf[i] ;
				}

				setEdgeOffset( nleaf, alpha, count ) ;

				if ( num > 1 )
				{
					float * pts = ( float * ) ( leaf + leaf_node_bytes ) ;
					float * npts = ( float * ) ( nleaf + leaf_node_bytes ) ;
					for ( int i = 0 ; i < count ; i ++ )
					{
						for ( int j = 0 ; j < EDGE_FLOATS ; j ++ )
						{
							npts[i * EDGE_FLOATS + j] = pts[i * EDGE_FLOATS + j] ;
						}
					}
					for ( int i = count + 1 ; i < num ; i ++ )
					{
						for ( int j = 0 ; j < EDGE_FLOATS ; j ++ )
						{
							npts[i * EDGE_FLOATS + j] = pts[ (i - 1) * EDGE_FLOATS + j] ;
						}
					}
				}

				
				removeLeaf( num-1, leaf ) ;
				leaf = nleaf ;
			}
		}

		return leaf ;
	};

	/// Update parent link
	void updateParent( UCHAR* node, int len, int st[3], UCHAR* leaf ) 
	{
		// First, locate the parent
		int count ;
		UCHAR* parent = locateParent( node, len, st, count ) ;

		// UPdate
		setChild( parent, count, leaf ) ;
	}

	void updateParent( UCHAR* node, int len, int st[3] ) 
	{
		if ( len == dimen )
		{
			root = node ;
			return ;
		}

		// First, locate the parent
		int count ;
		UCHAR* parent = locateParent( len, st, count ) ;

		// UPdate
		setChild( parent, count, node ) ;
	}

	/// Find edge intersection on a given edge
	int getEdgeIntersectionByIndex( int st[3], int index, float pt[3], int check )
	{
		// First, locat the leaf
		UCHAR* leaf ;
		if ( check )
		{
			leaf = locateLeafCheck( st ) ;
		}
		else
		{
			leaf = locateLeaf( st ) ;
		}

		if ( leaf && getStoredEdgesParity( leaf, index ) )
		{
			float off = getEdgeOffset( leaf, getEdgeCount( leaf, index ) ) ;
			pt[0] = (float) st[0] ;
			pt[1] = (float) st[1] ;
			pt[2] = (float) st[2] ;
			pt[index] += off * mindimen ;

			return 1 ;
		}
		else
		{
			return 0 ;
		}
	}

	/// Retrieve number of edges intersected
	int getPrimalEdgesMask( UCHAR* leaf )
	{
		// return (( leaf[0] & 1 ) | ( (leaf[0] >> 3) & 2 ) | ( (leaf[1] & 1) << 2 ) ) ;
		return ( ( leaf[1] >> 4 ) & 7 ) ;
	}

	int getPrimalEdgesMask2( UCHAR* leaf )
	{
		return (( leaf[0] & 1 ) | ( (leaf[0] >> 3) & 2 ) | ( (leaf[1] & 1) << 2 ) ) ;
	}

	/// Get the count for a primary edge
	int getEdgeCount( UCHAR* leaf, int index )
	{
		return edgeCountTable[ getPrimalEdgesMask( leaf ) ][ index ] ;
	}
	int getNumEdges( UCHAR* leaf )
	{
		return numEdgeTable[ getPrimalEdgesMask( leaf ) ] ;
	}

	int getNumEdges2( UCHAR* leaf )
	{
		return numEdgeTable[ getPrimalEdgesMask2( leaf ) ] ;
	}

	/// Set edge intersection
	void setEdgeOffset( UCHAR* leaf, float pt, int count )
	{
		float * pts = ( float * ) ( leaf + leaf_node_bytes ) ;
#ifdef USE_HERMIT
		pts[ EDGE_FLOATS * count ] = pt ;
		pts[ EDGE_FLOATS * count + 1 ] = 0 ;
		pts[ EDGE_FLOATS * count + 2 ] = 0 ;
		pts[ EDGE_FLOATS * count + 3 ] = 0 ;
#else
		pts[ count ] = pt ;
#endif
	}

	/// Set multiple edge intersections
	void setEdgeOffsets( UCHAR* leaf, float pt[3], int len )
	{
		float * pts = ( float * ) ( leaf + leaf_node_bytes ) ;
		for ( int i = 0 ; i < len ; i ++ )
		{
			pts[i] = pt[i] ;
		}
	}

	/// Retrieve edge intersection
	float getEdgeOffset( UCHAR* leaf, int count )
	{
#ifdef USE_HERMIT
		return (( float * ) ( leaf + leaf_node_bytes ))[ 4 * count ] ;
#else
		return (( float * ) ( leaf + leaf_node_bytes ))[ count ] ;
#endif
	}

	/// Update method
	UCHAR* updateEdgeOffsets( UCHAR* leaf, int oldlen, int newlen, float offs[3] )
	{
		// First, create a new leaf node
		UCHAR* nleaf = createLeaf( newlen ) ;
		for ( int i = 0 ; i < leaf_node_bytes ; i ++ )
		{
			nleaf[i] = leaf[i] ;
		}

		// Next, fill in the offsets
		setEdgeOffsets( nleaf, offs, newlen ) ;

		// Finally, delete the old leaf
		removeLeaf( oldlen, leaf ) ;

		return nleaf ;
	}

	/// Set original vertex index
	void setOriginalIndex( UCHAR* leaf, int index )
	{
		((int *)( leaf + leaf_node_bytes ))[ 0 ] = index ;
	}
	int getOriginalIndex( UCHAR* leaf )
	{
		return 	((int *)( leaf + leaf_node_bytes ))[ 0 ] ;
	}
#ifdef USE_HERMIT
	/// Set minimizer index
	void setMinimizerIndex( UCHAR* leaf, int index )
	{
		((int *)( leaf + leaf_node_bytes - leaf_extra_bytes - 4 ))[ 0 ] = index ;
	}

	/// Get minimizer index
	int getMinimizerIndex( UCHAR* leaf )
	{
		return ((int *)( leaf + leaf_node_bytes - leaf_extra_bytes - 4 ))[ 0 ] ;
	}
	
	int getMinimizerIndex( UCHAR* leaf, int eind )
	{
		int add = manifold_table[ getSignMask( leaf ) ].pairs[ eind ][ 0 ] - 1 ;
		if ( add < 0 )
		{
			printf("Manifold components wrong!\n") ;
		}
		return ((int *)( leaf + leaf_node_bytes - leaf_extra_bytes - 4 ))[ 0 ] + add ;
	}

	void getMinimizerIndices( UCHAR* leaf, int eind, int inds[2] )
	{
		const int* add = manifold_table[ getSignMask( leaf ) ].pairs[ eind ] ;
		inds[0] = ((int *)( leaf + leaf_node_bytes - leaf_extra_bytes - 4 ))[ 0 ] + add[0] - 1 ;
		if ( add[0] == add[1] )
		{
			inds[1] = -1 ;
		}
		else
		{
			inds[1] = ((int *)( leaf + leaf_node_bytes - leaf_extra_bytes - 4 ))[ 0 ] + add[1] - 1 ;
		}
	}


	/// Set edge intersection
	void setEdgeOffsetNormal( UCHAR* leaf, float pt, float a, float b, float c, int count )
	{
		float * pts = ( float * ) ( leaf + leaf_node_bytes ) ;
		pts[ 4 * count ] = pt ;
		pts[ 4 * count + 1 ] = a ;
		pts[ 4 * count + 2 ] = b ;
		pts[ 4 * count + 3 ] = c ;
	}

	float getEdgeOffsetNormal( UCHAR* leaf, int count, float& a, float& b, float& c )
	{
		float * pts = ( float * ) ( leaf + leaf_node_bytes ) ;
		a = pts[ 4 * count + 1 ] ;
		b = pts[ 4 * count + 2 ] ;
		c = pts[ 4 * count + 3 ] ;
		return pts[ 4 * count ] ;
	}

	/// Set multiple edge intersections
	void setEdgeOffsetsNormals( UCHAR* leaf, float pt[], float a[], float b[], float c[], int len )
	{
		float * pts = ( float * ) ( leaf + leaf_node_bytes ) ;
		for ( int i = 0 ; i < len ; i ++ )
		{
			if ( pt[i] > 1 || pt[i] < 0 )
			{
				printf("\noffset: %f\n", pt[i]) ;
			}
			pts[ i * 4 ] = pt[i] ;
			pts[ i * 4 + 1 ] = a[i] ;
			pts[ i * 4 + 2 ] = b[i] ;
			pts[ i * 4 + 3 ] = c[i] ;
		}
	}

	/// Retrieve complete edge intersection
	void getEdgeIntersectionByIndex( UCHAR* leaf, int index, int st[3], int len, float pt[3], float nm[3] )
	{
		int count = getEdgeCount( leaf, index ) ;
		float * pts = ( float * ) ( leaf + leaf_node_bytes ) ;
		
		float off = pts[ 4 * count ] ;
		
		pt[0] =  (float) st[0] ;
		pt[1] =  (float) st[1] ;
		pt[2] =  (float) st[2] ;
		pt[ index ] += ( off * len ) ;

		nm[0] = pts[ 4 * count + 1 ] ;
		nm[1] = pts[ 4 * count + 2 ] ;
		nm[2] = pts[ 4 * count + 3 ] ;
	}

	float getEdgeOffsetNormalByIndex( UCHAR* leaf, int index, float nm[3] )
	{
		int count = getEdgeCount( leaf, index ) ;
		float * pts = ( float * ) ( leaf + leaf_node_bytes ) ;
		
		float off = pts[ 4 * count ] ;
		
		nm[0] = pts[ 4 * count + 1 ] ;
		nm[1] = pts[ 4 * count + 2 ] ;
		nm[2] = pts[ 4 * count + 3 ] ;

		return off ;
	}

	void fillEdgeIntersections( UCHAR* leaf, int st[3], int len, float pts[12][3], float norms[12][3] )
	{
		int i ;
		// int stt[3] = { 0, 0, 0 } ;

		// The three primal edges are easy
		int pmask[3] = { 0, 4, 8 } ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			if ( getEdgeParity( leaf, pmask[i] ) )
			{
				// getEdgeIntersectionByIndex( leaf, i, stt, 1, pts[ pmask[i] ], norms[ pmask[i] ] ) ;
				getEdgeIntersectionByIndex( leaf, i, st, len, pts[ pmask[i] ], norms[ pmask[i] ] ) ;
			}
		}
		
		// 3 face adjacent cubes
		int fmask[3][2] = {{6,10},{2,9},{1,5}} ;
		int femask[3][2] = {{1,2},{0,2},{0,1}} ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			int e1 = getEdgeParity( leaf, fmask[i][0] ) ;
			int e2 = getEdgeParity( leaf, fmask[i][1] ) ;
			if ( e1 || e2 )
			{
				int nst[3] = {st[0], st[1], st[2]} ;
				nst[ i ] += len ;
				// int nstt[3] = {0, 0, 0} ;
				// nstt[ i ] += 1 ;
				UCHAR* node = locateLeaf( nst ) ;
				
				if ( e1 )
				{
					// getEdgeIntersectionByIndex( node, femask[i][0], nstt, 1, pts[ fmask[i][0] ], norms[ fmask[i][0] ] ) ;
					getEdgeIntersectionByIndex( node, femask[i][0], nst, len, pts[ fmask[i][0] ], norms[ fmask[i][0] ] ) ;
				}
				if ( e2 )
				{
					// getEdgeIntersectionByIndex( node, femask[i][1], nstt, 1, pts[ fmask[i][1] ], norms[ fmask[i][1] ] ) ;
					getEdgeIntersectionByIndex( node, femask[i][1], nst, len, pts[ fmask[i][1] ], norms[ fmask[i][1] ] ) ;
				}
			}
		}
		
		// 3 edge adjacent cubes
		int emask[3] = {3, 7, 11} ;
		int eemask[3] = {0, 1, 2} ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			if ( getEdgeParity( leaf, emask[i] ) )
			{
				int nst[3] = {st[0] + len, st[1] + len, st[2] + len} ;
				nst[ i ] -= len ;
				// int nstt[3] = {1, 1, 1} ;
				// nstt[ i ] -= 1 ;
				UCHAR* node = locateLeaf( nst ) ;
				
				// getEdgeIntersectionByIndex( node, eemask[i], nstt, 1, pts[ emask[i] ], norms[ emask[i] ] ) ;
				getEdgeIntersectionByIndex( node, eemask[i], nst, len, pts[ emask[i] ], norms[ emask[i] ] ) ;
			}
		}
	}


	void fillEdgeIntersections( UCHAR* leaf, int st[3], int len, float pts[12][3], float norms[12][3], int parity[12] )
	{
		int i ;
		for ( i = 0 ; i < 12 ; i ++ )
		{
			parity[ i ] = 0 ;
		}
		// int stt[3] = { 0, 0, 0 } ;

		// The three primal edges are easy
		int pmask[3] = { 0, 4, 8 } ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			if ( getStoredEdgesParity( leaf, i ) )
			{
				// getEdgeIntersectionByIndex( leaf, i, stt, 1, pts[ pmask[i] ], norms[ pmask[i] ] ) ;
				getEdgeIntersectionByIndex( leaf, i, st, len, pts[ pmask[i] ], norms[ pmask[i] ] ) ;
				parity[ pmask[i] ] = 1 ;
			}
		}
		
		// 3 face adjacent cubes
		int fmask[3][2] = {{6,10},{2,9},{1,5}} ;
		int femask[3][2] = {{1,2},{0,2},{0,1}} ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			{
				int nst[3] = {st[0], st[1], st[2]} ;
				nst[ i ] += len ;
				// int nstt[3] = {0, 0, 0} ;
				// nstt[ i ] += 1 ;
				UCHAR* node = locateLeafCheck( nst ) ;
				if ( node == NULL )
				{
					continue ;
				}
		
				int e1 = getStoredEdgesParity( node, femask[i][0] ) ;
				int e2 = getStoredEdgesParity( node, femask[i][1] ) ;
				
				if ( e1 )
				{
					// getEdgeIntersectionByIndex( node, femask[i][0], nstt, 1, pts[ fmask[i][0] ], norms[ fmask[i][0] ] ) ;
					getEdgeIntersectionByIndex( node, femask[i][0], nst, len, pts[ fmask[i][0] ], norms[ fmask[i][0] ] ) ;
					parity[ fmask[i][0] ] = 1 ;
				}
				if ( e2 )
				{
					// getEdgeIntersectionByIndex( node, femask[i][1], nstt, 1, pts[ fmask[i][1] ], norms[ fmask[i][1] ] ) ;
					getEdgeIntersectionByIndex( node, femask[i][1], nst, len, pts[ fmask[i][1] ], norms[ fmask[i][1] ] ) ;
					parity[ fmask[i][1] ] = 1 ;
				}
			}
		}
		
		// 3 edge adjacent cubes
		int emask[3] = {3, 7, 11} ;
		int eemask[3] = {0, 1, 2} ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
//			if ( getEdgeParity( leaf, emask[i] ) )
			{
				int nst[3] = {st[0] + len, st[1] + len, st[2] + len} ;
				nst[ i ] -= len ;
				// int nstt[3] = {1, 1, 1} ;
				// nstt[ i ] -= 1 ;
				UCHAR* node = locateLeafCheck( nst ) ;
				if ( node == NULL )
				{
					continue ;
				}
				
				if ( getStoredEdgesParity( node, eemask[i] ) )
				{
					// getEdgeIntersectionByIndex( node, eemask[i], nstt, 1, pts[ emask[i] ], norms[ emask[i] ] ) ;
					getEdgeIntersectionByIndex( node, eemask[i], nst, len, pts[ emask[i] ], norms[ emask[i] ] ) ;
					parity[ emask[ i ] ] = 1 ;
				}
			}
		}
	}

	void fillEdgeOffsetsNormals( UCHAR* leaf, int st[3], int len, float pts[12], float norms[12][3], int parity[12] )
	{
		int i ;
		for ( i = 0 ; i < 12 ; i ++ )
		{
			parity[ i ] = 0 ;
		}
		// int stt[3] = { 0, 0, 0 } ;

		// The three primal edges are easy
		int pmask[3] = { 0, 4, 8 } ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			if ( getStoredEdgesParity( leaf, i ) )
			{
				pts[ pmask[i] ] = getEdgeOffsetNormalByIndex( leaf, i, norms[ pmask[i] ] ) ;
				parity[ pmask[i] ] = 1 ;
			}
		}
		
		// 3 face adjacent cubes
		int fmask[3][2] = {{6,10},{2,9},{1,5}} ;
		int femask[3][2] = {{1,2},{0,2},{0,1}} ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			{
				int nst[3] = {st[0], st[1], st[2]} ;
				nst[ i ] += len ;
				// int nstt[3] = {0, 0, 0} ;
				// nstt[ i ] += 1 ;
				UCHAR* node = locateLeafCheck( nst ) ;
				if ( node == NULL )
				{
					continue ;
				}
		
				int e1 = getStoredEdgesParity( node, femask[i][0] ) ;
				int e2 = getStoredEdgesParity( node, femask[i][1] ) ;
				
				if ( e1 )
				{
					pts[ fmask[i][0] ] = getEdgeOffsetNormalByIndex( node, femask[i][0], norms[ fmask[i][0] ] ) ;
					parity[ fmask[i][0] ] = 1 ;
				}
				if ( e2 )
				{
					pts[ fmask[i][1] ] = getEdgeOffsetNormalByIndex( node, femask[i][1], norms[ fmask[i][1] ] ) ;
					parity[ fmask[i][1] ] = 1 ;
				}
			}
		}
		
		// 3 edge adjacent cubes
		int emask[3] = {3, 7, 11} ;
		int eemask[3] = {0, 1, 2} ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
//			if ( getEdgeParity( leaf, emask[i] ) )
			{
				int nst[3] = {st[0] + len, st[1] + len, st[2] + len} ;
				nst[ i ] -= len ;
				// int nstt[3] = {1, 1, 1} ;
				// nstt[ i ] -= 1 ;
				UCHAR* node = locateLeafCheck( nst ) ;
				if ( node == NULL )
				{
					continue ;
				}
				
				if ( getStoredEdgesParity( node, eemask[i] ) )
				{
					pts[ emask[i] ] = getEdgeOffsetNormalByIndex( node, eemask[i], norms[ emask[i] ] ) ;
					parity[ emask[ i ] ] = 1 ;
				}
			}
		}
	}


	/// Update method
	UCHAR* updateEdgeOffsetsNormals( UCHAR* leaf, int oldlen, int newlen, float offs[3], float a[3], float b[3], float c[3] )
	{
		// First, create a new leaf node
		UCHAR* nleaf = createLeaf( newlen ) ;
		for ( int i = 0 ; i < leaf_node_bytes ; i ++ )
		{
			nleaf[i] = leaf[i] ;
		}

		// Next, fill in the offsets
		setEdgeOffsetsNormals( nleaf, offs, a, b, c, newlen ) ;

		// Finally, delete the old leaf
		removeLeaf( oldlen, leaf ) ;

		return nleaf ;
	}
#endif

	/// Locate a leaf
	/// WARNING: assuming this leaf already exists!
	
	UCHAR* locateLeaf( int st[3] )
	{
		UCHAR* node = root ;
		for ( int i = GRID_DIMENSION - 1 ; i > GRID_DIMENSION - maxDepth - 1 ; i -- )
		{
			int index = ( ( ( st[0] >> i ) & 1 ) << 2 ) | 
						( ( ( st[1] >> i ) & 1 ) << 1 ) | 
						( ( ( st[2] >> i ) & 1 ) ) ;
			node = getChild( node, getChildCount( node, index ) ) ;
		}

		return node ;
	}
	
	UCHAR* locateLeaf( UCHAR* node, int len, int st[3] )
	{
		int index ;
		for ( int i = len / 2 ; i >= mindimen ; i >>= 1 )
		{
			index = ( ( ( st[0] & i ) ? 4 : 0 ) | 
					( ( st[1] & i ) ? 2 : 0 ) | 
					( ( st[2] & i ) ? 1 : 0 ) ) ;
			node = getChild( node, getChildCount( node, index ) ) ;
		}

		return node ;
	}
	UCHAR* locateLeafCheck( int st[3] )
	{
		UCHAR* node = root ;
		for ( int i = GRID_DIMENSION - 1 ; i > GRID_DIMENSION - maxDepth - 1 ; i -- )
		{
			int index = ( ( ( st[0] >> i ) & 1 ) << 2 ) | 
						( ( ( st[1] >> i ) & 1 ) << 1 ) | 
						( ( ( st[2] >> i ) & 1 ) ) ;
			if ( ! hasChild( node, index ) )
			{
				return NULL ;
			}
			node = getChild( node, getChildCount( node, index ) ) ;
		}

		return node ;
	}
	UCHAR* locateParent( int len, int st[3], int& count )
	{
		UCHAR* node = root ;
		UCHAR* pre = NULL ;
		int index = 0 ;
		for ( int i = dimen / 2 ; i >= len ; i >>= 1 )
		{
			index = ( ( ( st[0] & i ) ? 4 : 0 ) | 
					( ( st[1] & i ) ? 2 : 0 ) | 
					( ( st[2] & i ) ? 1 : 0 ) ) ;
			pre = node ;
			node = getChild( node, getChildCount( node, index ) ) ;
		}

		count = getChildCount( pre, index ) ;
		return pre ;
	}
	UCHAR* locateParent( UCHAR* papa, int len, int st[3], int& count )
	{
		UCHAR* node = papa ;
		UCHAR* pre = NULL ;
		int index = 0;
		for ( int i = len / 2 ; i >= mindimen ; i >>= 1 )
		{
			index = ( ( ( st[0] & i ) ? 4 : 0 ) | 
					( ( st[1] & i ) ? 2 : 0 ) | 
					( ( st[2] & i ) ? 1 : 0 ) ) ;
			pre = node ;
			node = getChild( node, getChildCount( node, index ) ) ;
		}

		count = getChildCount( pre, index ) ;
		return pre ;
	}
	/************ Operators for internal nodes ************/

	/// Print the node information
	void printNode( UCHAR* node )
	{
		printf("Address: %p ", node ) ;
		printf("Leaf Mask: ") ;
		for ( int i = 0 ; i < 8 ; i ++ )
		{
			printf( "%d ", isLeaf( node, i ) ) ;
		}
		printf("Child Mask: ") ;
		for ( int i = 0 ; i < 8 ; i ++ )
		{
			printf( "%d ", hasChild( node, i ) ) ;
		}
		printf("\n") ;
	}

	/// Get size of an internal node
	int getSize ( int length )
	{
		return INTERNAL_NODE_BYTES + length * 4 ;	
	};

	/// If child index exists
	int hasChild( UCHAR* node, int index )
	{
		return ( node[0] >> index ) & 1 ;
	};

	/// Test if child is leaf
	int isLeaf ( UCHAR* node, int index )
	{
		return ( node[1] >> index ) & 1 ;
	};

	/// Get the pointer to child index
	UCHAR* getChild ( UCHAR* node, int count )
	{
		return (( UCHAR ** ) ( node + INTERNAL_NODE_BYTES )) [ count ] ;	
	};

	/// Get total number of children
	int getNumChildren( UCHAR* node )
	{
		return numChildrenTable[ node[0] ] ;
	};

	/// Get the count of children
	int getChildCount( UCHAR* node, int index )
	{
		return childrenCountTable[ node[0] ][ index ] ;
	}
	int getChildIndex( UCHAR* node, int count )
	{
		return childrenIndexTable[ node[0] ][ count ] ;
	}
	int* getChildCounts( UCHAR* node )
	{
		return childrenCountTable[ node[0] ] ;
	}

	/// Get all children
	void fillChildren( UCHAR* node, UCHAR* chd[ 8 ], int leaf[ 8 ] )
	{
		int count = 0 ;
		for ( int i = 0 ; i < 8 ; i ++ )
		{	
			leaf[ i ] = isLeaf( node, i ) ;
			if ( hasChild( node, i ) )
			{
				chd[ i ] = getChild( node, count ) ;
				count ++ ;
			}
			else
			{
			 	chd[ i ] = NULL ;
				leaf[ i ] = 0 ;
			}
		}
	}

	/// Sets the child pointer
	void setChild ( UCHAR* node, int count, UCHAR* chd )
	{
		(( UCHAR ** ) ( node + INTERNAL_NODE_BYTES )) [ count ] = chd ;
	}
	void setInternalChild ( UCHAR* node, int index, int count, UCHAR* chd )
	{
		setChild( node, count, chd ) ;
		node[0] |= ( 1 << index ) ;
	};
	void setLeafChild ( UCHAR* node, int index, int count, UCHAR* chd )
	{
		setChild( node, count, chd ) ;
		node[0] |= ( 1 << index ) ;
		node[1] |= ( 1 << index ) ;
	};

	/// Add a kid to an existing internal node
	/// Fix me: can we do this without wasting memory ?
	/// Fixed: using variable memory
	UCHAR* addChild( UCHAR* node, int index, UCHAR* chd, int aLeaf )
	{
		// Create new internal node
		int num = getNumChildren( node ) ;
		UCHAR* rnode = createInternal( num + 1 ) ;

		// Establish children
		int i ;
		int count1 = 0, count2 = 0 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( i == index )
			{
				if ( aLeaf )
				{
					setLeafChild( rnode, i, count2, chd ) ;
				}
				else
				{
					setInternalChild( rnode, i, count2, chd ) ;
				}
				count2 ++ ;
			}
			else if ( hasChild( node, i ) )
			{
				if ( isLeaf( node, i ) )
				{
					setLeafChild( rnode, i, count2, getChild( node, count1 ) ) ;
				}
				else
				{
					setInternalChild( rnode, i, count2, getChild( node, count1 ) ) ;
				}
				count1 ++ ;
				count2 ++ ;
			}
		}

		removeInternal( num, node ) ;
		return rnode ;
	}

	/// Allocate a node
	UCHAR* createInternal( int length )
	{
		UCHAR* inode = alloc[ length ]->allocate( ) ;
		inode[0] = inode[1] = 0 ;
		return inode ;
	};
	UCHAR* createLeaf( int length )
	{
		if ( length > 3 )
		{
			printf("wierd");
		}
		UCHAR* lnode = leafalloc[ length ]->allocate( ) ;
		lnode[0] = lnode[1] = lnode[2] = 0 ;

		return lnode ;
	};

	void removeInternal ( int num, UCHAR* node )
	{
		alloc[ num ]->deallocate( node ) ;
	}

	void removeLeaf ( int num, UCHAR* leaf )
	{
		if ( num > 3 || num < 0 )
		{
			printf("wierd");
		}
		leafalloc[ num ]->deallocate( leaf ) ;
	}

	/// Add a leaf (by creating a new par node with the leaf added)
	UCHAR* addLeafChild ( UCHAR* par, int index, int count, UCHAR* leaf )
	{
		int num = getNumChildren( par ) + 1 ;
		UCHAR* npar = createInternal( num ) ;
		npar[0] = par[0] ;
		npar[1] = par[1] ;
		
		if ( num == 1 )
		{
			setLeafChild( npar, index, 0, leaf ) ;
		}
		else
		{
			int i ;
			for ( i = 0 ; i < count ; i ++ )
			{
				setChild( npar, i, getChild( par, i ) ) ;
			}
			setLeafChild( npar, index, count, leaf ) ;
			for ( i = count + 1 ; i < num ; i ++ )
			{
				setChild( npar, i, getChild( par, i - 1 ) ) ;
			}
		}
		
		removeInternal( num-1, par ) ;
		return npar ;
	};

	UCHAR* addInternalChild ( UCHAR* par, int index, int count, UCHAR* node )
	{
		int num = getNumChildren( par ) + 1 ;
		UCHAR* npar = createInternal( num ) ;
		npar[0] = par[0] ;
		npar[1] = par[1] ;
		
		if ( num == 1 )
		{
			setInternalChild( npar, index, 0, node ) ;
		}
		else
		{
			int i ;
			for ( i = 0 ; i < count ; i ++ )
			{
				setChild( npar, i, getChild( par, i ) ) ;
			}
			setInternalChild( npar, index, count, node ) ;
			for ( i = count + 1 ; i < num ; i ++ )
			{
				setChild( npar, i, getChild( par, i - 1 ) ) ;
			}
		}
		
		removeInternal( num-1, par ) ;
		return npar ;
	};
};



#endif
