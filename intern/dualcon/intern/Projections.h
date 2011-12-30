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

#ifndef PROJECTIONS_H
#define PROJECTIONS_H

#include <stdio.h>
#include <stdlib.h>

#define CONTAINS_INDEX
#define GRID_DIMENSION 20

#if defined(_WIN32) && !defined(FREE_WINDOWS)
#define LONG __int64
#else
#define LONG int64_t
#endif
#define UCHAR unsigned char

/**
 * Structures and classes for computing projections of triangles
 * onto separating axes during scan conversion
 *
 * @author Tao Ju
 */


extern const int vertmap[8][3];
extern const int centmap[3][3][3][2];
extern const int edgemap[12][2];
extern const int facemap[6][4];

/**
 * Structure for the projections inheritable from parent
 */
struct InheritableProjections
{
	/// Projections of triangle
	LONG trigProj[13][2] ;

	/// Projections of triangle vertices on primary axes
	LONG trigVertProj[13][3] ;

	/// Projections of triangle edges
	LONG trigEdgeProj[13][3][2] ;

	/// Normal of the triangle
	double norm[3] ;
	double normA, normB ;

	/// End points along each axis
	//int cubeEnds[13][2] ;

	/// Error range on each axis
	/// LONG errorProj[13];

#ifdef CONTAINS_INDEX
	/// Index of polygon
	int index ;
#endif
};


/**
 * Class for projections of cube / triangle vertices on the separating axes
 */
class Projections
{
public:
	/// Inheritable portion
	InheritableProjections* inherit ;

	/// Projections of the cube vertices
	LONG cubeProj[13][6] ;

public:

	Projections( )
	{
	}

	/** 
	 * Construction
	 * from a cube (axes aligned) and triangle
	 */
	Projections( LONG cube[2][3], LONG trig[3][3], LONG error, int triind )
	{
		int i, j ;
		inherit = new InheritableProjections ;
#ifdef CONTAINS_INDEX
		inherit->index = triind ;
#endif
		/// Create axes
		LONG axes[13][3] ;

		// Cube faces
		axes[0][0] = 1 ;
		axes[0][1] = 0 ;
		axes[0][2] = 0 ;

		axes[1][0] = 0 ;
		axes[1][1] = 1 ;
		axes[1][2] = 0 ;

		axes[2][0] = 0 ;
		axes[2][1] = 0 ;
		axes[2][2] = 1 ;

		// Triangle face
		LONG trigedge[3][3] ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			for ( j = 0 ; j < 3 ; j ++ )
			{
				trigedge[i][j] = trig[(i+1)%3][j] - trig[i][j] ;
			}
		}
		crossProduct( trigedge[0], trigedge[1], axes[3] ) ;

		/// Normalize face normal and store
		double dedge1[] = { (double) trig[1][0] - (double) trig[0][0],
							(double) trig[1][1] - (double) trig[0][1],
							(double) trig[1][2] - (double) trig[0][2] } ;
		double dedge2[] = { (double) trig[2][0] - (double) trig[1][0],
							(double) trig[2][1] - (double) trig[1][1],
							(double) trig[2][2] - (double) trig[1][2] } ;
		crossProduct( dedge1, dedge2, inherit->norm ) ;
		normalize( inherit->norm ) ;
//		inherit->normA = norm[ 0 ] ;
//		inherit->normB = norm[ 2 ] > 0 ? norm[ 1 ] : 2 + norm[ 1 ] ;

		// Face edges and triangle edges
		int ct = 4 ;
		for ( i = 0 ; i < 3 ; i ++ )
			for ( j = 0 ; j < 3 ; j ++ )
			{
				crossProduct( axes[j], trigedge[i], axes[ct] ) ;
				ct ++ ;
			}		

		/// Generate projections
		LONG cubeedge[3][3] ;
		for ( i = 0 ; i < 3 ; i ++ )
		{
			for ( j = 0 ; j < 3 ; j ++ )
			{
				cubeedge[i][j] = 0 ;
			}
			cubeedge[i][i] = cube[1][i] - cube[0][i] ;
		}

		for ( j = 0 ; j < 13 ; j ++ )
		{
			// Origin
			cubeProj[j][0] = dotProduct( axes[j], cube[0] ) ;

			// 3 direction vectors
			for ( i = 1 ; i < 4 ; i ++ )
			{
				cubeProj[j][i] = dotProduct( axes[j], cubeedge[i-1] ) ;
			}

			// Offsets of 2 ends of cube projection
			LONG max = 0 ;
			LONG min = 0 ;
			for ( i = 1 ; i < 8 ; i ++ )
			{
				LONG proj = vertmap[i][0] * cubeProj[j][1] + vertmap[i][1] * cubeProj[j][2] + vertmap[i][2] * cubeProj[j][3] ;
				if ( proj > max )
				{
					max = proj ;
				}
				if ( proj < min )
				{
					min = proj ;
				}
			}
			cubeProj[j][4] = min ;
			cubeProj[j][5] = max ;

		}

		for ( j = 0 ; j < 13 ; j ++ )
		{
			LONG vts[3] = { dotProduct( axes[j], trig[0] ),
							dotProduct( axes[j], trig[1] ),
							dotProduct( axes[j], trig[2] )	} ;

			// Vertex
			inherit->trigVertProj[j][0] = vts[0] ;
			inherit->trigVertProj[j][1] = vts[1] ;
			inherit->trigVertProj[j][2] = vts[2] ;

			// Edge
			for ( i = 0 ; i < 3 ; i ++ )
			{
				if ( vts[i] < vts[(i+1) % 3] )
				{
					inherit->trigEdgeProj[j][i][0] = vts[i] ; 
					inherit->trigEdgeProj[j][i][1] = vts[(i+1) % 3] ; 
				}
				else
				{
					inherit->trigEdgeProj[j][i][1] = vts[i] ; 
					inherit->trigEdgeProj[j][i][0] = vts[(i+1) % 3] ; 
				}
			}

			// Triangle
			inherit->trigProj[j][0] = vts[0] ;
			inherit->trigProj[j][1] = vts[0] ;
			for ( i = 1 ; i < 3 ; i ++ )
			{
				if ( vts[i] < inherit->trigProj[j][0] )
				{
					inherit->trigProj[j][0] = vts[i] ;
				}
				if ( vts[i] > inherit->trigProj[j][1] )
				{
					inherit->trigProj[j][1] = vts[i] ;
				}
			}
		}

	}

	/**
	 * Construction
	 * from a parent Projections object and the index of the children
	 */
	Projections ( Projections* parent ) 
	{
		// Copy inheritable projections
		this->inherit = parent->inherit ;

		// Shrink cube projections
		for ( int i = 0 ; i < 13 ; i ++ )
		{
			cubeProj[i][0] = parent->cubeProj[i][0] ;
			for ( int j = 1 ; j < 6 ; j ++ )
			{
				cubeProj[i][j] = parent->cubeProj[i][j] >> 1 ;
			}
		}
	};

	Projections ( Projections* parent, int box[3], int depth ) 
	{
		int mask =  ( 1 << depth ) - 1 ;
		int nbox[3] = { box[0] & mask, box[1] & mask, box[2] & mask } ;

		// Copy inheritable projections
		this->inherit = parent->inherit ;

		// Shrink cube projections
		for ( int i = 0 ; i < 13 ; i ++ )
		{
			for ( int j = 1 ; j < 6 ; j ++ )
			{
				cubeProj[i][j] = parent->cubeProj[i][j] >> depth ;
			}

			cubeProj[i][0] = parent->cubeProj[i][0] + nbox[0] * cubeProj[i][1] + nbox[1] * cubeProj[i][2] + nbox[2] * cubeProj[i][3] ;
		}
	};

	/**
	 * Testing intersection based on vertex/edge masks
	 */
	int getIntersectionMasks( UCHAR cedgemask, UCHAR& edgemask )
	{
		int i, j ;
		edgemask = cedgemask ;

		// Pre-processing
		/*
		if ( cvertmask & 1 )
		{
			edgemask |= 5 ;
		}
		if ( cvertmask & 2 )
		{
			edgemask |= 3 ;
		}
		if ( cvertmask & 4 )
		{
			edgemask |= 6 ;
		}

		*/

		// Test axes for edge intersection
		UCHAR bit = 1 ;
		for ( j = 0 ; j < 3 ; j ++ )
		{
			if ( edgemask & bit )
			{
				for ( i = 0 ; i < 13 ; i ++ )
				{
					LONG proj0 = cubeProj[i][0] + cubeProj[i][4] ;
					LONG proj1 = cubeProj[i][0] + cubeProj[i][5] ;

					if ( proj0 > inherit->trigEdgeProj[i][j][1] ||
						 proj1 < inherit->trigEdgeProj[i][j][0] )
					{
						edgemask &= ( ~ bit ) ;
						break ;
					}
				}
			}
			bit <<= 1 ;
		}
		
		/*
		if ( edgemask != 0 )
		{
			printf("%d %d\n", cedgemask, edgemask) ;
		}
		*/

		// Test axes for triangle intersection
		if ( edgemask )
		{
			return 1 ;
		}

		for ( i = 3 ; i < 13 ; i ++ )
		{
			LONG proj0 = cubeProj[i][0] + cubeProj[i][4] ;
			LONG proj1 = cubeProj[i][0] + cubeProj[i][5] ;

			if ( proj0 > inherit->trigProj[i][1] ||
				 proj1 < inherit->trigProj[i][0] )
			{
				return 0 ;
			}
		}
		
		return 1 ;
	}

	/**
	 * Retrieving children masks using PRIMARY AXES
	 */
	UCHAR getChildrenMasks( UCHAR cvertmask, UCHAR vertmask[8] )
	{
		int i, j, k ;
		int bmask[3][2] = {{0,0},{0,0},{0,0}} ;
		int vmask[3][3][2] = {{{0,0},{0,0},{0,0}},{{0,0},{0,0},{0,0}},{{0,0},{0,0},{0,0}}} ;
		UCHAR boxmask = 0 ;
		LONG len = cubeProj[0][1] >> 1 ;
		
		for ( i = 0 ; i < 3 ; i ++ )
		{
			LONG mid = cubeProj[i][0] + len ;

			// Check bounding box
			if ( mid >= inherit->trigProj[i][0] ) 
			{
				bmask[i][0] = 1 ;
			}
			if ( mid <= inherit->trigProj[i][1] ) 
			{
				bmask[i][1] = 1 ;
			}

			// Check vertex mask
			if ( cvertmask )
			{
				for ( j = 0 ; j < 3 ; j ++ )
				{
					if ( cvertmask & ( 1 << j ) )
					{
						// Only check if it's contained this node
						if ( mid >= inherit->trigVertProj[i][j] ) 
						{
							vmask[i][j][0] = 1 ;
						}
						if ( mid <= inherit->trigVertProj[i][j] ) 
						{
							vmask[i][j][1] = 1 ;
						}
					}
				}
			}

			/*
			// Check edge mask
			if ( cedgemask )
			{
				for ( j = 0 ; j < 3 ; j ++ )
				{
					if ( cedgemask & ( 1 << j ) )
					{
						// Only check if it's contained this node
						if ( mid >= inherit->trigEdgeProj[i][j][0] ) 
						{
							emask[i][j][0] = 1 ;
						}
						if ( mid <= inherit->trigEdgeProj[i][j][1] ) 
						{
							emask[i][j][1] = 1 ;
						}
					}
				}
			}
			*/

		}

		// Fill in masks
		int ct = 0 ;
		for ( i = 0 ; i < 2 ; i ++ )
			for ( j = 0 ; j < 2 ; j ++ )
				for ( k = 0 ; k < 2 ; k ++ )
				{
					boxmask |= ( ( bmask[0][i] & bmask[1][j] & bmask[2][k] ) << ct ) ;
					vertmask[ct] = (( vmask[0][0][i] & vmask[1][0][j] & vmask[2][0][k] ) |
								   (( vmask[0][1][i] & vmask[1][1][j] & vmask[2][1][k] ) << 1 ) |
								   (( vmask[0][2][i] & vmask[1][2][j] & vmask[2][2][k] ) << 2 ) ) ;
					/*
					edgemask[ct] = (( emask[0][0][i] & emask[1][0][j] & emask[2][0][k] ) |
								   (( emask[0][1][i] & emask[1][1][j] & emask[2][1][k] ) << 1 ) |
								   (( emask[0][2][i] & emask[1][2][j] & emask[2][2][k] ) << 2 ) ) ;
					edgemask[ct] = cedgemask ;
					*/
					ct ++ ;
				}

		// Return bounding box masks
		return boxmask ;
	}

	UCHAR getBoxMask( )
	{
		int i, j, k ;
		int bmask[3][2] = {{0,0},{0,0},{0,0}} ;
		UCHAR boxmask = 0 ;
		LONG len = cubeProj[0][1] >> 1 ;
		
		for ( i = 0 ; i < 3 ; i ++ )
		{
			LONG mid = cubeProj[i][0] + len ;

			// Check bounding box
			if ( mid >= inherit->trigProj[i][0] ) 
			{
				bmask[i][0] = 1 ;
			}
			if ( mid <= inherit->trigProj[i][1] ) 
			{
				bmask[i][1] = 1 ;
			}

		}

		// Fill in masks
		int ct = 0 ;
		for ( i = 0 ; i < 2 ; i ++ )
			for ( j = 0 ; j < 2 ; j ++ )
				for ( k = 0 ; k < 2 ; k ++ )
				{
					boxmask |= ( ( bmask[0][i] & bmask[1][j] & bmask[2][k] ) << ct ) ;
					ct ++ ;
				}

		// Return bounding box masks
		return boxmask ;
	}


	/**
	 * Get projections for sub-cubes (simple axes)
	 */
	void getSubProjectionsSimple( Projections* p[8] )
	{
		// Process the axes cooresponding to the triangle's normal
		int ind = 3 ;
		LONG len = cubeProj[ 0 ][ 1 ] >> 1 ;
		LONG trigproj[3] = { cubeProj[ ind ][ 1 ] >> 1, cubeProj[ ind ][ 2 ] >> 1, cubeProj[ ind ][ 3 ] >> 1 } ;

		int ct = 0 ; 
		for ( int i = 0 ; i < 2 ; i ++ )
			for ( int j = 0 ; j < 2 ; j ++ )
				for ( int k = 0 ; k < 2 ; k ++ )
				{
					p[ct] = new Projections( ) ;
					p[ct]->inherit = inherit ;

					p[ct]->cubeProj[ 0 ][ 0 ] = cubeProj[ 0 ][ 0 ] + i * len ;
					p[ct]->cubeProj[ 1 ][ 0 ] = cubeProj[ 1 ][ 0 ] + j * len ;
					p[ct]->cubeProj[ 2 ][ 0 ] = cubeProj[ 2 ][ 0 ] + k * len ;
					p[ct]->cubeProj[ 0 ][ 1 ] = len ;

					for ( int m = 1 ; m < 4 ; m ++ )
					{
						p[ct]->cubeProj[ ind ][ m ] = trigproj[ m - 1 ] ;
					}
					p[ct]->cubeProj[ ind ][ 0 ] = cubeProj[ ind ][0] + i * trigproj[0] + j * trigproj[1] + k * trigproj[2] ;

					ct ++ ;
				}
	}

	/**
	 * Shifting a cube to a new origin
	 */
	void shift ( int off[3] ) 
	{
		for ( int i = 0 ; i < 13 ; i ++ )
		{
			cubeProj[i][0] += off[0] * cubeProj[i][1] + off[1] * cubeProj[i][2] + off[2] * cubeProj[i][3] ;
		}
	}

	void shiftNoPrimary ( int off[3] ) 
	{
		for ( int i = 3 ; i < 13 ; i ++ )
		{
			cubeProj[i][0] += off[0] * cubeProj[i][1] + off[1] * cubeProj[i][2] + off[2] * cubeProj[i][3] ;
		}
	}

	/**
	 * Method to test intersection of the triangle and the cube
	 */
	int isIntersecting ( ) 
	{
		for ( int i = 0 ; i < 13 ; i ++ )
		{
			/*
			LONG proj0 = cubeProj[i][0] + 
				vertmap[inherit->cubeEnds[i][0]][0] * cubeProj[i][1] + 
				vertmap[inherit->cubeEnds[i][0]][1] * cubeProj[i][2] + 
				vertmap[inherit->cubeEnds[i][0]][2] * cubeProj[i][3] ;
			LONG proj1 = cubeProj[i][0] + 
				vertmap[inherit->cubeEnds[i][1]][0] * cubeProj[i][1] + 
				vertmap[inherit->cubeEnds[i][1]][1] * cubeProj[i][2] + 
				vertmap[inherit->cubeEnds[i][1]][2] * cubeProj[i][3] ;
			*/

			LONG proj0 = cubeProj[i][0] + cubeProj[i][4] ;
			LONG proj1 = cubeProj[i][0] + cubeProj[i][5] ;

			if ( proj0 > inherit->trigProj[i][1] ||
				 proj1 < inherit->trigProj[i][0] )
			{
				return 0 ;
			}
		}
		
		return 1 ;
	};

	int isIntersectingNoPrimary ( ) 
	{
		for ( int i = 3 ; i < 13 ; i ++ )
		{
			/*
			LONG proj0 = cubeProj[i][0] + 
				vertmap[inherit->cubeEnds[i][0]][0] * cubeProj[i][1] + 
				vertmap[inherit->cubeEnds[i][0]][1] * cubeProj[i][2] + 
				vertmap[inherit->cubeEnds[i][0]][2] * cubeProj[i][3] ;
			LONG proj1 = cubeProj[i][0] + 
				vertmap[inherit->cubeEnds[i][1]][0] * cubeProj[i][1] + 
				vertmap[inherit->cubeEnds[i][1]][1] * cubeProj[i][2] + 
				vertmap[inherit->cubeEnds[i][1]][2] * cubeProj[i][3] ;
			*/

			LONG proj0 = cubeProj[i][0] + cubeProj[i][4] ;
			LONG proj1 = cubeProj[i][0] + cubeProj[i][5] ;

			if ( proj0 > inherit->trigProj[i][1] ||
				 proj1 < inherit->trigProj[i][0] )
			{
				return 0 ;
			}
		}
		
		return 1 ;
	};	
	
	/**
	 * Method to test intersection of the triangle and one edge
	 */
	int isIntersecting ( int edgeInd ) 
	{
		for ( int i = 0 ; i < 13 ; i ++ )
		{
			
			LONG proj0 = cubeProj[i][0] + 
				vertmap[edgemap[edgeInd][0]][0] * cubeProj[i][1] + 
				vertmap[edgemap[edgeInd][0]][1] * cubeProj[i][2] + 
				vertmap[edgemap[edgeInd][0]][2] * cubeProj[i][3] ;
			LONG proj1 = cubeProj[i][0] + 
				vertmap[edgemap[edgeInd][1]][0] * cubeProj[i][1] + 
				vertmap[edgemap[edgeInd][1]][1] * cubeProj[i][2] + 
				vertmap[edgemap[edgeInd][1]][2] * cubeProj[i][3] ;


			if ( proj0 < proj1 )
			{
				if ( proj0 > inherit->trigProj[i][1] ||
					 proj1 < inherit->trigProj[i][0] )
				{
					return 0 ;
				}
			}
			else
			{
				if ( proj1 > inherit->trigProj[i][1] ||
					 proj0 < inherit->trigProj[i][0] )
				{
					return 0 ;
				}
			}
		}
		
		// printf( "Intersecting: %d %d\n", edgemap[edgeInd][0], edgemap[edgeInd][1] )  ;
		return 1 ;
	};

	/**
	 * Method to test intersection of one triangle edge and one cube face
	 */
	int isIntersecting ( int edgeInd, int faceInd ) 
	{
		for ( int i = 0 ; i < 13 ; i ++ )
		{
			LONG trigproj0 = inherit->trigVertProj[i][edgeInd] ;
			LONG trigproj1 = inherit->trigVertProj[i][(edgeInd+1)%3] ;

			if ( trigproj0 < trigproj1 )
			{
				int t1 = 1 , t2 = 1 ;
				for ( int j = 0 ; j < 4 ; j ++ )
				{
					LONG proj = cubeProj[i][0] + 
						vertmap[facemap[faceInd][j]][0] * cubeProj[i][1] + 
						vertmap[facemap[faceInd][j]][1] * cubeProj[i][2] + 
						vertmap[facemap[faceInd][j]][2] * cubeProj[i][3] ;
					if ( proj >= trigproj0 )
					{
						t1 = 0 ;
					}
					if ( proj <= trigproj1 )
					{
						t2 = 0 ;
					}
				}
				if ( t1 || t2 )
				{
					return 0 ;
				}
			}
			else
			{
				int t1 = 1 , t2 = 1  ;
				for ( int j = 0 ; j < 4 ; j ++ )
				{
					LONG proj = cubeProj[i][0] + 
						vertmap[facemap[faceInd][j]][0] * cubeProj[i][1] + 
						vertmap[facemap[faceInd][j]][1] * cubeProj[i][2] + 
						vertmap[facemap[faceInd][j]][2] * cubeProj[i][3] ;
					if ( proj >= trigproj1 ) 
					{
						t1 = 0 ;
					}
					if ( proj <= trigproj0 )
					{
						t2 = 0 ;
					}
				}
				if ( t1 || t2 )
				{
					return 0 ;
				}
			}
		}

		return 1 ;
	};


	int isIntersectingPrimary ( int edgeInd ) 
	{
		for ( int i = 0 ; i < 13 ; i ++ )
		{
			
			LONG proj0 = cubeProj[i][0] ;
			LONG proj1 = cubeProj[i][0] + cubeProj[i][edgeInd + 1] ;

			if ( proj0 < proj1 )
			{
				if ( proj0 > inherit->trigProj[i][1] ||
					 proj1 < inherit->trigProj[i][0] )
				{
					return 0 ;
				}
			}
			else
			{
				if ( proj1 > inherit->trigProj[i][1] ||
					 proj0 < inherit->trigProj[i][0] )
				{
					return 0 ;
				}
			}

		}
		
		// printf( "Intersecting: %d %d\n", edgemap[edgeInd][0], edgemap[edgeInd][1] )  ;
		return 1 ;
	};

	double getIntersection ( int edgeInd ) 
	{
		int i = 3 ;

		LONG proj0 = cubeProj[i][0] + 
			vertmap[edgemap[edgeInd][0]][0] * cubeProj[i][1] + 
			vertmap[edgemap[edgeInd][0]][1] * cubeProj[i][2] + 
			vertmap[edgemap[edgeInd][0]][2] * cubeProj[i][3] ;
		LONG proj1 = cubeProj[i][0] + 
			vertmap[edgemap[edgeInd][1]][0] * cubeProj[i][1] + 
			vertmap[edgemap[edgeInd][1]][1] * cubeProj[i][2] + 
			vertmap[edgemap[edgeInd][1]][2] * cubeProj[i][3] ;
		LONG proj2 = inherit->trigProj[i][1] ;

		/*
		if ( proj0 < proj1 )
		{
			if ( proj2 < proj0 || proj2 > proj1 )
			{
				return -1 ;
			}
		}
		else
		{
			if ( proj2 < proj1 || proj2 > proj0 )
			{
				return -1 ;
			}
		}
		*/

		double alpha = (double)( proj2 - proj0 ) / (double)( proj1 - proj0 ) ;
		/*
		if ( alpha < 0 )
		{
			alpha = 0.5 ;
		}
		else if ( alpha > 1 )
		{
			alpha = 0.5 ;
		}
		*/

		return alpha ;
	};

	float getIntersectionPrimary ( int edgeInd ) 
	{
		int i = 3 ;

		
		LONG proj0 = cubeProj[i][0] ;
		LONG proj1 = cubeProj[i][0] + cubeProj[i][edgeInd + 1] ;
		LONG proj2 = inherit->trigProj[i][1] ;

		// double alpha = (double)( ( proj2 - proj0 ) * cubeProj[edgeInd][edgeInd + 1] ) / (double)( proj1 - proj0 ) ;
		double alpha = (double)( ( proj2 - proj0 ) ) / (double)( proj1 - proj0 ) ;
		
		if ( alpha < 0 )
		{
			alpha = 0.5 ;
		}
		else if ( alpha > 1 )
		{
			alpha = 0.5 ;
		}
		

		return (float)alpha ;
	};

	/**
	 * Method to perform cross-product
	 */
	void crossProduct ( LONG a[3], LONG b[3], LONG res[3] )
	{
		res[0] = a[1] * b[2] - a[2] * b[1] ;
		res[1] = a[2] * b[0] - a[0] * b[2] ;
		res[2] = a[0] * b[1] - a[1] * b[0] ;
	}
	void crossProduct ( double a[3], double b[3], double res[3] )
	{
		res[0] = a[1] * b[2] - a[2] * b[1] ;
		res[1] = a[2] * b[0] - a[0] * b[2] ;
		res[2] = a[0] * b[1] - a[1] * b[0] ;
	}

	/**
	 * Method to perform dot product
	 */
	LONG dotProduct ( LONG a[3], LONG b[3] )
	{
		return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] ;
	}

	void normalize( double a[3] )
	{
		double mag = a[0] * a[0] + a[1] * a[1] + a[2] * a[2] ;
		if ( mag > 0 )
		{
			mag = sqrt( mag ) ;
			a[0] /= mag ;
			a[1] /= mag ;
			a[2] /= mag ;
		}
	}

};

#endif
