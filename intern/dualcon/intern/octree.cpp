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

#include "octree.h"
#include <Eigen/Dense>
#include <limits>
#include <time.h>

/**
 * Implementations of Octree member functions.
 *
 * @author Tao Ju
 */

/* set to non-zero value to enable debugging output */
#define DC_DEBUG 0

#if DC_DEBUG
/* enable debug printfs */
#define dc_printf printf
#else
/* disable debug printfs */
#define dc_printf(...) do {} while(0)
#endif

Octree::Octree( ModelReader* mr,
				DualConAllocOutput alloc_output_func,
				DualConAddVert add_vert_func,
				DualConAddQuad add_quad_func,
				DualConFlags flags, DualConMode dualcon_mode, int depth,
				float threshold, float sharpness )
	: use_flood_fill(flags & DUALCON_FLOOD_FILL),
	  /* note on `use_manifold':
		 
		 After playing around with this option, the only case I could
		 find where this option gives different results is on
		 relatively thin corners. Sometimes along these corners two
		 vertices from seperate sides will be placed in the same
		 position, so hole gets filled with a 5-sided face, where two
		 of those vertices are in the same 3D location. If
		 `use_manifold' is disabled, then the modifier doesn't
		 generate two separate vertices so the results end up as all
		 quads.

		 Since the results are just as good with all quads, there
		 doesn't seem any reason to allow this to be toggled in
		 Blender. -nicholasbishop
	   */
	  use_manifold(false),
	  hermite_num(sharpness),
	  mode(dualcon_mode),
	  alloc_output(alloc_output_func),
	  add_vert(add_vert_func),
	  add_quad(add_quad_func)
{
	this->thresh = threshold ;
	this->reader = mr ;
	this->dimen = 1 << GRID_DIMENSION ;
	this->range = reader->getBoundingBox( this->origin ) ;
	this->nodeCount = this->nodeSpace = 0;
	this->maxDepth = depth ;
	this->mindimen = ( dimen >> maxDepth ) ;
	this->minshift = ( GRID_DIMENSION - maxDepth ) ;
	this->buildTable( ) ;

	flood_bytes = use_flood_fill ? FLOOD_FILL_BYTES : 0;
	leaf_extra_bytes = flood_bytes + CINDY_BYTES;

#ifdef USE_HERMIT
	leaf_node_bytes = 7 + leaf_extra_bytes;
#else
	leaf_node_bytes = 3 + leaf_extra_bytes;
#endif

#ifdef QIANYI
	dc_printf("Origin: (%f %f %f), Dimension: %f\n", origin[0], origin[1], origin[2], range) ;
#endif

	this->maxTrianglePerCell = 0 ;

	// Initialize memory
#ifdef IN_VERBOSE_MODE
	dc_printf("Range: %f origin: %f, %f,%f \n", range, origin[0], origin[1], origin[2] ) ;
	dc_printf("Initialize memory...\n") ;
#endif
	initMemory( ) ;
	this->root = createInternal( 0 ) ;

	// Read MC table
#ifdef IN_VERBOSE_MODE
	dc_printf("Reading contour table...\n") ;
#endif
	this->cubes = new Cubes();

}

Octree::~Octree( )
{
	freeMemory( ) ;
}

void Octree::scanConvert()
{
	// Scan triangles
#if DC_DEBUG
	clock_t start, finish ;
	start = clock( ) ;
#endif
	
	this->addTrian( ) ;
	this->resetMinimalEdges( ) ;
	this->preparePrimalEdgesMask( this->root ) ;

#if DC_DEBUG
	finish = clock( ) ;
	dc_printf("Time taken: %f seconds                   \n", 
		(double)(finish - start) / CLOCKS_PER_SEC ) ;
#endif

	// Generate signs
	// Find holes
#if DC_DEBUG
	dc_printf("Patching...\n") ;
	start = clock( ) ;
#endif
	this->trace( ) ;
#if DC_DEBUG
	finish = clock( ) ;
	dc_printf("Time taken: %f seconds \n",	(double)(finish - start) / CLOCKS_PER_SEC ) ;
#ifdef IN_VERBOSE_MODE
	dc_printf("Holes: %d Average Length: %f Max Length: %d \n", numRings, (float)totRingLengths / (float) numRings, maxRingLength ) ;
#endif
#endif
	
	// Check again
	int tnumRings = numRings ;
	this->trace( ) ;
#ifdef IN_VERBOSE_MODE
	dc_printf("Holes after patching: %d \n", numRings) ;
#endif	
	numRings = tnumRings ;

#if DC_DEBUG
	dc_printf("Building signs...\n") ;
	start = clock( ) ;
#endif
	this->buildSigns( ) ;
#if DC_DEBUG
	finish = clock( ) ;
	dc_printf("Time taken: %f seconds \n",	(double)(finish - start) / CLOCKS_PER_SEC ) ;
#endif

	if(use_flood_fill) {
		/*
		  start = clock( ) ;
		  this->floodFill( ) ;
		  // Check again
		  tnumRings = numRings ;
		  this->trace( ) ;
		  dc_printf("Holes after filling: %d \n", numRings) ;
		  numRings = tnumRings ;
		  this->buildSigns( ) ;
		  finish = clock( ) ;
		  dc_printf("Time taken: %f seconds \n",	(double)(finish - start) / CLOCKS_PER_SEC ) ;
		*/
#if DC_DEBUG
		start = clock( ) ;
		dc_printf("Removing components...\n");
#endif
		this->floodFill( ) ;
		this->buildSigns( ) ;
		//	dc_printf("Checking...\n");
		//	this->floodFill( ) ;
#if DC_DEBUG
		finish = clock( ) ;
		dc_printf("Time taken: %f seconds \n", (double)(finish - start) / CLOCKS_PER_SEC ) ;
#endif
	}

	// Output
#ifdef OUTPUT_REPAIRED
#if DC_DEBUG
	start = clock( ) ;
#endif
	writeOut();
#if DC_DEBUG
	finish = clock( ) ;
#endif
	// dc_printf("Time taken: %f seconds \n",	(double)(finish - start) / CLOCKS_PER_SEC ) ;
#ifdef CINDY
	this->writeTags( "tags.txt" ) ;
	dc_printf("Tags output to tags.txt\n") ;
#endif

#endif

	// Print info
#ifdef IN_VERBOSE_MODE
	printMemUsage( ) ;
#endif
}

#if 0
void Octree::writeOut( char* fname )
{
	dc_printf( "\n" ) ;
	if ( strstr( fname, ".ply" ) != NULL )
	{
		dc_printf("Writing PLY file format.\n") ;
		this->outType = 1 ;
		writePLY( fname ) ;
	} 
	else if ( strstr( fname, ".off" ) != NULL )
	{
		dc_printf("Writing OFF file format.\n") ;
		this->outType = 0 ;
		writeOFF( fname ) ;
	}
	else if ( strstr( fname, ".sof" ) != NULL )
	{
		dc_printf("Writing Signed Octree File format.\n") ;
		this->outType = 2 ;
		writeOctree( fname ) ;
	}
	else if ( strstr( fname, ".dcf" ) != NULL )
	{
#ifdef USE_HERMIT
		dc_printf("Writing Dual Contouring File format.\n") ;
		this->outType = 3 ;
		writeDCF( fname ) ;
#else
		dc_printf("Can not write Dual Contouring File format in non-DC mode.\n") ;
#endif
	}
#ifdef USE_HERMIT
	else if ( strstr( fname, ".sog" ) != NULL )
	{
		dc_printf("Writing signed octree with geometry.\n") ;
		this->outType = 4 ;
		writeOctreeGeom( fname ) ;
	}
#endif
	/*
	else if ( strstr( fname, ".sof" ) != NULL )
	{
		dc_printf("Writing SOF file format.\n") ;
		this->outType = 2 ;
		writeOctree( fname ) ;
	}
	*/
	else
	{
		dc_printf("Unknown output format.\n") ;
	}

}
#endif

void Octree::initMemory( )
{
#ifdef USE_HERMIT
	const int leaf_node_bytes = 7;
#else
	const int leaf_node_bytes = 3;
#endif

	if(use_flood_fill) {
		const int bytes = leaf_node_bytes + CINDY_BYTES + FLOOD_FILL_BYTES;
		this->leafalloc[ 0 ] = new MemoryAllocator< bytes > ( ) ;
		this->leafalloc[ 1 ] = new MemoryAllocator< bytes + EDGE_BYTES > ( ) ;
		this->leafalloc[ 2 ] = new MemoryAllocator< bytes + EDGE_BYTES * 2 > ( ) ;
		this->leafalloc[ 3 ] = new MemoryAllocator< bytes + EDGE_BYTES * 3 > ( ) ;
	}
	else {
		const int bytes = leaf_node_bytes + CINDY_BYTES;
		this->leafalloc[ 0 ] = new MemoryAllocator< bytes > ( ) ;
		this->leafalloc[ 1 ] = new MemoryAllocator< bytes + EDGE_BYTES > ( ) ;
		this->leafalloc[ 2 ] = new MemoryAllocator< bytes + EDGE_BYTES * 2 > ( ) ;
		this->leafalloc[ 3 ] = new MemoryAllocator< bytes + EDGE_BYTES * 3 > ( ) ;
	}

	this->alloc[ 0 ] = new MemoryAllocator< INTERNAL_NODE_BYTES > ( ) ;
	this->alloc[ 1 ] = new MemoryAllocator< INTERNAL_NODE_BYTES + POINTER_BYTES > ( ) ;
	this->alloc[ 2 ] = new MemoryAllocator< INTERNAL_NODE_BYTES + POINTER_BYTES*2 > ( ) ;
	this->alloc[ 3 ] = new MemoryAllocator< INTERNAL_NODE_BYTES + POINTER_BYTES*3 > ( ) ;
	this->alloc[ 4 ] = new MemoryAllocator< INTERNAL_NODE_BYTES + POINTER_BYTES*4 > ( ) ;
	this->alloc[ 5 ] = new MemoryAllocator< INTERNAL_NODE_BYTES + POINTER_BYTES*5 > ( ) ;
	this->alloc[ 6 ] = new MemoryAllocator< INTERNAL_NODE_BYTES + POINTER_BYTES*6 > ( ) ;
	this->alloc[ 7 ] = new MemoryAllocator< INTERNAL_NODE_BYTES + POINTER_BYTES*7 > ( ) ;
	this->alloc[ 8 ] = new MemoryAllocator< INTERNAL_NODE_BYTES + POINTER_BYTES*8 > ( ) ;
}

void Octree::freeMemory( )
{
	for ( int i = 0 ; i < 9 ; i ++ )
	{
		alloc[i]->destroy() ;
		delete alloc[i] ;
	}

	for ( int i = 0 ; i < 4 ; i ++ )
	{
		leafalloc[i]->destroy() ;
		delete leafalloc[i] ;
	}
}

void Octree::printMemUsage( )
{
	int totalbytes = 0 ;
	dc_printf("********* Internal nodes: \n") ;
	for ( int i = 0 ; i < 9 ; i ++ )
	{
		this->alloc[ i ]->printInfo() ;

		totalbytes += alloc[i]->getAll( ) * alloc[i]->getBytes() ;
	}
	dc_printf("********* Leaf nodes: \n") ;
	int totalLeafs = 0 ;
	for ( int i = 0 ; i < 4 ; i ++ )
	{
		this->leafalloc[ i ]->printInfo() ;

		totalbytes += leafalloc[i]->getAll( ) * leafalloc[i]->getBytes() ;
		totalLeafs += leafalloc[i]->getAllocated() ;
	}
	
	dc_printf("Total allocated bytes on disk: %d \n", totalbytes) ;
	dc_printf("Total leaf nodes: %d\n", totalLeafs ) ;
}

void Octree::resetMinimalEdges( )
{
	this->cellProcParity( this->root, 0, maxDepth ) ;
}

void Octree::writeModel( char* fname )
{
	reader->reset() ;

	int nFace = reader->getNumTriangles() ;
	Triangle* trian ;
	// int unitcount = 10000;
	int count = 0 ;
	int nVert = nFace * 3 ;
	FILE* modelfout = fopen( "model.off", "w" ) ;
	fprintf( modelfout, "OFF\n" ) ;
	fprintf( modelfout, "%d %d 0\n", nVert, nFace ) ;

	//int total = this->reader->getNumTriangles() ;
	dc_printf( "Start writing model to OFF...\n" ) ;
	srand(0) ;
	while ( ( trian = reader->getNextTriangle() ) != NULL )
	{
		// Drop polygons
		{
			int i, j ;

			// Blow up the triangle
			float mid[3] = {0, 0, 0} ;
			for ( i = 0 ; i < 3 ; i ++ )
				for ( j = 0 ; j < 3 ; j ++ )
				{
					trian->vt[i][j] = dimen * ( trian->vt[i][j] - origin[j] ) / range ;

					mid[j] += trian->vt[i][j] / 3 ;
				}
				
				// Generate projections
				// LONG cube[2][3] = { { 0, 0, 0 }, { dimen, dimen, dimen } } ;
				int trig[3][3] ;

				// Blowing up the triangle to the grid
				for ( i = 0 ; i < 3 ; i ++ )
					for (  j = 0 ; j < 3 ; j ++ )
					{
						trig[i][j] = (int) (trian->vt[i][j]) ;
						// Perturb end points, if set so
					}

					
					for ( i = 0 ; i < 3 ; i ++ )
					{
						fprintf( modelfout, "%f %f %f\n", 
							(float)(((double) trig[i][0] / dimen) * range  + origin[0]) ,
							(float)(((double) trig[i][1] / dimen) * range  + origin[1]) ,
							(float)(((double) trig[i][2] / dimen) * range  + origin[2]) ) ;
					}
		}
		delete trian ;

		count ++ ;
		
	}

	for ( int i = 0 ; i < nFace ; i ++ )
	{
		fprintf( modelfout, "3 %d %d %d\n", 3 * i + 2, 3 * i + 1, 3 * i  ) ;
	}

	fclose( modelfout ) ;

}

#ifdef CINDY
void Octree::writeTags( char* fname )
{
	FILE* fout = fopen( fname, "w" ) ;

	clearCindyBits( root, maxDepth ) ;
	readVertices() ;
	outputTags( root, maxDepth, fout ) ;

	fclose ( fout ) ;
}

void Octree::readVertices( )
{
	int total = this->reader->getNumVertices() ;
	reader->reset() ;
	float v[3] ;
	int st[3] = {0,0,0};
	int unitcount = 1000 ;
	dc_printf( "\nRead in original %d vertices...\n", total ) ;

	for ( int i = 0 ; i < total ; i ++ )
	{
		reader->getNextVertex( v ) ;
		// Blowing up the triangle to the grid
		float mid[3] = {0, 0, 0} ;
		for ( int j = 0 ; j < 3 ; j ++ )
		{
			v[j] = dimen * ( v[j] - origin[j] ) / range ;
		}

//		dc_printf("vertex: %f %f %f, dimen: %d\n", v[0], v[1], v[2], dimen ) ;
		readVertex ( root, st, dimen, maxDepth, v, i ) ;

		
		if ( i % unitcount == 0 )
		{
			putchar ( 13 ) ;

			switch ( ( i / unitcount ) % 4 )
			{
			case 0 : dc_printf("-");
				break ;
			case 1 : dc_printf("/") ;
				break ;
			case 2 : dc_printf("|");
				break ;
			case 3 : dc_printf("\\") ;
				break ;
			}

			float percent = (float) i / total ;
			/*
			int totbars = 50 ;
			int bars = (int)( percent * totbars ) ;
			for ( int i = 0 ; i < bars ; i ++ )
			{
				putchar( 219 ) ;
			}
			for ( i = bars ; i < totbars ; i ++ )
			{
				putchar( 176 ) ;
			}
			*/

			dc_printf(" %d vertices: ", i ) ;
			dc_printf( " %f%% complete.", 100 * percent ) ;
		}
		
	}
	putchar ( 13 ) ;
	dc_printf("                                             \n");
}

void Octree::readVertex(  UCHAR* node, int st[3], int len, int height, float v[3], int index )
{
	int nst[3] ;
	nst[0] = ( (int) v[0] / mindimen ) * mindimen ;
	nst[1] = ( (int) v[1] / mindimen ) * mindimen ;
	nst[2] = ( (int) v[2] / mindimen ) * mindimen ;

	UCHAR* cell = this->locateLeafCheck( nst ) ;
	if ( cell == NULL )
	{
		dc_printf("Cell %d %d %d is not found!\n", nst[0]/ mindimen, nst[1]/ mindimen, nst[2]/ mindimen) ;
		return ;
	}

	setOriginalIndex( cell, index ) ;


	/*
	int i ;
	if ( height == 0 )
	{
		// Leaf cell, assign index
		dc_printf("Setting: %d\n", index ) ;
		setOriginalIndex( node, index ) ;
	}
	else
	{
		len >>= 1 ;
		// Internal cell, check and recur
		int x = ( v[0] > st[0] + len ) ? 1 : 0 ;
		int y = ( v[1] > st[1] + len ) ? 1 : 0 ;
		int z = ( v[2] > st[2] + len ) ? 1 : 0 ;
		int child = x * 4 + y * 2 + z ;

		int count = 0 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( i == child && hasChild( node, i ) )
			{
				int nst[3] ;
				nst[0] = st[0] + vertmap[i][0] * len ;
				nst[1] = st[1] + vertmap[i][1] * len ;
				nst[2] = st[2] + vertmap[i][2] * len ;

				dc_printf("Depth: %d -- child %d vertex: %f %f %f in %f %f %f\n", height - 1, child, v[0]/mindimen, v[1]/mindimen, v[2]/mindimen, 
					nst[0]/mindimen, nst[1]/mindimen, nst[2]/mindimen, len/mindimen ) ;
				
				readVertex( getChild( node, count ), nst, len, height - 1, v, index ) ;
				count ++ ;
			}
		}
	}
	*/
}

void Octree::outputTags( UCHAR* node, int height, FILE* fout )
{
	int i ;

	if ( height == 0 )
	{
		// Leaf cell, generate
		int smask = getSignMask( node ) ;

		if(use_manifold) {
			int comps = manifold_table[ smask ].comps ;
			if ( comps != 1 )
			{
				return ;
			}
		}
		else
		{
			if ( smask == 0 || smask == 255 )
			{
				return ;
			}
		}

		int rindex = getMinimizerIndex( node ) ;
		int oindex = getOriginalIndex( node ) ;

		if ( oindex >= 0 )
		{
			fprintf( fout, "%d: %d\n", rindex, oindex ) ;
		}

	}
	else
	{
		// Internal cell, recur
		int count = 0 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				outputTags( getChild( node, count ), height - 1, fout ) ;
				count ++ ;
			}
		}
	}
}

void Octree::clearCindyBits( UCHAR* node, int height )
{
	int i;

	if ( height == 0 )
	{
		// Leaf cell, 
		{
			setOriginalIndex( node, - 1 ) ;
		}
	}
	else
	{
		// Internal cell, recur
		int count = 0 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				clearCindyBits( getChild( node, count ), height - 1 ) ;
				count ++ ;
			}
		}
	}	
}
#endif

void Octree::addTrian( )
{
	Triangle* trian ;
	int count = 0 ;
	
#if DC_DEBUG
	int total = this->reader->getNumTriangles() ;
	int unitcount = 1000 ;
	dc_printf( "\nScan converting to depth %d...\n", maxDepth ) ;
#endif

	srand(0) ;

	while ( ( trian = reader->getNextTriangle() ) != NULL )
	{
		// Drop triangles
		{
			addTrian ( trian, count ) ;
		}
		delete trian ;

		count ++ ;

#if DC_DEBUG
		if ( count % unitcount == 0 )
		{
			putchar ( 13 ) ;

			switch ( ( count / unitcount ) % 4 )
			{
			case 0 : dc_printf("-");
				break ;
			case 1 : dc_printf("/") ;
				break ;
			case 2 : dc_printf("|");
				break ;
			case 3 : dc_printf("\\") ;
				break ;
			}

			float percent = (float) count / total ;
			
			/*
			int totbars = 50 ;
			int bars = (int)( percent * totbars ) ;
			for ( int i = 0 ; i < bars ; i ++ )
			{
				putchar( 219 ) ;
			}
			for ( i = bars ; i < totbars ; i ++ )
			{
				putchar( 176 ) ;
			}
			*/

			dc_printf(" %d triangles: ", count ) ;
			dc_printf( " %f%% complete.", 100 * percent ) ;
		}
#endif
		
	}
	putchar ( 13 ) ;
}

void Octree::addTrian( Triangle* trian, int triind )
{
	int i, j ;

	// Blowing up the triangle to the grid
	float mid[3] = {0, 0, 0} ;
	for ( i = 0 ; i < 3 ; i ++ )
		for ( j = 0 ; j < 3 ; j ++ )
		{
			trian->vt[i][j] = dimen * ( trian->vt[i][j] - origin[j] ) / range ;
			mid[j] += trian->vt[i][j] / 3 ;
		}

	// Generate projections
	LONG cube[2][3] = { { 0, 0, 0 }, { dimen, dimen, dimen } } ;
	LONG trig[3][3] ;
	
	for ( i = 0 ; i < 3 ; i ++ )
		for (  j = 0 ; j < 3 ; j ++ )
		{
			trig[i][j] = (LONG) (trian->vt[i][j]) ;
	// Perturb end points, if set so
		}
		
	// Add to the octree
	// int start[3] = { 0, 0, 0 } ;
	LONG errorvec = (LONG) ( 0 ) ;
	Projections* proj = new Projections( cube, trig, errorvec, triind ) ;
	root = addTrian( root, proj, maxDepth ) ;
	
	delete proj->inherit ;
	delete proj ;
}


UCHAR* Octree::addTrian( UCHAR* node, Projections* p, int height )
{
	int i ;
	int vertdiff[8][3] = {{0,0,0},{0,0,1},{0,1,-1},{0,0,1},{1,-1,-1},{0,0,1},{0,1,-1},{0,0,1}} ;
	UCHAR boxmask = p->getBoxMask( ) ;
	Projections* subp = new Projections( p ) ;
	
	int count = 0 ;
	int tempdiff[3] = {0,0,0} ;
	for ( i = 0 ; i < 8 ; i ++ )
	{
		tempdiff[0] += vertdiff[i][0] ;
		tempdiff[1] += vertdiff[i][1] ;
		tempdiff[2] += vertdiff[i][2] ;

		/* Quick pruning using bounding box */
		if ( boxmask & ( 1 << i ) ) 
		{
			subp->shift( tempdiff ) ;
			tempdiff[0] = tempdiff[1] = tempdiff[2] = 0 ;

			/* Pruning using intersection test */
			if ( subp->isIntersecting() )
			// if ( subp->getIntersectionMasks( cedgemask, edgemask ) )
			{
				if ( ! hasChild( node, i ) )
				{
					if ( height == 1 )
					{
						node = addLeafChild( node, i, count, createLeaf(0) ) ;
					}
					else
					{
						node = addInternalChild( node, i, count, createInternal(0) ) ;
					}
				}
				UCHAR* chd = getChild( node, count ) ;
						
				if ( ! isLeaf( node, i ) )
				{
					// setChild( node, count, addTrian ( chd, subp, height - 1, vertmask[i], edgemask ) ) ;
					setChild( node, count, addTrian ( chd, subp, height - 1 ) ) ;
				}
				else
				{
					setChild( node, count, updateCell( chd, subp ) ) ;
				}
			}
		}

		if ( hasChild( node, i ) )
		{
			count ++ ;
		}
	}

	delete subp ;
	return node ;
}

UCHAR* Octree::updateCell( UCHAR* node, Projections* p )
{
	int i ;

	// Edge connectivity
	int mask[3] = { 0, 4, 8	} ;
	int oldc = 0, newc = 0 ;
	float offs[3] ;
#ifdef USE_HERMIT
	float a[3], b[3], c[3] ;
#endif

	for ( i = 0 ; i < 3 ; i ++ )
	{
		if ( ! getEdgeParity( node, mask[i] ) )
		{
			if ( p->isIntersectingPrimary( i ) )
			{
				// this->actualQuads ++ ;
				setEdge( node, mask[i] ) ;
				offs[ newc ] = p->getIntersectionPrimary( i ) ;
#ifdef USE_HERMIT
				a[ newc ] = (float) p->inherit->norm[0] ;
				b[ newc ] = (float) p->inherit->norm[1] ;
				c[ newc ] = (float) p->inherit->norm[2] ;
#endif
				newc ++ ;
			}
		}
		else
		{
#ifndef USE_HERMIT
			offs[ newc ] = getEdgeOffset( node, oldc ) ;
#else
			offs[ newc ] = getEdgeOffsetNormal( node, oldc, a[ newc ], b[ newc ], c[ newc ] ) ;
#endif

//			if ( p->isIntersectingPrimary( i ) )
			{
				// dc_printf("Multiple intersections!\n") ;
				
//				setPatchEdge( node, i ) ;
			}
			
			oldc ++ ;
			newc ++ ;
		}
	}

	if ( newc > oldc )
	{
		// New offsets added, update this node
#ifndef USE_HERMIT
		node = updateEdgeOffsets( node, oldc, newc, offs ) ;
#else
		node = updateEdgeOffsetsNormals( node, oldc, newc, offs, a, b, c ) ;
#endif
	}



	return node ;
}

void Octree::preparePrimalEdgesMask( UCHAR* node )
{
	int count = 0 ;
	for ( int i = 0 ; i < 8 ; i ++ )
	{
		if ( hasChild( node, i ) )
		{
			if ( isLeaf( node, i ) )
			{
				createPrimalEdgesMask( getChild( node, count ) ) ;
			}
			else
			{
				preparePrimalEdgesMask( getChild( node, count ) ) ;
			}

			count ++ ;
		}
	}
}

void Octree::trace( )
{
	int st[3] = { 0, 0, 0, } ;
	this->numRings = 0 ;
	this->totRingLengths = 0 ;
	this->maxRingLength = 0 ;

	PathList* chdpath = NULL ;
	this->root = trace( this->root, st, dimen, maxDepth, chdpath ) ;

	if ( chdpath != NULL )
	{
		dc_printf("there are incomplete rings.\n") ;	
		printPaths( chdpath ) ;
	};
}

UCHAR* Octree::trace( UCHAR* node, int* st, int len, int depth, PathList*& paths)
{
	UCHAR* newnode = node ;
	len >>= 1 ;
	PathList* chdpaths[ 8 ] ;
	UCHAR* chd[ 8 ] ;
	int nst[ 8 ][ 3 ] ;
	int i, j ;

	// Get children paths
	int chdleaf[ 8 ] ;
	fillChildren( newnode, chd, chdleaf ) ;

	// int count = 0 ;
	for ( i = 0 ; i < 8 ; i ++ )
	{
		for ( j = 0 ; j < 3 ; j ++ )
		{
			nst[ i ][ j ] = st[ j ] + len * vertmap[ i ][ j ] ;
		}

		if ( chd[ i ] == NULL || isLeaf( node, i ) )
		{
			chdpaths[ i ] = NULL ;
		}
		else
		{
			trace( chd[ i ], nst[i], len, depth - 1, chdpaths[ i ] ) ;
		}
	}

	// Get connectors on the faces
	PathList* conn[ 12 ] ;
	UCHAR* nf[2] ;
	int lf[2] ;
	int df[2] = { depth - 1, depth - 1 } ;
	int* nstf[ 2 ];

	fillChildren( newnode, chd, chdleaf ) ;

	for ( i = 0 ; i < 12 ; i ++ )
	{
		int c[ 2 ] = { cellProcFaceMask[ i ][ 0 ], cellProcFaceMask[ i ][ 1 ] };
		
		for ( int j = 0 ; j < 2 ; j ++ )
		{
			lf[j] = chdleaf[ c[j] ] ;
			nf[j] = chd[ c[j] ] ;
			nstf[j] = nst[ c[j] ] ;
		}

		conn[ i ] = NULL ;
		
		findPaths( nf, lf, df, nstf, depth - 1, cellProcFaceMask[ i ][ 2 ], conn[ i ] ) ;

		//if ( conn[i] )
		//{
		//		printPath( conn[i] ) ;
		//}
	}
	
	// Connect paths
	PathList* rings = NULL ;
	combinePaths( chdpaths[0], chdpaths[1], conn[8], rings ) ;
	combinePaths( chdpaths[2], chdpaths[3], conn[9], rings ) ;
	combinePaths( chdpaths[4], chdpaths[5], conn[10], rings ) ;
	combinePaths( chdpaths[6], chdpaths[7], conn[11], rings ) ;

	combinePaths( chdpaths[0], chdpaths[2], conn[4], rings ) ;
	combinePaths( chdpaths[4], chdpaths[6], conn[5], rings ) ;
	combinePaths( chdpaths[0], NULL, conn[6], rings ) ;
	combinePaths( chdpaths[4], NULL, conn[7], rings ) ;

	combinePaths( chdpaths[0], chdpaths[4], conn[0], rings ) ;
	combinePaths( chdpaths[0], NULL, conn[1], rings ) ;
	combinePaths( chdpaths[0], NULL, conn[2], rings ) ;
	combinePaths( chdpaths[0], NULL, conn[3], rings ) ;

	// By now, only chdpaths[0] and rings have contents

	// Process rings
	if ( rings )
	{
		// printPath( rings ) ;

		/* Let's count first */
		PathList* trings = rings ;
		while ( trings )
		{
			this->numRings ++ ;
			this->totRingLengths += trings->length ;
			if ( trings->length > this->maxRingLength )
			{
				this->maxRingLength = trings->length ;
			}
			trings = trings->next ;
		}

		// printPath( rings ) ;
		newnode = patch( newnode, st, ( len << 1 ), rings ) ;
	}

	// Return incomplete paths
	paths = chdpaths[0] ;
	return newnode ;
}

void Octree::findPaths( UCHAR* node[2], int leaf[2], int depth[2], int* st[2], int maxdep, int dir, PathList*& paths )
{
	if ( ! ( node[0] && node[1] ) )
	{
		return ;
	}

	if ( ! ( leaf[0] && leaf[1] ) )
	{
		// Not at the bottom, recur

		// Fill children nodes
		int i, j ;
		UCHAR* chd[ 2 ][ 8 ] ;
		int chdleaf[ 2 ][ 8 ] ;
		int nst[ 2 ][ 8 ][ 3 ] ;

		for ( j = 0 ; j < 2 ; j ++ )
		{
			if ( ! leaf[j] )
			{
				fillChildren( node[j], chd[j], chdleaf[j] ) ;

				int len = ( dimen >> ( maxDepth - depth[j] + 1 ) ) ;
				for ( i = 0 ; i < 8 ; i ++ )
				{
					for ( int k = 0 ; k < 3 ; k ++ )
					{
						nst[ j ][ i ][ k ] = st[ j ][ k ] + len * vertmap[ i ][ k ] ;
					}
				}

			}
		}

		// 4 face calls
		UCHAR* nf[2] ;
		int df[2] ;
		int lf[2] ;
		int* nstf[2] ;
		for ( i = 0 ; i < 4 ; i ++ )
		{
			int c[2] = { faceProcFaceMask[ dir ][ i ][ 0 ], faceProcFaceMask[ dir ][ i ][ 1 ] };
			for ( int j = 0 ; j < 2 ; j ++ )
			{
				if ( leaf[j] )
				{
					lf[j] = leaf[j] ;
					nf[j] = node[j] ;
					df[j] = depth[j] ;
					nstf[j] = st[j] ;
				}
				else
				{
					lf[j] = chdleaf[ j ][ c[j] ] ;
					nf[j] = chd[ j ][ c[j] ] ;
					df[j] = depth[j] - 1 ;
					nstf[j] = nst[ j ][ c[j] ] ;
				}
			}
			findPaths( nf, lf, df, nstf, maxdep - 1, faceProcFaceMask[ dir ][ i ][ 2 ], paths ) ;
		}

	}
	else
	{
		// At the bottom, check this face
		int ind = ( depth[0] == maxdep ? 0 : 1 ) ;
		int fcind = 2 * dir + ( 1 - ind ) ;
		if ( getFaceParity( node[ ind ], fcind ) )
		{
			// Add into path
			PathElement* ele1 = new PathElement ;
			PathElement* ele2 = new PathElement ;

			ele1->pos[0] = st[0][0] ;
			ele1->pos[1] = st[0][1] ;
			ele1->pos[2] = st[0][2] ;

			ele2->pos[0] = st[1][0] ;
			ele2->pos[1] = st[1][1] ;
			ele2->pos[2] = st[1][2] ;

			ele1->next = ele2 ;
			ele2->next = NULL ;

			PathList* lst = new PathList ;
			lst->head = ele1 ;
			lst->tail = ele2 ;
			lst->length = 2 ;
			lst->next = paths ;
			paths = lst ;

			// int l = ( dimen >> maxDepth ) ;
		}
	}

}

void Octree::combinePaths( PathList*& list1, PathList* list2, PathList* paths, PathList*& rings )
{
	// Make new list of paths
	PathList* nlist = NULL ;

	// Search for each connectors in paths
	PathList* tpaths = paths ;
	PathList* tlist, * pre ;
	while ( tpaths )
	{
		PathList* singlist = tpaths ;
		PathList* templist ;
		tpaths = tpaths->next ;
		singlist->next = NULL ;

		// Look for hookup in list1
		tlist = list1 ;
		pre = NULL ;
		while ( tlist )
		{
			if (  (templist = combineSinglePath( list1, pre, tlist, singlist, NULL, singlist )) != NULL )
			{
				singlist = templist ;
				continue ;
			}
			pre = tlist ;
			tlist = tlist->next ;
		}

		// Look for hookup in list2
		tlist = list2 ;
		pre = NULL ;
		while ( tlist )
		{
			if (  (templist = combineSinglePath( list2, pre, tlist, singlist, NULL, singlist )) != NULL )
			{
				singlist = templist ;
				continue ;
			}
			pre = tlist ;
			tlist = tlist->next ;
		}

		// Look for hookup in nlist
		tlist = nlist ;
		pre = NULL ;
		while ( tlist )
		{
			if (  (templist = combineSinglePath( nlist, pre, tlist, singlist, NULL, singlist )) != NULL )
			{
				singlist = templist ;
				continue ;
			}
			pre = tlist ;
			tlist = tlist->next ;
		}

		// Add to nlist or rings
		if ( isEqual( singlist->head, singlist->tail ) )
		{
			PathElement* temp = singlist->head ;
			singlist->head = temp->next ;
			delete temp ;
			singlist->length -- ;
			singlist->tail->next = singlist->head ;

			singlist->next = rings ;
			rings = singlist ;
		}
		else
		{
			singlist->next = nlist ;
			nlist = singlist ;
		}

	}

	// Append list2 and nlist to the end of list1 
	tlist = list1 ;
	if ( tlist != NULL )
	{
		while ( tlist->next != NULL )
		{
			tlist = tlist->next ;
		}
		tlist->next = list2 ;
	}
	else
	{
		tlist = list2 ;
		list1 = list2 ;
	}

	if ( tlist != NULL )
	{
		while ( tlist->next != NULL )
		{
			tlist = tlist->next ;
		}
		tlist->next = nlist ;
	}
	else
	{
		tlist = nlist ;
		list1 = nlist ;
	}

}

PathList* Octree::combineSinglePath( PathList*& head1, PathList* pre1, PathList*& list1, PathList*& head2, PathList* pre2, PathList*& list2 )
{
	if ( isEqual( list1->head, list2->head ) || isEqual( list1->tail, list2->tail ) )
	{
		// Reverse the list
		if ( list1->length < list2->length )
		{
			// Reverse list1
			PathElement* prev = list1->head ;
			PathElement* next = prev->next ;
			prev->next = NULL ;
			while ( next != NULL )
			{
				PathElement* tnext = next->next ;
				next->next = prev ;

				prev = next ;
				next = tnext ;
			}

			list1->tail = list1->head ;
			list1->head = prev ;
		}
		else
		{
			// Reverse list2
			PathElement* prev = list2->head ;
			PathElement* next = prev->next ;
			prev->next = NULL ;
			while ( next != NULL )
			{
				PathElement* tnext = next->next ;
				next->next = prev ;

				prev = next ;
				next = tnext ;
			}

			list2->tail = list2->head ;
			list2->head = prev ;
		}
	}	
	
	if ( isEqual( list1->head, list2->tail ) )
	{

		// Easy case
		PathElement* temp = list1->head->next ;
		delete list1->head ;
		list2->tail->next = temp ;

		PathList* nlist = new PathList ;
		nlist->length = list1->length + list2->length - 1 ;
		nlist->head = list2->head ;
		nlist->tail = list1->tail ;
		nlist->next = NULL ;

		deletePath( head1, pre1, list1 ) ;
		deletePath( head2, pre2, list2 ) ;

		return nlist ;
	} 
	else if ( isEqual( list1->tail, list2->head ) )
	{
		// Easy case
		PathElement* temp = list2->head->next ;
		delete list2->head ;
		list1->tail->next = temp ;

		PathList* nlist = new PathList ;
		nlist->length = list1->length + list2->length - 1 ;
		nlist->head = list1->head ;
		nlist->tail = list2->tail ;
		nlist->next = NULL ;

		deletePath( head1, pre1, list1 ) ;
		deletePath( head2, pre2, list2 ) ;

		return nlist ;
	}

	return NULL ;
}

void Octree::deletePath( PathList*& head, PathList* pre, PathList*& curr )
{
	PathList* temp = curr ;
	curr = temp->next ;
	delete temp ;

	if ( pre == NULL )
	{
		head = curr ;
	}
	else
	{
		pre->next = curr ;
	}
}

void Octree::printElement( PathElement* ele )
{
	if ( ele != NULL )
	{
		dc_printf( " (%d %d %d)", ele->pos[0], ele->pos[1], ele->pos[2] ) ;
	}
}

void Octree::printPath( PathList* path )
{
	PathElement* n = path->head;
	int same = 0 ;

#if DC_DEBUG
	int len = ( dimen >> maxDepth ) ;
#endif
	while ( n && ( same == 0 || n != path->head ) )
	{
		same ++ ;
		dc_printf( " (%d %d %d)", n->pos[0] / len, n->pos[1] / len, n->pos[2] / len ) ;
		n = n->next ;
	}

	if ( n == path->head )
	{
		dc_printf(" Ring!\n") ;
	}
	else
	{
		dc_printf(" %p end!\n", n) ;
	}
}

void Octree::printPath( PathElement* path )
{
	PathElement *n = path;
	int same = 0 ;
#if DC_DEBUG
	int len = ( dimen >> maxDepth ) ; 
#endif
	while ( n && ( same == 0 || n != path ) )
	{
		same ++ ;
		dc_printf( " (%d %d %d)", n->pos[0] / len, n->pos[1] / len, n->pos[2] / len ) ;
		n = n->next ;
	}

	if ( n == path )
	{
		dc_printf(" Ring!\n") ;
	}
	else
	{
		dc_printf(" %p end!\n", n) ;
	}

}


void Octree::printPaths( PathList* path )
{
	PathList* iter = path ;
	int i = 0 ;
	while ( iter != NULL )
	{
		dc_printf("Path %d:\n", i) ;
		printPath( iter ) ;
		iter = iter->next ;
		i ++ ;
	}
}

UCHAR* Octree::patch( UCHAR* node, int st[3], int len, PathList* rings )
{
#ifdef IN_DEBUG_MODE
	dc_printf("Call to PATCH with rings: \n");
	printPaths( rings ) ;
#endif

	/* Do nothing but couting 
	PathList* tlist = rings ;
	PathList* ttlist ;
	PathElement* telem, * ttelem ;
	while ( tlist!= NULL )
	{
		// printPath( tlist ) ;
		this->numRings ++ ;
		this->totRingLengths += tlist->length ;
		if ( tlist->length > this->maxRingLength )
		{
			this->maxRingLength = tlist->length ;
		}
		ttlist = tlist ;
		tlist = tlist->next ;
	}
	return node ;
	*/
	

	/* Pass onto separate calls in each direction */
	UCHAR* newnode = node ;
	if ( len == mindimen )
	{
		dc_printf("Error! should have no list by now.\n") ;
		exit(0) ;
	}
	
	// YZ plane
	PathList* xlists[2] ;
	newnode = patchSplit( newnode, st, len, rings, 0, xlists[0], xlists[1] ) ;
	
	// XZ plane
	PathList* ylists[4] ;
	newnode = patchSplit( newnode, st, len, xlists[0], 1, ylists[0], ylists[1] ) ;
	newnode = patchSplit( newnode, st, len, xlists[1], 1, ylists[2], ylists[3] ) ;
	
	// XY plane
	PathList* zlists[8] ;
	newnode = patchSplit( newnode, st, len, ylists[0], 2, zlists[0], zlists[1] ) ;
	newnode = patchSplit( newnode, st, len, ylists[1], 2, zlists[2], zlists[3] ) ;
	newnode = patchSplit( newnode, st, len, ylists[2], 2, zlists[4], zlists[5] ) ;
	newnode = patchSplit( newnode, st, len, ylists[3], 2, zlists[6], zlists[7] ) ;
	
	// Recur
	len >>= 1 ;
	int count = 0 ;
	for ( int i = 0 ; i < 8 ; i ++ )
	{
		if ( zlists[i] != NULL )
		{
			int nori[3] = { 
				st[0] + len * vertmap[i][0] , 
				st[1] + len * vertmap[i][1] , 
				st[2] + len * vertmap[i][2] } ;
			patch( getChild( newnode , count ), nori, len, zlists[i] ) ;
		}

		if ( hasChild( newnode, i ) )
		{
			count ++ ;
		}
	}
#ifdef IN_DEBUG_MODE
	dc_printf("Return from PATCH\n") ;
#endif
	return newnode ;
	
}


UCHAR* Octree::patchSplit( UCHAR* node, int st[3], int len, PathList* rings, int dir, PathList*& nrings1, PathList*& nrings2 )
{
#ifdef IN_DEBUG_MODE
	dc_printf("Call to PATCHSPLIT with direction %d and rings: \n", dir);
	printPaths( rings ) ;
#endif

	UCHAR* newnode = node ;
	nrings1 = NULL ;
	nrings2 = NULL ;
	PathList* tmp ;
	while ( rings != NULL )
	{
		// Process this ring
		newnode = patchSplitSingle( newnode, st, len, rings->head, dir, nrings1, nrings2 ) ;
		
		// Delete this ring from the group
		tmp = rings ;
		rings = rings->next ;
		delete tmp ;
	}

#ifdef IN_DEBUG_MODE
	dc_printf("Return from PATCHSPLIT with \n");
	dc_printf("Rings gourp 1:\n") ;
	printPaths( nrings1 ) ;
	dc_printf("Rings group 2:\n") ;
	printPaths( nrings2 ) ;
#endif

	return newnode ;
}

UCHAR* Octree::patchSplitSingle( UCHAR* node, int st[3], int len, PathElement* head, int dir, PathList*& nrings1, PathList*& nrings2 )
{
#ifdef IN_DEBUG_MODE
	dc_printf("Call to PATCHSPLITSINGLE with direction %d and path: \n", dir );
	printPath( head ) ;
#endif

	UCHAR* newnode = node ;

	if ( head == NULL )
	{
#ifdef IN_DEBUG_MODE
		dc_printf("Return from PATCHSPLITSINGLE with head==NULL.\n") ;
#endif
		return newnode;
	}
	else
	{
		// printPath( head ) ;
	}
	
	// Walk along the ring to find pair of intersections
	PathElement* pre1 = NULL ;
	PathElement* pre2 = NULL ;
	int side = findPair ( head, st[ dir ] + len / 2 , dir, pre1, pre2 ) ;
	
	/*
	if ( pre1 == pre2 )
	{
		int edgelen = ( dimen >> maxDepth ) ;
		dc_printf("Location: %d %d %d Direction: %d Reso: %d\n", st[0]/edgelen, st[1]/edgelen, st[2]/edgelen, dir, len/edgelen) ;
		printPath( head ) ;
		exit( 0 ) ;
	}
	*/
	
	if ( side )
	{
		// Entirely on one side
		PathList* nring = new PathList( ) ;
		nring->head = head ;
		
		if ( side == -1 )
		{
			nring->next = nrings1 ;
			nrings1 = nring ;
		}
		else
		{
			nring->next = nrings2 ;
			nrings2 = nring ;
		}
	}
	else
	{
		// Break into two parts
		PathElement* nxt1 = pre1->next ;
		PathElement* nxt2 = pre2->next ;
		pre1->next = nxt2 ;
		pre2->next = nxt1 ;

		newnode = connectFace( newnode, st, len, dir, pre1, pre2 ) ;
	
		if ( isEqual( pre1, pre1->next ) )
		{
			if ( pre1 == pre1->next )
			{
				delete pre1 ;
				pre1 = NULL ;
			}
			else
			{
				PathElement* temp = pre1->next ;
				pre1->next = temp->next ;
				delete temp ;
			}
		}
		if ( isEqual( pre2, pre2->next ) )
		{
			if ( pre2 == pre2->next )
			{
				delete pre2 ;
				pre2 = NULL ;
			}
			else
			{
				PathElement* temp = pre2->next ;
				pre2->next = temp->next ;
				delete temp ;
			}
		}

		compressRing ( pre1 ) ;
		compressRing ( pre2 ) ;
		
		// Recur
		newnode = patchSplitSingle( newnode, st, len, pre1, dir, nrings1, nrings2 ) ;
		newnode = patchSplitSingle( newnode, st, len, pre2, dir, nrings1, nrings2 ) ;
		
	}

#ifdef IN_DEBUG_MODE
	dc_printf("Return from PATCHSPLITSINGLE with \n");
	dc_printf("Rings gourp 1:\n") ;
	printPaths( nrings1 ) ;
	dc_printf("Rings group 2:\n") ;
	printPaths( nrings2 ) ;
#endif

	return newnode ;
}

UCHAR* Octree::connectFace( UCHAR* node, int st[3], int len, int dir, PathElement* f1, PathElement* f2 )
{
#ifdef IN_DEBUG_MODE
	dc_printf("Call to CONNECTFACE with direction %d and length %d path: \n", dir, len );
	dc_printf("Path (low side): \n" ) ;
	printPath( f1 ) ;
//	checkPath( f1 ) ;
	dc_printf("Path (high side): \n" ) ;
	printPath( f2 ) ;
//	checkPath( f2 ) ;
#endif

	UCHAR* newnode = node ;

	// Setup 2D 
	int pos = st[ dir ] + len / 2 ;
	int xdir = ( dir + 1 ) % 3 ;
	int ydir = ( dir + 2 ) % 3 ;
	
	// Use existing intersections on f1 and f2
	int x1, y1, x2, y2 ;
	float p1, q1, p2, q2 ;

	getFacePoint( f2->next, dir, x1, y1, p1, q1 ) ;
	getFacePoint( f2, dir, x2, y2, p2, q2 ) ;
 
	float dx = x2 + p2 - x1 - p1 ;
	float dy = y2 + q2 - y1 - q1 ;
	
	// Do adapted Bresenham line drawing
	float rx = p1, ry = q1 ;
	int incx = 1, incy = 1 ; 
	int lx = x1, ly = y1 ;
	int hx = x2, hy = y2 ;
	int choice ;
	if ( x2 < x1 )
	{
		incx = -1 ;
		rx = 1 - rx ;
		lx = x2 ;
		hx = x1 ;
	}
	if ( y2 < y1 )
	{
		incy = -1 ;
		ry = 1 - ry ;
		ly = y2 ;
		hy = y1 ;
	}
	
	float sx = dx * incx ;
	float sy = dy * incy ;
	
	int ori[3] ;
	ori[ dir ] = pos / mindimen ;
	ori[ xdir ] = x1 ;
	ori[ ydir ] = y1 ;
	int walkdir ;
	int inc ;
	float alpha ;

	PathElement* curEleN = f1 ;
	PathElement* curEleP = f2->next ;
	UCHAR *nodeN = NULL, *nodeP = NULL ;
	UCHAR *curN = locateLeaf( newnode, len, f1->pos ) ;
	UCHAR *curP = locateLeaf( newnode, len, f2->next->pos ) ;
	if ( curN == NULL || curP == NULL )
	{
		exit(0) ;
	}
	int stN[3], stP[3] ;
	int lenN, lenP ;
	
	/* Unused code, leaving for posterity

	float stpt[3], edpt[3] ;
	stpt[ dir ] = edpt[ dir ] = (float) pos ;
	stpt[ xdir ] = ( x1 + p1 ) * mindimen ;
	stpt[ ydir ] = ( y1 + q1 ) * mindimen ;
	edpt[ xdir ] = ( x2 + p2 ) * mindimen ;
	edpt[ ydir ] = ( y2 + q2 ) * mindimen ;
	*/
	while( ori[ xdir ] != x2 || ori[ ydir ] != y2 )
	{
		int next ;
		if ( sy * (1 - rx) > sx * (1 - ry) )
		{
			choice = 1 ; 
			next = ori[ ydir ] + incy ;
			if ( next < ly || next > hy ) 
			{
				choice = 4 ;
				next = ori[ xdir ] + incx ;
			}
		}
		else
		{
			choice = 2 ;
			next = ori[ xdir ] + incx ;
			if ( next < lx || next > hx ) 
			{
				choice = 3 ;
				next = ori[ ydir ] + incy ;
			}
		}
		
		if ( choice & 1 )
		{
			ori[ ydir ] = next ;
			if ( choice == 1 )
			{
				rx += ( sy == 0 ? 0 : (1 - ry) * sx / sy  ) ; 
				ry = 0 ;
			}
			
			walkdir = 2 ;
			inc = incy ;
			alpha = x2 < x1 ? 1 - rx : rx ;
		}
		else
		{
			ori[ xdir ] = next ;
			if ( choice == 2 )
			{
				ry += ( sx == 0 ? 0 : (1 - rx) * sy / sx ) ;
				rx = 0 ;	
			}
			
			walkdir = 1 ;
			inc = incx ;
			alpha = y2 < y1 ? 1 - ry : ry ;
		}
		


		// Get the exact location of the marcher
		int nori[3] = { ori[0] * mindimen, ori[1] * mindimen, ori[2] * mindimen } ;
		float spt[3] = { (float) nori[0], (float) nori[1], (float) nori[2] } ;
		spt[ ( dir + ( 3 - walkdir ) ) % 3 ] += alpha * mindimen ;
		if ( inc < 0 )
		{
			spt[ ( dir + walkdir ) % 3 ] += mindimen ;
		}
		
		// dc_printf("new x,y: %d %d\n", ori[ xdir ] / edgelen, ori[ ydir ] / edgelen ) ;
		// dc_printf("nori: %d %d %d alpha: %f walkdir: %d\n", nori[0], nori[1], nori[2], alpha, walkdir ) ;
		// dc_printf("%f %f %f\n", spt[0], spt[1], spt[2] ) ;

		// Locate the current cells on both sides
		newnode = locateCell( newnode, st, len, nori, dir, 1, nodeN, stN, lenN ) ;
		newnode = locateCell( newnode, st, len, nori, dir, 0, nodeP, stP, lenP ) ;

		updateParent( newnode, len, st ) ;

		int flag = 0 ;
		// Add the cells to the rings and fill in the patch
		PathElement* newEleN ;
		if ( curEleN->pos[0] != stN[0] || curEleN->pos[1] != stN[1] || curEleN->pos[2] != stN[2] )
		{
			if ( curEleN->next->pos[0] != stN[0] || curEleN->next->pos[1] != stN[1] || curEleN->next->pos[2] != stN[2] )
			{
				newEleN = new PathElement ;
				newEleN->next = curEleN->next ;
				newEleN->pos[0] = stN[0] ;
				newEleN->pos[1] = stN[1] ;
				newEleN->pos[2] = stN[2] ;

				curEleN->next = newEleN ;
			}
			else
			{
				newEleN = curEleN->next ;
			}
			curN = patchAdjacent( newnode, len, curEleN->pos, curN, newEleN->pos, nodeN, walkdir, inc, dir, 1, alpha ) ;

			curEleN = newEleN ;
			flag ++ ;
		}

		PathElement* newEleP ;
		if ( curEleP->pos[0] != stP[0] || curEleP->pos[1] != stP[1] || curEleP->pos[2] != stP[2] )
		{
			if ( f2->pos[0] != stP[0] || f2->pos[1] != stP[1] || f2->pos[2] != stP[2] )
			{
				newEleP = new PathElement ;
				newEleP->next = curEleP ;
				newEleP->pos[0] = stP[0] ;
				newEleP->pos[1] = stP[1] ;
				newEleP->pos[2] = stP[2] ;

				f2->next = newEleP ;
			}
			else
			{
				newEleP = f2 ;
			}
			curP = patchAdjacent( newnode, len, curEleP->pos, curP, newEleP->pos, nodeP, walkdir, inc, dir, 0, alpha ) ;



			curEleP = newEleP ;
			flag ++ ;
		}

			
		/*
		if ( flag == 0 )
		{
			dc_printf("error: non-synchronized patching! at \n") ;
		}
		*/
	}

#ifdef IN_DEBUG_MODE
	dc_printf("Return from CONNECTFACE with \n");
	dc_printf("Path (low side):\n") ;
	printPath( f1 ) ;
	checkPath( f1 ) ;
	dc_printf("Path (high side):\n") ;
	printPath( f2 ) ;
	checkPath( f2 ) ;
#endif


	return newnode ;
}

UCHAR* Octree::patchAdjacent( UCHAR* node, int len, int st1[3], UCHAR* leaf1, int st2[3], UCHAR* leaf2, int walkdir, int inc, int dir, int side, float alpha )
{
#ifdef IN_DEBUG_MODE
	dc_printf("Before patching.\n") ;
	printInfo( st1 ) ;
	printInfo( st2 ) ;
	dc_printf("-----------------%d %d %d ; %d %d %d\n", st1[0], st2[1], st1[2], st2[0], st2[1], st2[2] ) ;
#endif

	// Get edge index on each leaf
	int edgedir = ( dir + ( 3 - walkdir ) ) % 3 ;
	int incdir = ( dir + walkdir ) % 3 ;
	int ind1 = ( edgedir == 1 ? ( dir + 3 - edgedir ) % 3 - 1 : 2 - ( dir + 3 - edgedir ) % 3 ) ;
	int ind2 = ( edgedir == 1 ? ( incdir + 3 - edgedir ) % 3 - 1 : 2 - ( incdir + 3 - edgedir ) % 3 ) ;

	int eind1 = ( ( edgedir << 2 ) | ( side << ind1 ) | ( ( inc > 0 ? 1 : 0 ) << ind2 ) ) ;
	int eind2 = ( ( edgedir << 2 ) | ( side << ind1 ) | ( ( inc > 0 ? 0 : 1 ) << ind2 ) ) ;

#ifdef IN_DEBUG_MODE
	dc_printf("Index 1: %d Alpha 1: %f Index 2: %d Alpha 2: %f\n", eind1, alpha, eind2, alpha ) ;
	/*
	if ( alpha < 0 || alpha > 1 )
	{
		dc_printf("Index 1: %d Alpha 1: %f Index 2: %d Alpha 2: %f\n", eind1, alpha, eind2, alpha ) ;
		printInfo( st1 ) ;
		printInfo( st2 ) ;
	}
	*/
#endif

	// Flip edge parity
	UCHAR* nleaf1 = flipEdge( leaf1, eind1, alpha ) ;
	UCHAR* nleaf2 = flipEdge( leaf2, eind2, alpha ) ;

	// Update parent link
	updateParent( node, len, st1, nleaf1 ) ;
	updateParent( node, len, st2, nleaf2 ) ;
	// updateParent( nleaf1, mindimen, st1 ) ;
	// updateParent( nleaf2, mindimen, st2 ) ;

	/*
	float m[3] ;
	dc_printf("Adding new point: %f %f %f\n", spt[0], spt[1], spt[2] ) ;
	getMinimizer( leaf1, m ) ;
	dc_printf("Cell %d now has minimizer %f %f %f\n", leaf1, m[0], m[1], m[2] ) ;
	getMinimizer( leaf2, m ) ;
	dc_printf("Cell %d now has minimizer %f %f %f\n", leaf2, m[0], m[1], m[2] ) ;
	*/		

#ifdef IN_DEBUG_MODE
	dc_printf("After patching.\n") ;
	printInfo( st1 ) ;
	printInfo( st2 ) ;
#endif
	return nleaf2 ;
}

UCHAR* Octree::locateCell( UCHAR* node, int st[3], int len, int ori[3], int dir, int side, UCHAR*& rleaf, int rst[3], int& rlen )
{
#ifdef IN_DEBUG_MODE
	// dc_printf("Call to LOCATECELL with node ") ;
	// printNode( node ) ;
#endif
	UCHAR* newnode = node ;
	int i ;
	len >>= 1 ;
	int ind = 0 ;
	for ( i = 0 ; i < 3 ; i ++ )
	{
		ind <<= 1 ;
		if ( i == dir && side == 1 )
		{
			ind |= ( ori[ i ] <= ( st[ i ] + len ) ? 0 : 1 ) ;
		}
		else
		{
			ind |= ( ori[ i ] < ( st[ i ] + len ) ? 0 : 1 ) ;
		}
	}

#ifdef IN_DEBUG_MODE
	// dc_printf("In LOCATECELL index of ori (%d %d %d) with dir %d side %d in st (%d %d %d, %d) is: %d\n",
	//	ori[0], ori[1], ori[2], dir, side, st[0], st[1], st[2], len, ind ) ;
#endif

	rst[0] = st[0] + vertmap[ ind ][ 0 ] * len ;
	rst[1] = st[1] + vertmap[ ind ][ 1 ] * len ;
	rst[2] = st[2] + vertmap[ ind ][ 2 ] * len ;
	
	if ( hasChild( newnode, ind ) )
	{
		int count = getChildCount( newnode, ind ) ;
		UCHAR* chd = getChild( newnode, count ) ;
		if ( isLeaf( newnode, ind ) )
		{
			rleaf = chd ;
			rlen = len ;
		}
		else
		{
			// Recur
			setChild( newnode, count, locateCell( chd, rst, len, ori, dir, side, rleaf, rst, rlen ) ) ;
		}
	}
	else
	{
		// Create a new child here
		if ( len == this->mindimen )
		{
			UCHAR* chd = createLeaf( 0 ) ;
			newnode = addChild( newnode, ind, chd, 1 ) ;
			rleaf = chd ;
			rlen = len ;
		}
		else
		{
			// Subdivide the empty cube
			UCHAR* chd = createInternal( 0 ) ;
			newnode = addChild( newnode, ind, locateCell( chd, rst, len, ori, dir, side, rleaf, rst, rlen ), 0 ) ;
		}
	}
	
#ifdef IN_DEBUG_MODE
	// dc_printf("Return from LOCATECELL with node ") ;
	// printNode( newnode ) ;
#endif
	return newnode ;
}

void Octree::checkElement( PathElement* ele )
{
	/*
	if ( ele != NULL && locateLeafCheck( ele->pos ) != ele->node )
	{
		dc_printf("Screwed! at pos: %d %d %d\n", ele->pos[0]>>minshift, ele->pos[1]>>minshift, ele->pos[2]>>minshift);
		exit( 0 ) ;
	}
	*/
}

void Octree::checkPath( PathElement* path )
{
	PathElement *n = path ;
	int same = 0 ;
	while ( n && ( same == 0 || n != path ) )
	{
		same ++ ;
		checkElement( n ) ;
		n = n->next ;
	}

}

void Octree::testFacePoint( PathElement* e1, PathElement* e2 )
{
	int i ;
	PathElement * e = NULL ;
	for ( i = 0 ; i < 3 ; i ++ )
	{
		if ( e1->pos[i] != e2->pos[i] )
		{
			if ( e1->pos[i] < e2->pos[i] )
			{
				e = e2 ;
			}
			else
			{
				e = e1 ;
			}
			break ;
		}
	}

	int x, y ;
	float p, q ;
	dc_printf("Test.") ;
	getFacePoint( e, i, x, y, p, q ) ;
}

void Octree::getFacePoint( PathElement* leaf, int dir, int& x, int& y, float& p, float& q )
{
	// Find average intersections
	float avg[3] = {0, 0, 0} ;
	float off[3] ;
	int num = 0, num2 = 0 ;

	UCHAR* leafnode = locateLeaf( leaf->pos ) ;
	for ( int i = 0 ; i < 4 ; i ++ )
	{
		int edgeind = faceMap[ dir * 2 ][ i ] ;
		int nst[3] ;
		for ( int j = 0 ; j < 3 ; j ++ )
		{
			nst[j] = leaf->pos[j] + mindimen * vertmap[ edgemap[ edgeind][ 0 ] ][ j ] ;
		}

		if ( getEdgeIntersectionByIndex( nst, edgeind / 4, off, 1 ) )
		{
			avg[0] += off[0] ;
			avg[1] += off[1] ;
			avg[2] += off[2] ;
			num ++ ;
		}
		if ( getEdgeParity( leafnode, edgeind ) )
		{
			num2 ++ ;
		}
	}
	if ( num == 0 )
	{
 		dc_printf("Wrong! dir: %d pos: %d %d %d num: %d\n", dir, leaf->pos[0]>>minshift, leaf->pos[1]>>minshift, leaf->pos[2]>>minshift, num2);
		avg[0] = (float) leaf->pos[0] ;
		avg[1] = (float) leaf->pos[1] ;
		avg[2] = (float) leaf->pos[2] ;
	}
	else
	{
		
		avg[0] /= num ;
		avg[1] /= num ;
		avg[2] /= num ;
		
		//avg[0] = (float) leaf->pos[0];
		//avg[1] = (float) leaf->pos[1];
		//avg[2] = (float) leaf->pos[2];
	}
	
	int xdir = ( dir + 1 ) % 3 ;
	int ydir = ( dir + 2 ) % 3 ;

	float xf = avg[ xdir ] ;
	float yf = avg[ ydir ] ;

#ifdef IN_DEBUG_MODE
	// Is it outside?
	// PathElement* leaf = leaf1->len < leaf2->len ? leaf1 : leaf2 ;
	/*
	float* m = ( leaf == leaf1 ? m1 : m2 ) ;
	if ( xf < leaf->pos[ xdir ] || 
		 yf < leaf->pos[ ydir ] ||
		 xf > leaf->pos[ xdir ] + leaf->len ||
		 yf > leaf->pos[ ydir ] + leaf->len)
	{
		dc_printf("Outside cube (%d %d %d), %d : %d %d %f %f\n", leaf->pos[ 0 ], leaf->pos[1], leaf->pos[2], leaf->len, 
						pos, dir, xf, yf) ;

		// For now, snap to cell
		xf = m[ xdir ] ;
		yf = m[ ydir ] ;
	}
	*/

	/*
	if ( alpha < 0 || alpha > 1 ||
		 xf < leaf->pos[xdir] || xf > leaf->pos[xdir] + leaf->len || 
		 yf < leaf->pos[ydir] || yf > leaf->pos[ydir] + leaf->len )
	{
		dc_printf("Alpha: %f Address: %d and %d\n", alpha, leaf1->node, leaf2->node ) ;
		dc_printf("GETFACEPOINT result: (%d %d %d) %d min: (%f %f %f) ;(%d %d %d) %d min: (%f %f %f).\n",
			leaf1->pos[0], leaf1->pos[1], leaf1->pos[2], leaf1->len, m1[0], m1[1], m1[2], 
			leaf2->pos[0], leaf2->pos[1], leaf2->pos[2], leaf2->len, m2[0], m2[1], m2[2]);
		dc_printf("Face point at dir %d pos %d: %f %f\n", dir, pos, xf, yf ) ;
	}
	*/
#endif
	

	// Get the integer and float part
	x = ( ( leaf->pos[ xdir ] ) >> minshift ) ;
	y = ( ( leaf->pos[ ydir ] ) >> minshift ) ;

	p = ( xf - leaf->pos[ xdir ] ) / mindimen ;
	q = ( yf - leaf->pos[ ydir ] ) / mindimen ;


#ifdef IN_DEBUG_MODE
	dc_printf("Face point at dir %d : %f %f\n", dir, xf, yf ) ;
#endif
}

int Octree::findPair( PathElement* head, int pos, int dir, PathElement*& pre1, PathElement*& pre2 )
{
	int side = getSide ( head, pos, dir ) ;
	PathElement* cur = head ;
	PathElement* anchor ;
	PathElement* ppre1, *ppre2 ;
	
	// Start from this face, find a pair
	anchor = cur ;
	ppre1 = cur ;
	cur = cur->next ;
	while ( cur != anchor && ( getSide( cur, pos, dir ) == side ) )
	{
		ppre1 = cur ;
		cur = cur->next ;
	}
	if ( cur == anchor )
	{
		// No pair found
		return side ;
	}
	
	side = getSide( cur, pos, dir ) ;
	ppre2 = cur ;
	cur = cur->next ;
	while ( getSide( cur, pos, dir ) == side )
	{
		ppre2 = cur ;
		cur = cur->next ;
	}
	
	
	// Switch pre1 and pre2 if we start from the higher side
	if ( side == -1 )
	{
		cur = ppre1 ;
		ppre1 = ppre2 ;
		ppre2 = cur ;
	}

	pre1 = ppre1 ;
	pre2 = ppre2 ;
	
	return 0 ;
}

int Octree::getSide( PathElement* e, int pos, int dir )
{
	return ( e->pos[ dir ] < pos ? -1 : 1 ) ;
}

int Octree::isEqual( PathElement* e1, PathElement* e2 )
{
	return ( e1->pos[0] == e2->pos[0] && e1->pos[1] == e2->pos[1] && e1->pos[2] == e2->pos[2] ) ;
}

void Octree::compressRing( PathElement*& ring )
{
	if ( ring == NULL )
	{
		return ;
	}
#ifdef IN_DEBUG_MODE
	dc_printf("Call to COMPRESSRING with path: \n" );
	printPath( ring ) ;
#endif

	PathElement* cur = ring->next->next ;
	PathElement* pre = ring->next ;
	PathElement* prepre = ring ;
	PathElement* anchor = prepre ;
	
	do
	{
		while ( isEqual( cur, prepre ) )
		{
			// Delete
			if ( cur == prepre )
			{
				// The ring has shrinked to a point
				delete pre ;
				delete cur ;
				anchor = NULL ;
				break ;
			}
			else
			{
				prepre->next = cur->next ;
				delete pre ;
				delete cur ;
				pre = prepre->next ;
				cur = pre->next ;
				anchor = prepre ;
			}
		}
		
		if ( anchor == NULL )
		{
			break ;
		}
		
		prepre = pre ;
		pre = cur ;
		cur = cur->next ;
	} while ( prepre != anchor ) ;
	
	ring = anchor ;

#ifdef IN_DEBUG_MODE
	dc_printf("Return from COMPRESSRING with path: \n" );
	printPath( ring ) ;
#endif
}

void Octree::buildSigns( )
{
	// First build a lookup table
	// dc_printf("Building up look up table...\n") ;
	int size = 1 << 12 ;
	unsigned char table[ 1 << 12 ] ;
	for ( int i = 0 ; i < size ; i ++ )
	{
		table[i] = 0 ;
	}
	for ( int i = 0 ; i < 256 ; i ++ )
	{
		int ind = 0 ;
		for ( int j = 11 ; j >= 0 ; j -- )
		{
			ind <<= 1 ;
			if ( ( ( i >> edgemap[j][0] ) & 1 ) ^ ( ( i >> edgemap[j][1] ) & 1 ) )
			{
				ind |= 1 ;
			}
		}

		table[ ind ] = i ;
	}

	// Next, traverse the grid
	int sg = 1 ;
	int cube[8] ;
	buildSigns( table, this->root, 0, sg, cube ) ;
}

void Octree::buildSigns( unsigned char table[], UCHAR* node, int isLeaf, int sg, int rvalue[8] )
{
	if ( node == NULL )
	{
		for ( int i = 0 ; i < 8 ; i ++ )
		{
			rvalue[i] = sg ;
		}
		return ;
	}

	if ( isLeaf == 0 )
	{
		// Internal node
		UCHAR* chd[8] ;
		int leaf[8] ;
		fillChildren( node, chd, leaf ) ;

		// Get the signs at the corners of the first cube
		rvalue[0] = sg ;
		int oris[8] ;
		buildSigns( table, chd[0], leaf[0], sg, oris ) ;

		// Get the rest
		int cube[8] ;
		for ( int i = 1 ; i < 8 ; i ++ )
		{
			buildSigns( table, chd[i], leaf[i], oris[i], cube ) ;
			rvalue[i] = cube[i] ;
		}

	}
	else
	{
		// Leaf node
		generateSigns( node, table, sg ) ;

		for ( int i = 0 ; i < 8 ; i ++ )
		{
			rvalue[i] = getSign( node, i ) ;
		}
	}
}

void Octree::floodFill( )
{
	// int threshold = (int) ((dimen/mindimen) * (dimen/mindimen) * 0.5f) ;
	int st[3] = { 0, 0, 0 } ;

	// First, check for largest component
	// size stored in -threshold
	this->clearProcessBits( root, maxDepth ) ;
	int threshold = this->floodFill( root, st, dimen, maxDepth, 0 ) ;

	// Next remove
	dc_printf("Largest component: %d\n", threshold);
	threshold *= thresh ;
	dc_printf("Removing all components smaller than %d\n", threshold) ;

	int st2[3] = { 0, 0, 0 } ;
	this->clearProcessBits( root, maxDepth ) ;
	this->floodFill( root, st2, dimen, maxDepth, threshold ) ;

}

void Octree::clearProcessBits( UCHAR* node, int height )
{
	int i;

	if ( height == 0 )
	{
		// Leaf cell, 
		for ( i = 0 ; i < 12 ; i ++ )
		{
			setOutProcess( node, i ) ;
		}
	}
	else
	{
		// Internal cell, recur
		int count = 0 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				clearProcessBits( getChild( node, count ), height - 1 ) ;
				count ++ ;
			}
		}
	}	
}

/*
void Octree::floodFill( UCHAR* node, int st[3], int len, int height, int threshold )
{
	int i, j;

	if ( height == 0 )
	{
		// Leaf cell, 
		int par, inp ;

		// Test if the leaf has intersection edges
		for ( i = 0 ; i < 12 ; i ++ )
		{
			par = getEdgeParity( node, i ) ;
			inp = isInProcess( node, i ) ;

			if ( par == 1 && inp == 0 )
			{
				// Intersection edge, hasn't been processed
				// Let's start filling
				GridQueue* queue = new GridQueue() ;
				int total = 1 ;

				// Set to in process
				int mst[3] ;
				mst[0] = st[0] + vertmap[edgemap[i][0]][0] * len ;
				mst[1] = st[1] + vertmap[edgemap[i][0]][1] * len ;
				mst[2] = st[2] + vertmap[edgemap[i][0]][2] * len;
				int mdir = i / 4 ;
				setInProcessAll( mst, mdir ) ;

				// Put this edge into queue
				queue->pushQueue( mst, mdir ) ;

				// Queue processing
				int nst[3], dir ;
				while ( queue->popQueue( nst, dir ) == 1 )
				{
					// dc_printf("nst: %d %d %d, dir: %d\n", nst[0]/mindimen, nst[1]/mindimen, nst[2]/mindimen, dir) ;
					// locations
					int stMask[3][3] = {
						{ 0, 0 - len, 0 - len },
						{ 0 - len, 0, 0 - len },
						{ 0 - len, 0 - len, 0 }
					};
					int cst[2][3] ;
					for ( j = 0 ; j < 3 ; j ++ )
					{
						cst[0][j] = nst[j] ;
						cst[1][j] = nst[j] + stMask[ dir ][ j ] ;
					}

					// cells 
					UCHAR* cs[2] ;
					for ( j = 0 ; j < 2 ; j ++ )
					{
						cs[ j ] = locateLeaf( cst[j] ) ;
					}

					// Middle sign
					int s = getSign( cs[0], 0 ) ;

					// Masks
					int fcCells[4] = {1,0,1,0};
					int fcEdges[3][4][3] = {
						{{9,2,11},{8,1,10},{5,1,7},{4,2,6}},
						{{10,6,11},{8,5,9},{1,5,3},{0,6,2}},
						{{6,10,7},{4,9,5},{2,9,3},{0,10,1}}
					};

					// Search for neighboring connected intersection edges
					for ( int find = 0 ; find < 4 ; find ++ )
					{
						int cind = fcCells[find] ;
						int eind, edge ;
						if ( s == 0 )
						{
							// Original order
							for ( eind = 0 ; eind < 3 ; eind ++ )
							{
								edge = fcEdges[dir][find][eind] ;
								if ( getEdgeParity( cs[cind], edge ) == 1 )
								{
									break ;
								}
							}
						}
						else
						{
							// Inverse order
							for ( eind = 2 ; eind >= 0 ; eind -- )
							{
								edge = fcEdges[dir][find][eind] ;
								if ( getEdgeParity( cs[cind], edge ) == 1 )
								{
									break ;
								}
							}
						}
						
						if ( eind == 3 || eind == -1 )
						{
							dc_printf("Wrong! this is not a consistent sign. %d\n", eind );
						}
						else 
						{
							int est[3] ;
							est[0] = cst[cind][0] + vertmap[edgemap[edge][0]][0] * len ;
							est[1] = cst[cind][1] + vertmap[edgemap[edge][0]][1] * len ;
							est[2] = cst[cind][2] + vertmap[edgemap[edge][0]][2] * len ;
							int edir = edge / 4 ;
							
							if ( isInProcess( cs[cind], edge ) == 0 )
							{
								setInProcessAll( est, edir ) ;
								queue->pushQueue( est, edir ) ;
								// dc_printf("Pushed: est: %d %d %d, edir: %d\n", est[0]/len, est[1]/len, est[2]/len, edir) ;
								total ++ ;
							}
							else
							{
								// dc_printf("Processed, not pushed: est: %d %d %d, edir: %d\n", est[0]/len, est[1]/len, est[2]/len, edir) ;
							}
						}
						
					}

				}

				dc_printf("Size of component: %d ", total) ;

				if ( total > threshold )
				{
					dc_printf("Maintained.\n") ;
					continue ;
				}
				dc_printf("Less then %d, removing...\n", threshold) ;

				// We have to remove this noise

				// Flip parity
				// setOutProcessAll( mst, mdir ) ;
				flipParityAll( mst, mdir ) ;

				// Put this edge into queue
				queue->pushQueue( mst, mdir ) ;

				// Queue processing
				while ( queue->popQueue( nst, dir ) == 1 )
				{
					// dc_printf("nst: %d %d %d, dir: %d\n", nst[0]/mindimen, nst[1]/mindimen, nst[2]/mindimen, dir) ;
					// locations
					int stMask[3][3] = {
						{ 0, 0 - len, 0 - len },
						{ 0 - len, 0, 0 - len },
						{ 0 - len, 0 - len, 0 }
					};
					int cst[2][3] ;
					for ( j = 0 ; j < 3 ; j ++ )
					{
						cst[0][j] = nst[j] ;
						cst[1][j] = nst[j] + stMask[ dir ][ j ] ;
					}

					// cells 
					UCHAR* cs[2] ;
					for ( j = 0 ; j < 2 ; j ++ )
					{
						cs[ j ] = locateLeaf( cst[j] ) ;
					}

					// Middle sign
					int s = getSign( cs[0], 0 ) ;

					// Masks
					int fcCells[4] = {1,0,1,0};
					int fcEdges[3][4][3] = {
						{{9,2,11},{8,1,10},{5,1,7},{4,2,6}},
						{{10,6,11},{8,5,9},{1,5,3},{0,6,2}},
						{{6,10,7},{4,9,5},{2,9,3},{0,10,1}}
					};

					// Search for neighboring connected intersection edges
					for ( int find = 0 ; find < 4 ; find ++ )
					{
						int cind = fcCells[find] ;
						int eind, edge ;
						if ( s == 0 )
						{
							// Original order
							for ( eind = 0 ; eind < 3 ; eind ++ )
							{
								edge = fcEdges[dir][find][eind] ;
								if ( isInProcess( cs[cind], edge ) == 1 )
								{
									break ;
								}
							}
						}
						else
						{
							// Inverse order
							for ( eind = 2 ; eind >= 0 ; eind -- )
							{
								edge = fcEdges[dir][find][eind] ;
								if ( isInProcess( cs[cind], edge ) == 1 )
								{
									break ;
								}
							}
						}
						
						if ( eind == 3 || eind == -1 )
						{
							dc_printf("Wrong! this is not a consistent sign. %d\n", eind );
						}
						else 
						{
							int est[3] ;
							est[0] = cst[cind][0] + vertmap[edgemap[edge][0]][0] * len ;
							est[1] = cst[cind][1] + vertmap[edgemap[edge][0]][1] * len ;
							est[2] = cst[cind][2] + vertmap[edgemap[edge][0]][2] * len ;
							int edir = edge / 4 ;
							
							if ( getEdgeParity( cs[cind], edge ) == 1 )
							{
								flipParityAll( est, edir ) ;
								queue->pushQueue( est, edir ) ;
								// dc_printf("Pushed: est: %d %d %d, edir: %d\n", est[0]/len, est[1]/len, est[2]/len, edir) ;
								total ++ ;
							}
							else
							{
								// dc_printf("Processed, not pushed: est: %d %d %d, edir: %d\n", est[0]/len, est[1]/len, est[2]/len, edir) ;
							}
						}
						
					}

				}

			}
		}
	}
	else
	{
		// Internal cell, recur
		int count = 0 ;
		len >>= 1 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				int nst[3] ;
				nst[0] = st[0] + vertmap[i][0] * len ;
				nst[1] = st[1] + vertmap[i][1] * len ;
				nst[2] = st[2] + vertmap[i][2] * len ;
				
				floodFill( getChild( node, count ), nst, len, height - 1, threshold ) ;
				count ++ ;
			}
		}
	}
}
*/

int Octree::floodFill( UCHAR* node, int st[3], int len, int height, int threshold )
{
	int i, j;
	int maxtotal = 0 ;

	if ( height == 0 )
	{
		// Leaf cell, 
		int par, inp ;

		// Test if the leaf has intersection edges
		for ( i = 0 ; i < 12 ; i ++ )
		{
			par = getEdgeParity( node, i ) ;
			inp = isInProcess( node, i ) ;

			if ( par == 1 && inp == 0 )
			{
				// Intersection edge, hasn't been processed
				// Let's start filling
				GridQueue* queue = new GridQueue() ;
				int total = 1 ;

				// Set to in process
				int mst[3] ;
				mst[0] = st[0] + vertmap[edgemap[i][0]][0] * len ;
				mst[1] = st[1] + vertmap[edgemap[i][0]][1] * len ;
				mst[2] = st[2] + vertmap[edgemap[i][0]][2] * len;
				int mdir = i / 4 ;
				setInProcessAll( mst, mdir ) ;

				// Put this edge into queue
				queue->pushQueue( mst, mdir ) ;

				// Queue processing
				int nst[3], dir ;
				while ( queue->popQueue( nst, dir ) == 1 )
				{
					// dc_printf("nst: %d %d %d, dir: %d\n", nst[0]/mindimen, nst[1]/mindimen, nst[2]/mindimen, dir) ;
					// locations
					int stMask[3][3] = {
						{ 0, 0 - len, 0 - len },
						{ 0 - len, 0, 0 - len },
						{ 0 - len, 0 - len, 0 }
					};
					int cst[2][3] ;
					for ( j = 0 ; j < 3 ; j ++ )
					{
						cst[0][j] = nst[j] ;
						cst[1][j] = nst[j] + stMask[ dir ][ j ] ;
					}

					// cells 
					UCHAR* cs[2] ;
					for ( j = 0 ; j < 2 ; j ++ )
					{
						cs[ j ] = locateLeaf( cst[j] ) ;
					}

					// Middle sign
					int s = getSign( cs[0], 0 ) ;

					// Masks
					int fcCells[4] = {1,0,1,0};
					int fcEdges[3][4][3] = {
						{{9,2,11},{8,1,10},{5,1,7},{4,2,6}},
						{{10,6,11},{8,5,9},{1,5,3},{0,6,2}},
						{{6,10,7},{4,9,5},{2,9,3},{0,10,1}}
					};

					// Search for neighboring connected intersection edges
					for ( int find = 0 ; find < 4 ; find ++ )
					{
						int cind = fcCells[find] ;
						int eind, edge ;
						if ( s == 0 )
						{
							// Original order
							for ( eind = 0 ; eind < 3 ; eind ++ )
							{
								edge = fcEdges[dir][find][eind] ;
								if ( getEdgeParity( cs[cind], edge ) == 1 )
								{
									break ;
								}
							}
						}
						else
						{
							// Inverse order
							for ( eind = 2 ; eind >= 0 ; eind -- )
							{
								edge = fcEdges[dir][find][eind] ;
								if ( getEdgeParity( cs[cind], edge ) == 1 )
								{
									break ;
								}
							}
						}
						
						if ( eind == 3 || eind == -1 )
						{
							dc_printf("Wrong! this is not a consistent sign. %d\n", eind );
						}
						else 
						{
							int est[3] ;
							est[0] = cst[cind][0] + vertmap[edgemap[edge][0]][0] * len ;
							est[1] = cst[cind][1] + vertmap[edgemap[edge][0]][1] * len ;
							est[2] = cst[cind][2] + vertmap[edgemap[edge][0]][2] * len ;
							int edir = edge / 4 ;
							
							if ( isInProcess( cs[cind], edge ) == 0 )
							{
								setInProcessAll( est, edir ) ;
								queue->pushQueue( est, edir ) ;
								// dc_printf("Pushed: est: %d %d %d, edir: %d\n", est[0]/len, est[1]/len, est[2]/len, edir) ;
								total ++ ;
							}
							else
							{
								// dc_printf("Processed, not pushed: est: %d %d %d, edir: %d\n", est[0]/len, est[1]/len, est[2]/len, edir) ;
							}
						}
						
					}

				}

				dc_printf("Size of component: %d ", total) ;

				if ( threshold == 0 )
				{
					// Measuring stage
					if ( total > maxtotal )
					{
						maxtotal = total ;
					}
					dc_printf(".\n") ;
					continue ;
				}
				
				if ( total >= threshold )
				{
					dc_printf("Maintained.\n") ;
					continue ;
				}
				dc_printf("Less then %d, removing...\n", threshold) ;

				// We have to remove this noise

				// Flip parity
				// setOutProcessAll( mst, mdir ) ;
				flipParityAll( mst, mdir ) ;

				// Put this edge into queue
				queue->pushQueue( mst, mdir ) ;

				// Queue processing
				while ( queue->popQueue( nst, dir ) == 1 )
				{
					// dc_printf("nst: %d %d %d, dir: %d\n", nst[0]/mindimen, nst[1]/mindimen, nst[2]/mindimen, dir) ;
					// locations
					int stMask[3][3] = {
						{ 0, 0 - len, 0 - len },
						{ 0 - len, 0, 0 - len },
						{ 0 - len, 0 - len, 0 }
					};
					int cst[2][3] ;
					for ( j = 0 ; j < 3 ; j ++ )
					{
						cst[0][j] = nst[j] ;
						cst[1][j] = nst[j] + stMask[ dir ][ j ] ;
					}

					// cells 
					UCHAR* cs[2] ;
					for ( j = 0 ; j < 2 ; j ++ )
					{
						cs[ j ] = locateLeaf( cst[j] ) ;
					}

					// Middle sign
					int s = getSign( cs[0], 0 ) ;

					// Masks
					int fcCells[4] = {1,0,1,0};
					int fcEdges[3][4][3] = {
						{{9,2,11},{8,1,10},{5,1,7},{4,2,6}},
						{{10,6,11},{8,5,9},{1,5,3},{0,6,2}},
						{{6,10,7},{4,9,5},{2,9,3},{0,10,1}}
					};

					// Search for neighboring connected intersection edges
					for ( int find = 0 ; find < 4 ; find ++ )
					{
						int cind = fcCells[find] ;
						int eind, edge ;
						if ( s == 0 )
						{
							// Original order
							for ( eind = 0 ; eind < 3 ; eind ++ )
							{
								edge = fcEdges[dir][find][eind] ;
								if ( isInProcess( cs[cind], edge ) == 1 )
								{
									break ;
								}
							}
						}
						else
						{
							// Inverse order
							for ( eind = 2 ; eind >= 0 ; eind -- )
							{
								edge = fcEdges[dir][find][eind] ;
								if ( isInProcess( cs[cind], edge ) == 1 )
								{
									break ;
								}
							}
						}
						
						if ( eind == 3 || eind == -1 )
						{
							dc_printf("Wrong! this is not a consistent sign. %d\n", eind );
						}
						else 
						{
							int est[3] ;
							est[0] = cst[cind][0] + vertmap[edgemap[edge][0]][0] * len ;
							est[1] = cst[cind][1] + vertmap[edgemap[edge][0]][1] * len ;
							est[2] = cst[cind][2] + vertmap[edgemap[edge][0]][2] * len ;
							int edir = edge / 4 ;
							
							if ( getEdgeParity( cs[cind], edge ) == 1 )
							{
								flipParityAll( est, edir ) ;
								queue->pushQueue( est, edir ) ;
								// dc_printf("Pushed: est: %d %d %d, edir: %d\n", est[0]/len, est[1]/len, est[2]/len, edir) ;
								total ++ ;
							}
							else
							{
								// dc_printf("Processed, not pushed: est: %d %d %d, edir: %d\n", est[0]/len, est[1]/len, est[2]/len, edir) ;
							}
						}
						
					}

				}

			}
		}

	}
	else
	{
		// Internal cell, recur
		int count = 0 ;
		len >>= 1 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				int nst[3] ;
				nst[0] = st[0] + vertmap[i][0] * len ;
				nst[1] = st[1] + vertmap[i][1] * len ;
				nst[2] = st[2] + vertmap[i][2] * len ;
				
				int d = floodFill( getChild( node, count ), nst, len, height - 1, threshold ) ;
				if ( d > maxtotal)
				{
					maxtotal = d ;
				}
				count ++ ;
			}
		}
	}


	return maxtotal ;

}

void Octree::writeOut()
{
	int numQuads = 0 ;
	int numVertices = 0 ;
	int numEdges = 0 ;
#ifdef USE_HERMIT
	countIntersection( root, maxDepth, numQuads, numVertices, numEdges ) ;
#else
	countIntersection( root, maxDepth, numQuads, numVertices ) ;
	numEdges = numQuads * 3 / 2 ;
#endif
	dc_printf("Vertices counted: %d Polys counted: %d \n", numVertices, numQuads ) ;
	output_mesh = alloc_output(numVertices, numQuads);	
	int offset = 0 ;
	int st[3] = { 0, 0, 0 } ;

	// First, output vertices
	offset = 0 ;
	actualVerts = 0 ;
	actualQuads = 0 ;
#ifdef USE_HERMIT
	generateMinimizer( root, st, dimen, maxDepth, offset ) ;
	cellProcContour( this->root, 0, maxDepth ) ;
	dc_printf("Vertices written: %d Quads written: %d \n", offset, actualQuads ) ;
#else
	writeVertex( root, st, dimen, maxDepth, offset, out ) ;
	writeQuad( root, st, dimen, maxDepth, out ) ;
	dc_printf("Vertices written: %d Triangles written: %d \n", offset, actualQuads ) ;
#endif
}

#if 0
void Octree::writePLY( char* fname )
{
	int numQuads = 0 ;
	int numVertices = 0 ;
	int numEdges = 0 ;
#ifdef USE_HERMIT
	countIntersection( root, maxDepth, numQuads, numVertices, numEdges ) ;
#else
	countIntersection( root, maxDepth, numQuads, numVertices ) ;
	numEdges = numQuads * 3 / 2 ;
#endif
	// int euler = numVertices + numQuads - numEdges ;
	// int genus =  ( 2 - euler ) / 2 ;
	// dc_printf("%d vertices %d quads %d edges\n", numVertices, numQuads, numEdges ) ;
	// dc_printf("Genus: %d Euler: %d\n", genus, euler ) ;

	FILE* fout = fopen ( fname, "wb" ) ;
	dc_printf("Vertices counted: %d Polys counted: %d \n", numVertices, numQuads ) ;
	PLYWriter::writeHeader( fout, numVertices, numQuads ) ;
	int offset = 0 ;
	int st[3] = { 0, 0, 0 } ;

	// First, output vertices
	offset = 0 ;
	actualVerts = 0 ;
	actualQuads = 0 ;
#ifdef USE_HERMIT
	generateMinimizer( root, st, dimen, maxDepth, offset, fout ) ;
#ifdef TESTMANIFOLD
	testfout = fopen("test.txt", "w");
	fprintf(testfout, "{");
#endif
	cellProcContour( this->root, 0, maxDepth, fout ) ;
#ifdef TESTMANIFOLD
	fprintf(testfout, "}");
	fclose( testfout ) ;
#endif
	dc_printf("Vertices written: %d Quads written: %d \n", offset, actualQuads ) ;
#else
	writeVertex( root, st, dimen, maxDepth, offset, fout ) ;
	writeQuad( root, st, dimen, maxDepth, fout ) ;
	dc_printf("Vertices written: %d Triangles written: %d \n", offset, actualQuads ) ;
#endif


	fclose( fout ) ;
}
#endif

void Octree::writeOctree( char* fname ) 
{
	FILE* fout = fopen ( fname, "wb" ) ;

	int sized = ( 1 << maxDepth ) ;
	fwrite( &sized, sizeof( int ), 1, fout ) ;
	writeOctree( fout, root, maxDepth ) ;
	dc_printf("Grid dimension: %d\n", sized ) ;


	fclose( fout ) ;
}
void Octree::writeOctree( FILE* fout, UCHAR* node, int depth )
{
	char type ;
	if ( depth > 0 )
	{
		type = 0 ;
		fwrite( &type, sizeof( char ), 1, fout ) ;

		// Get sign at the center
		char sg = (char) getSign( getChild( node, 0 ), depth - 1, 7 - getChildIndex( node, 0 ) ) ;

		int t = 0 ;
		for ( int i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				writeOctree( fout, getChild( node, t ), depth - 1 ) ;
				t ++ ;
			}
			else
			{
				type = 1 ;
				fwrite( &type, sizeof( char ), 1, fout ) ;
				fwrite( &sg, sizeof( char ), 1, fout ) ;
			}
		}
	}
	else
	{
		type = 2 ;
		fwrite( &type, sizeof( char ), 1, fout ) ;
		fwrite( &(node[2]), sizeof ( UCHAR ), 1, fout );
	}
}

#ifdef USE_HERMIT
#if 0
void Octree::writeOctreeGeom( char* fname ) 
{
	FILE* fout = fopen ( fname, "wb" ) ;

	// Write header
	char header[]="SOG.Format 1.0";
	int nlen = 128 - 4 * 4 - strlen(header) - 1 ;
	char* header2 = new char[ nlen ];
	for ( int i = 0 ; i < nlen ; i ++ )
	{
		header2[i] = '\0';
	}
	fwrite( header, sizeof( char ), strlen(header) + 1, fout ) ;
	fwrite( origin, sizeof( float ), 3, fout ) ;
	fwrite( &range, sizeof( float ), 1, fout ) ;
	fwrite( header2, sizeof( char ), nlen, fout ) ;

	
	int sized = ( 1 << maxDepth ) ;
	int st[3] = {0,0,0};
	fwrite( &sized, sizeof( int ), 1, fout ) ;

	writeOctreeGeom( fout, root, st, dimen, maxDepth ) ;
	dc_printf("Grid dimension: %d\n", sized ) ;


	fclose( fout ) ;
}
#endif
void Octree::writeOctreeGeom( FILE* fout, UCHAR* node, int st[3], int len, int depth ) 
{
	char type ;
	if ( depth > 0 )
	{
		type = 0 ;
		fwrite( &type, sizeof( char ), 1, fout ) ;

		// Get sign at the center
		char sg = (char) getSign( getChild( node, 0 ), depth - 1, 7 - getChildIndex( node, 0 ) ) ;

		int t = 0 ;
		len >>= 1 ;
		for ( int i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				int nst[3] ;
				nst[0] = st[0] + vertmap[i][0] * len ;
				nst[1] = st[1] + vertmap[i][1] * len ;
				nst[2] = st[2] + vertmap[i][2] * len ;
				writeOctreeGeom( fout, getChild( node, t ), nst, len, depth - 1 ) ;
				t ++ ;
			}
			else
			{
				type = 1 ;
				fwrite( &type, sizeof( char ), 1, fout ) ;
				fwrite( &sg, sizeof( char ), 1, fout ) ;
			}
		}
	}
	else
	{
		type = 2 ;
		fwrite( &type, sizeof( char ), 1, fout ) ;
		fwrite( &(node[2]), sizeof ( UCHAR ), 1, fout );

		// Compute minimizer
		// First, find minimizer
		float rvalue[3] ;
		rvalue[0] = (float) st[0] + len / 2 ;
		rvalue[1] = (float) st[1] + len / 2 ;
		rvalue[2] = (float) st[2] + len / 2 ;
		computeMinimizer( node, st, len, rvalue ) ;

		// Update
		// float flen = len * range / dimen ; 
		for ( int j = 0 ; j < 3 ; j ++ )
		{
			rvalue[ j ] = rvalue[ j ] * range / dimen + origin[ j ] ;
		}

		fwrite( rvalue, sizeof ( float ), 3, fout );
	}
}
#endif

#ifdef USE_HERMIT
void Octree::writeDCF( char* fname )
{
	FILE* fout = fopen ( fname, "wb" ) ;

	// Writing out version
	char version[10] = "multisign";
	fwrite ( &version, sizeof ( char ), 10, fout );

	// Writing out size
	int sized = ( 1 << maxDepth ) ;
	fwrite( &sized, sizeof( int ), 1, fout ) ;
	fwrite( &sized, sizeof( int ), 1, fout ) ;
	fwrite( &sized, sizeof( int ), 1, fout ) ;

	int st[3] = {0, 0, 0} ;
	writeDCF( fout, root, maxDepth, st, dimen ) ;
	
	dc_printf("Grid dimension: %d\n", sized ) ;
	fclose( fout ) ;
}

void Octree::writeDCF( FILE* fout, UCHAR* node, int height, int st[3], int len )
{
	nodetype type ;
	if ( height > 0 )
	{
		type = 0 ;
		len >>= 1 ;
		fwrite( &type, sizeof( nodetype ), 1, fout ) ;

		// Get sign at the center
		signtype sg = 1 - (signtype) getSign( getChild( node, 0 ), height - 1, 7 - getChildIndex( node, 0 ) ) ;

		int t = 0 ;
		for ( int i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				int nst[3] ;
				nst[0] = st[0] + vertmap[i][0] * len ;
				nst[1] = st[1] + vertmap[i][1] * len ;
				nst[2] = st[2] + vertmap[i][2] * len ;


				writeDCF( fout, getChild( node, t ), height - 1, nst, len ) ;
				t ++ ;
			}
			else
			{
				type = 1 ;
				fwrite( &type, sizeof( nodetype ), 1, fout ) ;
				fwrite ( &(sg), sizeof ( signtype ), 1, fout );
			}
		}
	}
	else
	{
		type = 2 ;
		fwrite( &type, sizeof( nodetype ), 1, fout ) ;

		// Write signs
		signtype sgn[8] ;
		for ( int i = 0 ; i < 8 ; i ++ )
		{
			sgn[ i ] = 1 - (signtype) getSign( node, i ) ;
		}
		fwrite (sgn, sizeof (signtype), 8, fout );

		// Write edge data
		float pts[12], norms[12][3] ;
		int parity[12] ;
		fillEdgeOffsetsNormals( node, st, len, pts, norms, parity ) ;

		numtype zero = 0, one = 1 ;
		for ( int i = 0 ; i < 12 ; i ++ )
		{
			int par = getEdgeParity( node, i ) ;
			// Let's check first
			if ( par )
			{
				if ( sgn[ edgemap[i][0] ] == sgn[ edgemap[i][1] ] )
				{
					dc_printf("Wrong! Parity: %d Sign: %d %d\n", parity[i], sgn[ edgemap[i][0] ], sgn[ edgemap[i][1] ]);
					exit(0) ;
				}
				if ( parity[ i ] == 0 )
				{
					dc_printf("Wrong! No intersection found.\n");
					exit(0) ;
				}
				fwrite( &one, sizeof ( numtype ) , 1, fout ) ;
				fwrite( &(pts[i]), sizeof( float ), 1, fout ) ;
				fwrite( norms[i], sizeof( float ), 3, fout ) ;

			}
			else
			{
				if ( sgn[ edgemap[i][0] ] != sgn[ edgemap[i][1] ] )
				{
					dc_printf("Wrong! Parity: %d Sign: %d %d\n", parity[i], sgn[ edgemap[i][0] ], sgn[ edgemap[i][1] ]);
					exit(0) ;
				}
				fwrite ( &zero, sizeof ( numtype ) , 1, fout );
			}
		}
	}
}
#endif


void Octree::writeOpenEdges( FILE* fout )
{
	// Total number of rings
	fprintf( fout, "%d\n", numRings ) ;
	dc_printf("Number of rings to write: %d\n", numRings) ;

	// Write each ring
	PathList* tlist = ringList ;
	for ( int i = 0 ; i < numRings ; i ++ )
	{
		fprintf(fout, "%d\n", tlist->length) ;
		// dc_printf("Ring length: %d\n", tlist->length ) ;
		PathElement* cur = tlist->head ;
		for ( int j = 0 ; j < tlist->length ; j ++ )
		{
			float cent[3] ;
			float flen = mindimen * range / dimen ; 
			for ( int k = 0 ; k < 3 ; k ++ )
			{
				cent[ k ] = cur->pos[ k ] * range / dimen + origin[ k ] + flen / 2 ;
			}
			fprintf(fout, "%f %f %f\n", cent[0], cent[1], cent[2]) ;
			cur = cur->next ;
		}

		tlist = tlist->next ;
	}
}

#ifndef USE_HERMIT
void Octree::countIntersection( UCHAR* node, int height, int& nquad, int& nvert )
{
	if ( height > 0 )
	{
		int total = getNumChildren( node ) ;
		for ( int i = 0 ; i < total ; i ++ )
		{
			countIntersection( getChild( node, i ), height - 1, nquad, nvert ) ;
		}
	}
	else
	{
		int mask = getSignMask( node ) ;
		nvert += getNumEdges2( node ) ;
		nquad += cubes->getNumTriangle( mask ) ;

	}
}

void Octree::writeVertex( UCHAR* node, int st[3], int len, int height, int& offset, FILE* fout )
{
	int i ;

	if ( height == 0 )
	{
		// Leaf cell, generate
		int emap[] = { 0, 4, 8 } ;
		for ( int i = 0 ; i < 3 ; i ++ )
		{
			if ( getEdgeParity( node, emap[i] ) )
			{
				// Get intersection location
				int count = getEdgeCount( node, i ) ;
				float off = getEdgeOffset( node, count ) ;

				float rvalue[3] ;
				rvalue[0] = (float) st[0] ;
				rvalue[1] = (float) st[1] ;
				rvalue[2] = (float) st[2] ;
				rvalue[i] += off * mindimen ;

				// Update
				float fnst[3] ;
				float flen = len * range / dimen ; 
				for ( int j = 0 ; j < 3 ; j ++ )
				{
					rvalue[ j ] = rvalue[ j ] * range / dimen + origin[ j ] ;
					fnst[ j ] = st[ j ] * range / dimen + origin[ j ] ;
				}

				if ( this->outType == 0 )
				{
					fprintf( fout, "%f %f %f\n", rvalue[0], rvalue[1], rvalue[2] ) ;
				}
				else if ( this->outType == 1 )
				{
					PLYWriter::writeVertex( fout, rvalue ) ;
				}
				
				// Store the index
				setEdgeIntersectionIndex( node, count, offset ) ;
				offset ++ ;
			}
		}

	}
	else
	{
		// Internal cell, recur
		int count = 0 ;
		len >>= 1 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				int nst[3] ;
				nst[0] = st[0] + vertmap[i][0] * len ;
				nst[1] = st[1] + vertmap[i][1] * len ;
				nst[2] = st[2] + vertmap[i][2] * len ;
				
				writeVertex( getChild( node, count ), nst, len, height - 1, offset, fout ) ;
				count ++ ;
			}
		}
	}
}

void Octree::writeQuad( UCHAR* node, int st[3], int len, int height, FILE* fout )
{
	int i ;
	if ( height == 0 )
	{
		int mask = getSignMask( node ) ;
		int num = cubes->getNumTriangle( mask ) ;
		int indices[12] ;
		fillEdgeIntersectionIndices( node, st, len, indices ) ;
		int einds[3], ind[3] ;

		//int flag1 = 0 ;
		//int flag2 = 0 ;
		for ( i = 0 ; i < num ; i ++ )
		{
			int color = 0 ;
			cubes->getTriangle( mask, i, einds ) ;
			// dc_printf("(%d %d %d) ", einds[0], einds[1], einds[2] ) ;
			
			for ( int j = 0 ; j < 3 ; j ++ )
			{
				ind[j] = indices[ einds[j] ] ;
				/*
				if ( ind[j] == 78381 )
				{
					flag1 = 1 ;
				}
				if ( ind[j] == 78384 )
				{
					flag2 = 1 ;
				}
				*/
			}

			if ( this->outType == 0 )
			{
				// OFF
				int numpoly = ( color ? -3 : 3 ) ;
				fprintf(fout, "%d %d %d %d\n", numpoly, ind[0], ind[1], ind[2] ) ;
				actualQuads ++ ;
			}
			else if ( this->outType == 1 )
			{
				// PLY
				PLYWriter::writeFace( fout, 3, ind ) ;
				actualQuads ++ ;
			}
		}

		/*
		if (flag1 && flag2)
		{
			dc_printf("%d\n", mask);
			cubes->printTriangles( mask ) ;
		}
		*/
	}
	else
	{
		// Internal cell, recur
		int count = 0 ;
		len >>= 1 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				int nst[3] ;
				nst[0] = st[0] + vertmap[i][0] * len ;
				nst[1] = st[1] + vertmap[i][1] * len ;
				nst[2] = st[2] + vertmap[i][2] * len ;
				
				writeQuad( getChild( node, count ), nst, len, height - 1, fout ) ;
				count ++ ;
			}
		}
	}
}

#endif


#ifdef USE_HERMIT
void Octree::countIntersection( UCHAR* node, int height, int& nedge, int& ncell, int& nface )
{
	if ( height > 0 )
	{
		int total = getNumChildren( node ) ;
		for ( int i = 0 ; i < total ; i ++ )
		{
			countIntersection( getChild( node, i ), height - 1, nedge, ncell, nface ) ;
		}
	}
	else
	{
		nedge += getNumEdges2( node ) ;

		int smask = getSignMask( node ) ;
		
		if(use_manifold)
		{
			int comps = manifold_table[ smask ].comps ;
			ncell += comps ;
		}
		else {
			if ( smask > 0 && smask < 255 )
			{
				ncell ++ ;
			}
		}
		
		for ( int i = 0 ; i < 3 ; i ++ )
		{
			if ( getFaceEdgeNum( node, i * 2 ) )
			{
				nface ++ ;
			}
		}
	}
}

/* from http://eigen.tuxfamily.org/bz/show_bug.cgi?id=257 */
template<typename _Matrix_Type_>
void pseudoInverse(const _Matrix_Type_ &a,
				   _Matrix_Type_ &result,
				   double epsilon = std::numeric_limits<typename _Matrix_Type_::Scalar>::epsilon())
{
	Eigen::JacobiSVD< _Matrix_Type_ > svd = a.jacobiSvd(Eigen::ComputeFullU |
														Eigen::ComputeFullV);

	typename _Matrix_Type_::Scalar tolerance = epsilon * std::max(a.cols(),
																  a.rows()) *
		svd.singularValues().array().abs().maxCoeff();

	result = svd.matrixV() *
		_Matrix_Type_((svd.singularValues().array().abs() >
					   tolerance).select(svd.singularValues().
										 array().inverse(), 0)).asDiagonal() *
		svd.matrixU().adjoint();
}

void solve_least_squares(const float halfA[], const float b[],
						 const float midpoint[], float rvalue[])
{
	/* calculate pseudo-inverse */
	Eigen::MatrixXf A(3, 3), pinv(3, 3);
	A << halfA[0], halfA[1], halfA[2],
		 halfA[1], halfA[3], halfA[4],
		 halfA[2], halfA[4], halfA[5];
	pseudoInverse(A, pinv);

	Eigen::Vector3f b2(b), mp(midpoint), result;
	b2 = b2 + A * -mp;
	result = pinv * b2 + mp;

	for(int i = 0; i < 3; i++)
		rvalue[i] = result(i);
}

void minimize(float rvalue[3], float mp[3], const float pts[12][3],
			  const float norms[12][3], const int parity[12])
{
	float ata[6] = { 0, 0, 0, 0, 0, 0 };
	float atb[3] = { 0, 0, 0 } ;
	int ec = 0 ;
	
	for ( int i = 0 ; i < 12 ; i ++ )
	{
		// if ( getEdgeParity( leaf, i) )
		if ( parity[ i ] )
		{
			const float* norm = norms[i] ;
			const float* p = pts[i] ;

			// QEF
			ata[ 0 ] += (float) ( norm[ 0 ] * norm[ 0 ] );
			ata[ 1 ] += (float) ( norm[ 0 ] * norm[ 1 ] );
			ata[ 2 ] += (float) ( norm[ 0 ] * norm[ 2 ] );
			ata[ 3 ] += (float) ( norm[ 1 ] * norm[ 1 ] );
			ata[ 4 ] += (float) ( norm[ 1 ] * norm[ 2 ] );
			ata[ 5 ] += (float) ( norm[ 2 ] * norm[ 2 ] );
			
			double pn = p[0] * norm[0] + p[1] * norm[1] + p[2] * norm[2] ;
			
			atb[ 0 ] += (float) ( norm[ 0 ] * pn ) ;
			atb[ 1 ] += (float) ( norm[ 1 ] * pn ) ;
			atb[ 2 ] += (float) ( norm[ 2 ] * pn ) ;

			// Minimizer
			mp[0] += p[0] ;
			mp[1] += p[1] ;
			mp[2] += p[2] ;
			
			ec ++ ;
		}
	}

	if ( ec == 0 )
	{
		return ;
	}
	mp[0] /= ec ;
	mp[1] /= ec ;
	mp[2] /= ec ;
	
	// Solve least squares
	solve_least_squares(ata, atb, mp, rvalue);
}

void Octree::computeMinimizer( UCHAR* leaf, int st[3], int len, float rvalue[3] )
{
	// First, gather all edge intersections
	float pts[12][3], norms[12][3] ;
	// fillEdgeIntersections( leaf, st, len, pts, norms ) ;
	int parity[12] ;
	fillEdgeIntersections( leaf, st, len, pts, norms, parity ) ;

	// Next, construct QEF and minimizer
	float mp[3] = {0, 0, 0};
	minimize(rvalue, mp, pts, norms, parity);
	
	/* Restraining the location of the minimizer */
	float nh1 = hermite_num * len ;
	float nh2 = ( 1 + hermite_num ) * len ;
	if((mode == DUALCON_MASS_POINT || mode == DUALCON_CENTROID) ||
		( rvalue[0] < st[0] - nh1 || rvalue[1] < st[1] - nh1 || rvalue[2] < st[2] - nh1 ||
		  rvalue[0] > st[0] + nh2 || rvalue[1] > st[1] + nh2 || rvalue[2] > st[2] + nh2 ))
	{
		if(mode == DUALCON_CENTROID) {
			// Use centroids
			rvalue[0] = (float) st[0] + len / 2 ;
			rvalue[1] = (float) st[1] + len / 2 ;
			rvalue[2] = (float) st[2] + len / 2 ;
		}
		else {
			// Use mass point instead
			rvalue[0] = mp[0] ;
			rvalue[1] = mp[1] ;
			rvalue[2] = mp[2] ;
		}
	}
}

void Octree::generateMinimizer( UCHAR* node, int st[3], int len, int height, int& offset )
{
	int i, j ;

	if ( height == 0 )
	{
		// Leaf cell, generate

		// First, find minimizer
		float rvalue[3] ;
		rvalue[0] = (float) st[0] + len / 2 ;
		rvalue[1] = (float) st[1] + len / 2 ;
		rvalue[2] = (float) st[2] + len / 2 ;
		computeMinimizer( node, st, len, rvalue ) ;

		// Update
		//float fnst[3] ;
		for ( j = 0 ; j < 3 ; j ++ )
		{
			rvalue[ j ] = rvalue[ j ] * range / dimen + origin[ j ] ;
			//fnst[ j ] = st[ j ] * range / dimen + origin[ j ] ;
		}

		int mult = 0, smask = getSignMask( node ) ;
		
		if(use_manifold)
		{
			mult = manifold_table[ smask ].comps ;
		}
		else
		{
			if ( smask > 0 && smask < 255 )
			{
				mult = 1 ;
			}
		}

		for ( j = 0 ; j < mult ; j ++ )
		{
			add_vert(output_mesh, rvalue);
		}
		
		// Store the index
		setMinimizerIndex( node, offset ) ;

		offset += mult ;
	}
	else
	{
		// Internal cell, recur
		int count = 0 ;
		len >>= 1 ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			if ( hasChild( node, i ) )
			{
				int nst[3] ;
				nst[0] = st[0] + vertmap[i][0] * len ;
				nst[1] = st[1] + vertmap[i][1] * len ;
				nst[2] = st[2] + vertmap[i][2] * len ;
				
				generateMinimizer( getChild( node, count ), nst, len, height - 1, offset ) ;
				count ++ ;
			}
		}
	}
}

void Octree::processEdgeWrite( UCHAR* node[4], int depth[4], int maxdep, int dir )
{
	//int color = 0 ;

	int i = 3 ;
	{
		if ( getEdgeParity( node[i], processEdgeMask[dir][i] ) )
		{
			int flip = 0 ;
			int edgeind = processEdgeMask[dir][i] ;
			if ( getSign( node[i], edgemap[ edgeind ][ 1 ] ) > 0 )
			{
				flip = 1 ;
			}
			
			int num = 0 ;
			{
				int ind[8];
				if(use_manifold)
				{
					/* Deprecated
					   int ind[4] = {
					   getMinimizerIndex( node[0], processEdgeMask[dir][0] ),
					   getMinimizerIndex( node[1], processEdgeMask[dir][1] ),
					   getMinimizerIndex( node[3], processEdgeMask[dir][3] ),
					   getMinimizerIndex( node[2], processEdgeMask[dir][2] )
					   } ;
					   num = 4 ;
					*/
					int vind[2] ;
					int seq[4] = {0,1,3,2};
					for ( int k = 0 ; k < 4 ; k ++ )
					{
						getMinimizerIndices( node[seq[k]], processEdgeMask[dir][seq[k]], vind ) ;
						ind[num] = vind[0] ; 
						num ++ ;
					
						if ( vind[1] != -1 )
						{
							ind[num] = vind[1] ; 
							num ++ ;
							if ( flip == 0 )
							{
								ind[num-1] = vind[0] ;
								ind[num-2] = vind[1] ;
							}
						}
					}
#ifdef TESTMANIFOLD						
					if ( num != 4 )
					{
						dc_printf("Polygon: %d\n", num);
					}
					for ( k = 0 ; k < num ; k ++ )
					{
						fprintf(testfout, "{%d,%d},", ind[k], ind[(k+1)%num] );
					}
#endif

					/* we don't use the manifold option, but if it is
					   ever enabled again note that it can output
					   non-quads */
				}
				else {
					if(flip) {
						ind[0] = getMinimizerIndex( node[2] );
						ind[1] = getMinimizerIndex( node[3] );
						ind[2] = getMinimizerIndex( node[1] );
						ind[3] = getMinimizerIndex( node[0] );
					}
					else {
						ind[0] = getMinimizerIndex( node[0] );
						ind[1] = getMinimizerIndex( node[1] );
						ind[2] = getMinimizerIndex( node[3] );
						ind[3] = getMinimizerIndex( node[2] );
					}
					
					add_quad(output_mesh, ind);
				}
				/*
				if ( this->outType == 0 )
				{
					// OFF

					num = ( color ? -num : num ) ;

					fprintf(fout, "%d ", num);

					if ( flip )
					{
						for ( int k = num - 1 ; k >= 0 ; k -- )
						{
							fprintf(fout, "%d ", ind[k] ) ;
						}
					}
					else
					{
						for ( int k = 0 ; k < num ; k ++ )
						{
							fprintf(fout, "%d ", ind[k] ) ;
						}
					}

					fprintf( fout, "\n") ;

					actualQuads ++ ;
				}
				else if ( this->outType == 1 )
				{
					// PLY

					if ( flip )
					{
						int tind[8];
						for ( int k = num - 1 ; k >= 0 ; k -- )
						{
							tind[k] = ind[num-1-k] ;
						}
						// PLYWriter::writeFace( fout, num, tind ) ;
					}
					else
					{
							// PLYWriter::writeFace( fout, num, ind ) ;
					}

					actualQuads ++ ;
					}*/
			}
			return ;
		}
		else
		{
			return ;
		}
	}
}


void Octree::edgeProcContour( UCHAR* node[4], int leaf[4], int depth[4], int maxdep, int dir )
{
	if ( ! ( node[0] && node[1] && node[2] && node[3] ) )
	{
		return ;
	}
	if ( leaf[0] && leaf[1] && leaf[2] && leaf[3] )
	{
		processEdgeWrite( node, depth, maxdep, dir ) ;
	}
	else
	{
		int i, j ;
		UCHAR* chd[ 4 ][ 8 ] ;
		for ( j = 0 ; j < 4 ; j ++ )
		{
			for ( i = 0 ; i < 8 ; i ++ )
			{
				chd[ j ][ i ] = ((!leaf[j]) && hasChild( node[j], i ) )? getChild( node[j], getChildCount( node[j], i ) ) : NULL ;
			}
		}

		// 2 edge calls
		UCHAR* ne[4] ;
		int le[4] ;
		int de[4] ;
		for ( i = 0 ; i < 2 ; i ++ )
		{
			int c[ 4 ] = { edgeProcEdgeMask[ dir ][ i ][ 0 ], 
						   edgeProcEdgeMask[ dir ][ i ][ 1 ], 
						   edgeProcEdgeMask[ dir ][ i ][ 2 ], 
						   edgeProcEdgeMask[ dir ][ i ][ 3 ] } ;

			for ( int j = 0 ; j < 4 ; j ++ )
			{
				if ( leaf[j] )
				{
					le[j] = leaf[j] ;
					ne[j] = node[j] ;
					de[j] = depth[j] ;
				}
				else
				{
					le[j] = isLeaf( node[j], c[j] ) ;
					ne[j] = chd[ j ][ c[j] ] ;
					de[j] = depth[j] - 1 ;
				}
			}

			edgeProcContour( ne, le, de, maxdep - 1, edgeProcEdgeMask[ dir ][ i ][ 4 ] ) ;
		}

	}
}

void Octree::faceProcContour( UCHAR* node[2], int leaf[2], int depth[2], int maxdep, int dir )
{
	if ( ! ( node[0] && node[1] ) )
	{
		return ;
	}

	if ( ! ( leaf[0] && leaf[1] ) )
	{
		int i, j ;
		// Fill children nodes
		UCHAR* chd[ 2 ][ 8 ] ;
		for ( j = 0 ; j < 2 ; j ++ )
		{
			for ( i = 0 ; i < 8 ; i ++ )
			{
				chd[ j ][ i ] = ((!leaf[j]) && hasChild( node[j], i )) ? getChild( node[j], getChildCount( node[j], i ) ) : NULL ;
			}
		}

		// 4 face calls
		UCHAR* nf[2] ;
		int df[2] ;
		int lf[2] ;
		for ( i = 0 ; i < 4 ; i ++ )
		{
			int c[2] = { faceProcFaceMask[ dir ][ i ][ 0 ], faceProcFaceMask[ dir ][ i ][ 1 ] };
			for ( int j = 0 ; j < 2 ; j ++ )
			{
				if ( leaf[j] )
				{
					lf[j] = leaf[j] ;
					nf[j] = node[j] ;
					df[j] = depth[j] ;
				}
				else
				{
					lf[j] = isLeaf( node[j], c[j] ) ;
					nf[j] = chd[ j ][ c[j] ] ;
					df[j] = depth[j] - 1 ;
				}
			}
			faceProcContour( nf, lf, df, maxdep - 1, faceProcFaceMask[ dir ][ i ][ 2 ] ) ;
		}

		// 4 edge calls
		int orders[2][4] = {{ 0, 0, 1, 1 }, { 0, 1, 0, 1 }} ;
		UCHAR* ne[4] ;
		int le[4] ;
		int de[4] ;
			
		for ( i = 0 ; i < 4 ; i ++ )
		{
			int c[4] = { faceProcEdgeMask[ dir ][ i ][ 1 ], faceProcEdgeMask[ dir ][ i ][ 2 ],
						 faceProcEdgeMask[ dir ][ i ][ 3 ], faceProcEdgeMask[ dir ][ i ][ 4 ] };
			int* order = orders[ faceProcEdgeMask[ dir ][ i ][ 0 ] ] ;

			for ( int j = 0 ; j < 4 ; j ++ )
			{
				if ( leaf[order[j]] )
				{
					le[j] = leaf[order[j]] ;
					ne[j] = node[order[j]] ;
					de[j] = depth[order[j]] ;
				}
				else
				{
					le[j] = isLeaf( node[order[j]], c[j] ) ;
					ne[j] = chd[ order[ j ] ][ c[j] ] ;
					de[j] = depth[order[j]] - 1 ;
				}
			}

			edgeProcContour( ne, le, de, maxdep - 1, faceProcEdgeMask[ dir ][ i ][ 5 ] ) ;
		}
	}
}


void Octree::cellProcContour( UCHAR* node, int leaf, int depth )
{
	if ( node == NULL )
	{
		return ;
	}

	if ( ! leaf )
	{
		int i ;

		// Fill children nodes
		UCHAR* chd[ 8 ] ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			chd[ i ] = ((!leaf) && hasChild( node, i )) ? getChild( node, getChildCount( node, i ) ) : NULL ;
		}

		// 8 Cell calls
		for ( i = 0 ; i < 8 ; i ++ )
		{
			cellProcContour( chd[ i ], isLeaf( node, i ), depth - 1 ) ;
		}

		// 12 face calls
		UCHAR* nf[2] ;
		int lf[2] ;
		int df[2] = { depth - 1, depth - 1 } ;
		for ( i = 0 ; i < 12 ; i ++ )
		{
			int c[ 2 ] = { cellProcFaceMask[ i ][ 0 ], cellProcFaceMask[ i ][ 1 ] };

			lf[0] = isLeaf( node, c[0] ) ;
			lf[1] = isLeaf( node, c[1] ) ;

			nf[0] = chd[ c[0] ] ;
			nf[1] = chd[ c[1] ] ;

			faceProcContour( nf, lf, df, depth - 1, cellProcFaceMask[ i ][ 2 ] ) ;
		}

		// 6 edge calls
		UCHAR* ne[4] ;
		int le[4] ;
		int de[4] = { depth - 1, depth - 1, depth - 1, depth - 1 } ;
		for ( i = 0 ; i < 6 ; i ++ )
		{
			int c[ 4 ] = { cellProcEdgeMask[ i ][ 0 ], cellProcEdgeMask[ i ][ 1 ], cellProcEdgeMask[ i ][ 2 ], cellProcEdgeMask[ i ][ 3 ] };

			for ( int j = 0 ; j < 4 ; j ++ )
			{
				le[j] = isLeaf( node, c[j] ) ;
				ne[j] = chd[ c[j] ] ;
			}

			edgeProcContour( ne, le, de, depth - 1, cellProcEdgeMask[ i ][ 4 ] ) ;
		}
	}
	
}

#endif



void Octree::processEdgeParity( UCHAR* node[4], int depth[4], int maxdep, int dir )
{
	int con = 0 ;
	for ( int i = 0 ; i < 4 ; i ++ )
	{
		// Minimal cell
		// if ( op == 0 )
		{
			if ( getEdgeParity( node[i], processEdgeMask[dir][i] ) )
			{
				con = 1 ;
				break ;
			}
		}
	}

	if ( con == 1 )
	{
		for ( int i = 0 ; i < 4 ; i ++ )
		{
			setEdge( node[ i ], processEdgeMask[dir][i] ) ;
		}
	}
	
}

void Octree::edgeProcParity( UCHAR* node[4], int leaf[4], int depth[4], int maxdep, int dir )
{
	if ( ! ( node[0] && node[1] && node[2] && node[3] ) )
	{
		return ;
	}
	if ( leaf[0] && leaf[1] && leaf[2] && leaf[3] )
	{
		processEdgeParity( node, depth, maxdep, dir ) ;
	}
	else
	{
		int i, j ;
		UCHAR* chd[ 4 ][ 8 ] ;
		for ( j = 0 ; j < 4 ; j ++ )
		{
			for ( i = 0 ; i < 8 ; i ++ )
			{
				chd[ j ][ i ] = ((!leaf[j]) && hasChild( node[j], i ) )? getChild( node[j], getChildCount( node[j], i ) ) : NULL ;
			}
		}

		// 2 edge calls
		UCHAR* ne[4] ;
		int le[4] ;
		int de[4] ;
		for ( i = 0 ; i < 2 ; i ++ )
		{
			int c[ 4 ] = { edgeProcEdgeMask[ dir ][ i ][ 0 ], 
						   edgeProcEdgeMask[ dir ][ i ][ 1 ], 
						   edgeProcEdgeMask[ dir ][ i ][ 2 ], 
						   edgeProcEdgeMask[ dir ][ i ][ 3 ] } ;

			// int allleaf = 1 ;
			for ( int j = 0 ; j < 4 ; j ++ )
			{

				if ( leaf[j] )
				{
					le[j] = leaf[j] ;
					ne[j] = node[j] ;
					de[j] = depth[j] ;
				}
				else
				{
					le[j] = isLeaf( node[j], c[j] ) ;
					ne[j] = chd[ j ][ c[j] ] ;
					de[j] = depth[j] - 1 ;

				}

			}

			edgeProcParity( ne, le, de, maxdep - 1, edgeProcEdgeMask[ dir ][ i ][ 4 ] ) ;
		}

	}
}

void Octree::faceProcParity( UCHAR* node[2], int leaf[2], int depth[2], int maxdep, int dir )
{
	if ( ! ( node[0] && node[1] ) )
	{
		return ;
	}

	if ( ! ( leaf[0] && leaf[1] ) )
	{
		int i, j ;
		// Fill children nodes
		UCHAR* chd[ 2 ][ 8 ] ;
		for ( j = 0 ; j < 2 ; j ++ )
		{
			for ( i = 0 ; i < 8 ; i ++ )
			{
				chd[ j ][ i ] = ((!leaf[j]) && hasChild( node[j], i )) ? getChild( node[j], getChildCount( node[j], i ) ) : NULL ;
			}
		}

		// 4 face calls
		UCHAR* nf[2] ;
		int df[2] ;
		int lf[2] ;
		for ( i = 0 ; i < 4 ; i ++ )
		{
			int c[2] = { faceProcFaceMask[ dir ][ i ][ 0 ], faceProcFaceMask[ dir ][ i ][ 1 ] };
			for ( int j = 0 ; j < 2 ; j ++ )
			{
				if ( leaf[j] )
				{
					lf[j] = leaf[j] ;
					nf[j] = node[j] ;
					df[j] = depth[j] ;
				}
				else
				{
					lf[j] = isLeaf( node[j], c[j] ) ;
					nf[j] = chd[ j ][ c[j] ] ;
					df[j] = depth[j] - 1 ;
				}
			}
			faceProcParity( nf, lf, df, maxdep - 1, faceProcFaceMask[ dir ][ i ][ 2 ] ) ;
		}

		// 4 edge calls
		int orders[2][4] = {{ 0, 0, 1, 1 }, { 0, 1, 0, 1 }} ;
		UCHAR* ne[4] ;
		int le[4] ;
		int de[4] ;
			
		for ( i = 0 ; i < 4 ; i ++ )
		{
			int c[4] = { faceProcEdgeMask[ dir ][ i ][ 1 ], faceProcEdgeMask[ dir ][ i ][ 2 ],
						 faceProcEdgeMask[ dir ][ i ][ 3 ], faceProcEdgeMask[ dir ][ i ][ 4 ] };
			int* order = orders[ faceProcEdgeMask[ dir ][ i ][ 0 ] ] ;

			for ( int j = 0 ; j < 4 ; j ++ )
			{
				if ( leaf[order[j]] )
				{
					le[j] = leaf[order[j]] ;
					ne[j] = node[order[j]] ;
					de[j] = depth[order[j]] ;
				}
				else
				{
					le[j] = isLeaf( node[order[j]], c[j] ) ;
					ne[j] = chd[ order[ j ] ][ c[j] ] ;
					de[j] = depth[order[j]] - 1 ;
				}
			}

			edgeProcParity( ne, le, de, maxdep - 1, faceProcEdgeMask[ dir ][ i ][ 5 ] ) ;
		}
	}
}


void Octree::cellProcParity( UCHAR* node, int leaf, int depth )
{
	if ( node == NULL )
	{
		return ;
	}

	if ( ! leaf )
	{
		int i ;

		// Fill children nodes
		UCHAR* chd[ 8 ] ;
		for ( i = 0 ; i < 8 ; i ++ )
		{
			chd[ i ] = ((!leaf) && hasChild( node, i )) ? getChild( node, getChildCount( node, i ) ) : NULL ;
		}

		// 8 Cell calls
		for ( i = 0 ; i < 8 ; i ++ )
		{
			cellProcParity( chd[ i ], isLeaf( node, i ), depth - 1 ) ;
		}

		// 12 face calls
		UCHAR* nf[2] ;
		int lf[2] ;
		int df[2] = { depth - 1, depth - 1 } ;
		for ( i = 0 ; i < 12 ; i ++ )
		{
			int c[ 2 ] = { cellProcFaceMask[ i ][ 0 ], cellProcFaceMask[ i ][ 1 ] };

			lf[0] = isLeaf( node, c[0] ) ;
			lf[1] = isLeaf( node, c[1] ) ;

			nf[0] = chd[ c[0] ] ;
			nf[1] = chd[ c[1] ] ;

			faceProcParity( nf, lf, df, depth - 1, cellProcFaceMask[ i ][ 2 ] ) ;
		}

		// 6 edge calls
		UCHAR* ne[4] ;
		int le[4] ;
		int de[4] = { depth - 1, depth - 1, depth - 1, depth - 1 } ;
		for ( i = 0 ; i < 6 ; i ++ )
		{
			int c[ 4 ] = { cellProcEdgeMask[ i ][ 0 ], cellProcEdgeMask[ i ][ 1 ], cellProcEdgeMask[ i ][ 2 ], cellProcEdgeMask[ i ][ 3 ] };

			for ( int j = 0 ; j < 4 ; j ++ )
			{
				le[j] = isLeaf( node, c[j] ) ;
				ne[j] = chd[ c[j] ] ;
			}

			edgeProcParity( ne, le, de, depth - 1, cellProcEdgeMask[ i ][ 4 ] ) ;
		}
	}
	
}

/* definitions for global arrays */
const int edgemask[3] = {5, 3, 6};

const int faceMap[6][4] = {
	{4, 8, 5, 9},
	{6, 10, 7, 11},
	{0, 8, 1, 10},
	{2, 9, 3, 11},
	{0, 4, 2, 6},
	{1, 5, 3, 7}
};

const int cellProcFaceMask[12][3] = {
	{0, 4, 0},
	{1, 5, 0},
	{2, 6, 0},
	{3, 7, 0},
	{0, 2, 1},
	{4, 6, 1},
	{1, 3, 1},
	{5, 7, 1},
	{0, 1, 2},
	{2, 3, 2},
	{4, 5, 2},
	{6, 7, 2}
};

const int cellProcEdgeMask[6][5] = {
	{0, 1, 2, 3, 0},
	{4, 5, 6, 7, 0},
	{0, 4, 1, 5, 1},
	{2, 6, 3, 7, 1},
	{0, 2, 4, 6, 2},
	{1, 3, 5, 7, 2}
};

const int faceProcFaceMask[3][4][3] = {
	{{4, 0, 0},
	 {5, 1, 0},
	 {6, 2, 0},
	 {7, 3, 0}},
	{{2, 0, 1},
	 {6, 4, 1},
	 {3, 1, 1},
	 {7, 5, 1}},
	{{1, 0, 2},
	 {3, 2, 2},
	 {5, 4, 2},
	 {7, 6, 2}}
};

const int faceProcEdgeMask[3][4][6] = {
	{{1, 4, 0, 5, 1, 1},
	 {1, 6, 2, 7, 3, 1},
	 {0, 4, 6, 0, 2, 2},
	 {0, 5, 7, 1, 3, 2}},
	{{0, 2, 3, 0, 1, 0},
	 {0, 6, 7, 4, 5, 0},
	 {1, 2, 0, 6, 4, 2},
	 {1, 3, 1, 7, 5, 2}},
	{{1, 1, 0, 3, 2, 0},
	 {1, 5, 4, 7, 6, 0},
	 {0, 1, 5, 0, 4, 1},
	 {0, 3, 7, 2, 6, 1}}
};

const int edgeProcEdgeMask[3][2][5] = {
	{{3, 2, 1, 0, 0},
	 {7, 6, 5, 4, 0}},
	{{5, 1, 4, 0, 1},
	 {7, 3, 6, 2, 1}},
	{{6, 4, 2, 0, 2},
	 {7, 5, 3, 1, 2}},
};

const int processEdgeMask[3][4] = {
	{3, 2, 1, 0},
	{7, 5, 6, 4},
	{11, 10, 9, 8}
};

const int dirCell[3][4][3] = {
	{{0, -1, -1},
	 {0, -1, 0},
	 {0, 0, -1},
	 {0, 0, 0}},
	{{-1, 0, -1},
	 {-1, 0, 0},
	 {0, 0, -1},
	 {0, 0, 0}},
	{{-1, -1, 0},
	 {-1, 0, 0},
	 {0, -1, 0},
	 {0, 0, 0}}
};

const int dirEdge[3][4] = {
	{3, 2, 1, 0},
	{7, 6, 5, 4},
	{11, 10, 9, 8}
};
